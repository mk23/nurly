/*****************************************************************************
 * Compile with the following command:
 *
 *     gcc -I../include -std=c99 -shared -fPIC -o nurly.o nurly.c -lcurl
 *
 *****************************************************************************/

#include "nurly.h"

/* specify event broker API version (required) */
NEB_API_VERSION(CURRENT_NEB_API_VERSION)

/* global variables from nagios */
extern int             event_broker_options;

/* global variables for nurly */
static pthread_mutex_t nurly_mutex  = PTHREAD_MUTEX_INITIALIZER;
       void*           nurly_module = NULL;
       pthread_t       nurly_thread[NURLY_THREADS];


/* declare nurly functions */
static int    nurly_process_events(int, void*);

       void*  nurly_worker_start(void*);
static void   nurly_worker_purge(void*);

static void   nurly_log(const char*, ...);


int nebmodule_init(int flags, char* args, nebmodule* handle) {
    nurly_log("nebmodule_init(%i, args, handle)", flags);

    nurly_module = handle;

    if (!(event_broker_options & BROKER_PROGRAM_STATE)) {
        nurly_log("need BROKER_PROGRAM_STATE (%i or -1) in event_broker_options enabled to work", BROKER_PROGRAM_STATE);
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
    neb_register_callback(NEBCALLBACK_PROCESS_DATA, nurly_module, 0, nurly_process_events);

    return NEB_OK;
}

int nebmodule_deinit(int flags, int reason) {
    nurly_log("nebmodule_deinit(%i, %i)", flags, reason);

    neb_deregister_callback(NEBCALLBACK_PROCESS_DATA, nurly_module);

    for (int i = 0; i < NURLY_THREADS; i++) {
        pthread_cancel(nurly_thread[i]);
        pthread_join(nurly_thread[i], NULL);
    }

    curl_global_cleanup();

    return NEB_OK;
}

static int nurly_process_events(int event_type, void* data) {
    struct nebstruct_process_struct* event_data;

    nurly_log("nurly_process_events(%i, data)", event_type);

    event_data = (struct nebstruct_process_struct*)data;
    if (event_data->type == NEBTYPE_PROCESS_EVENTLOOPSTART) {
        curl_global_init(CURL_GLOBAL_DEFAULT);

        for (int i = 0; i < NURLY_THREADS; i++) {
            pthread_create(&nurly_thread[i], NULL, nurly_worker_start, (void *)NULL);
        }

        /* nurly_register_callbacks(); */
    }

    return NEB_OK;
}

void* nurly_worker_start(void* data) {
    nurly_worker_t worker_data;

    nurly_log("nurly_worker_start()");

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    worker_data.curl = curl_easy_init();

    pthread_cleanup_push(nurly_worker_purge, (void*)&worker_data);
    while (1) {
        sleep(1);
    }
    pthread_cleanup_pop(0);

    return NULL;
}

static void nurly_worker_purge(void* data) {
    nurly_worker_t* worker_data = (nurly_worker_t*)data;

    nurly_log("nurly_worker_purge()");

    curl_easy_cleanup(worker_data->curl);
}

static void nurly_log(const char* text, ...) {
    va_list args;
    char line[1024] = "nurly: ";

    va_start(args, text);
    vsnprintf(line + 7, sizeof(line) - 7, text, args);
    va_end(args);

    write_to_all_logs(line, NSLOG_INFO_MESSAGE);
}
