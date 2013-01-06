/*****************************************************************************
 * Compile with the following command:
 *
 *     gcc -I../include -std=c99 -shared -fPIC -o nurly.o nurly.c queue.c callbacks.c -lcurl -lrt -pthread
 *
 *****************************************************************************/

#include "nurly.h"

/* specify event broker API version (required) */
NEB_API_VERSION(CURRENT_NEB_API_VERSION)

/* global variables from nagios */
extern int      event_broker_options;

/* global variables for nurly */
nurly_queue_t   nurly_work_q = NURLY_QUEUE_INITIALIZER;
nebmodule*      nurly_module = NULL;
pthread_t       nurly_thread[NURLY_THREADS];


/* declare nurly functions */
void*  nurly_worker_start(void*);
void   nurly_worker_purge(void*);

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
    neb_deregister_callback(NEBCALLBACK_PROCESS_DATA, (void*)nurly_module);

    //for (long i = 0; i < NURLY_THREADS; i++) {
    //    pthread_cancel(nurly_thread[i]);
    //    pthread_join(nurly_thread[i], NULL);
    //}

    curl_global_cleanup();

    return NEB_OK;
}

void* nurly_worker_start(void* data) {
//    char*          curl_escaped;
    nurly_worker_t worker_data;

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    worker_data.id   = (long)data;
    worker_data.curl = curl_easy_init();

    pthread_cleanup_push(nurly_worker_purge, (void*)&worker_data);
    while (1) {
//        command_line = (char*)nurly_queue_get(&nurly_work_q);
//        if (command_line) {
//            curl_escaped = curl_easy_escape(worker_data.curl, command_line, 0);
//            nurly_log("got work item in worker %02i: %s", worker_data.id, curl_escaped);
//            curl_free(curl_escaped);
//
//            free(command_line);
//        }
        sleep(1);
    }
    pthread_cleanup_pop(0);

    return NULL;
}

void nurly_worker_purge(void* data) {
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
