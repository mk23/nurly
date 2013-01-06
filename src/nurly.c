/*****************************************************************************
 * Compile with the following command:
 *
 *     gcc -I../include -std=c99 -shared -fPIC -o nurly.o nurly.c queue.c -lcurl -lrt -pthread
 *
 *****************************************************************************/

#include "nurly.h"

/* specify event broker API version (required) */
NEB_API_VERSION(CURRENT_NEB_API_VERSION)

/* global variables from nagios */
extern int             event_broker_options;

/* global variables for nurly */
nurly_queue_t   nurly_work_q = NURLY_QUEUE_INITIALIZER;
void*           nurly_module = NULL;
pthread_t       nurly_thread[NURLY_THREADS];


/* declare nurly functions */
static int    nurly_process_data(int, void*);
static int    nurly_service_check(int, void*);

       void*  nurly_worker_start(void*);
static void   nurly_worker_purge(void*);


int nebmodule_init(int flags, char* args, nebmodule* handle) {
    nurly_module = handle;

    if (!(event_broker_options & BROKER_PROGRAM_STATE)) {
        nurly_log("need BROKER_PROGRAM_STATE (%i or -1) in event_broker_options enabled to work", BROKER_PROGRAM_STATE);
        return NEB_ERROR;
    }
    if (!(event_broker_options & BROKER_SERVICE_CHECKS)) {
        nurly_log("need BROKER_SERVICE_CHECKS (%i or -1) in event_broker_options enabled to work", BROKER_SERVICE_CHECKS);
        return NEB_ERROR;
    }

    /* module info */
    neb_set_module_info(nurly_module, NEBMODULE_MODINFO_TITLE,     "Nurly");
    neb_set_module_info(nurly_module, NEBMODULE_MODINFO_AUTHOR,    "Max Kalika");
    neb_set_module_info(nurly_module, NEBMODULE_MODINFO_COPYRIGHT, "Copyright (c) 2013 Max Kalika");
    neb_set_module_info(nurly_module, NEBMODULE_MODINFO_VERSION,   NURLY_VERSION);
    neb_set_module_info(nurly_module, NEBMODULE_MODINFO_LICENSE,   "as-is");
    neb_set_module_info(nurly_module, NEBMODULE_MODINFO_DESC,      "distribute host/service checks via libcurl");

    /* register initializer callback */
    neb_register_callback(NEBCALLBACK_PROCESS_DATA, nurly_module, 0, nurly_process_data);

    return NEB_OK;
}

int nebmodule_deinit(int flags, int reason) {
    neb_deregister_callback(NEBCALLBACK_PROCESS_DATA, nurly_module);

    //for (long i = 0; i < NURLY_THREADS; i++) {
    //    pthread_cancel(nurly_thread[i]);
    //    pthread_join(nurly_thread[i], NULL);
    //}

    curl_global_cleanup();

    return NEB_OK;
}

static int nurly_process_data(int event_type, void* data) {
    nebstruct_process_data* process_data;

    process_data = (nebstruct_process_data*)data;
    if (process_data == NULL) {
        nurly_log("error: received invalid process data");
        return NEB_ERROR;
    }

    if (process_data->type == NEBTYPE_PROCESS_EVENTLOOPSTART) {
        curl_global_init(CURL_GLOBAL_DEFAULT);

        for (long i = 0; i < NURLY_THREADS; i++) {
            pthread_create(&nurly_thread[i], NULL, nurly_worker_start, (void*)i);
        }

        neb_register_callback(NEBCALLBACK_SERVICE_CHECK_DATA, nurly_module, 0, nurly_service_check);

        nurly_log("initialization complete, version: %s", NURLY_VERSION);
    }

    return NEB_OK;
}

static int nurly_service_check(int event_type, void* data) {
    char*                         command_line;
    nebstruct_service_check_data* service_data;

    service_data = (nebstruct_service_check_data*)data;
    if (service_data == NULL) {
        nurly_log("error: received invalid service data");
        return NEB_ERROR;
    }

    /* ignore non-initiate service checks */
    if (service_data->type != NEBTYPE_SERVICECHECK_INITIATE) {
        return NEB_OK;
    }

    command_line = malloc(sizeof(char) * (strlen(service_data->command_line) + 1));
    if (!command_line) {
        nurly_log("unable to allocate memory for command line");
        return NEB_ERROR;
    }

    strcpy(command_line, service_data->command_line);

    nurly_log("processing service check command: %s", command_line);
    nurly_queue_put(&nurly_work_q, (void*)command_line);

    return NEBERROR_CALLBACKOVERRIDE;
}

void* nurly_worker_start(void* data) {
    char*          command_line;
    char*          curl_escaped;
    nurly_worker_t worker_data;

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    worker_data.id   = (long)data;
    worker_data.curl = curl_easy_init();

    pthread_cleanup_push(nurly_worker_purge, (void*)&worker_data);
    while (1) {
        command_line = (char*)nurly_queue_get(&nurly_work_q);
        if (command_line) {
            curl_escaped = curl_easy_escape(worker_data.curl, command_line, 0);
            nurly_log("got work item in worker %02i: %s", worker_data.id, curl_escaped);
            curl_free(curl_escaped);

            free(command_line);
        }
        sleep(1);
    }
    pthread_cleanup_pop(0);

    return NULL;
}

static void nurly_worker_purge(void* data) {
    nurly_worker_t* worker_data = (nurly_worker_t*)data;

    curl_easy_cleanup(worker_data->curl);
}

void nurly_log(const char* text, ...) {
    va_list args;
    char line[1024] = "nurly: ";

    va_start(args, text);
    vsnprintf(line + 7, sizeof(line) - 7, text, args);
    va_end(args);

    write_to_all_logs(line, NSLOG_INFO_MESSAGE);
}
