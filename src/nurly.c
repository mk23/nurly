#include "nurly.h"

/* specify event broker API version (required) */
NEB_API_VERSION(CURRENT_NEB_API_VERSION)

/* global variables from nagios */
extern int      event_broker_options;

/* global variables for nurly */
nurly_config_t  nurly_config = NURLY_CONFIG_INITIALIZER;

char*           nurly_server;
nurly_queue_t   nurly_work_q = NURLY_QUEUE_INITIALIZER;
nebmodule*      nurly_module = NULL;
pthread_t       nurly_thread[NURLY_THREADS];

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

    if (nurly_config_read(args, &nurly_config) == OK) {
        nurly_server = nurly_config.checks_url;
        nurly_log("starting nurly via %s", nurly_config.checks_url);
    } else {
        return NEB_ERROR;
    }

    /* module info */
    neb_set_module_info(nurly_module, NEBMODULE_MODINFO_TITLE,     "Nurly");
    neb_set_module_info(nurly_module, NEBMODULE_MODINFO_AUTHOR,    "Max Kalika");
    neb_set_module_info(nurly_module, NEBMODULE_MODINFO_COPYRIGHT, "Copyright (c) 2013 Max Kalika");
    neb_set_module_info(nurly_module, NEBMODULE_MODINFO_VERSION,   NURLY_VERSION);
    neb_set_module_info(nurly_module, NEBMODULE_MODINFO_LICENSE,   "as-is");
    neb_set_module_info(nurly_module, NEBMODULE_MODINFO_DESC,      "distribute service checks via libcurl");

    /* register initializer callback */
    neb_register_callback(NEBCALLBACK_PROCESS_DATA, nurly_module, 0, nurly_callback_process_data);

    return NEB_OK;
}

int nebmodule_deinit(int flags, int reason) {
    neb_deregister_callback(NEBCALLBACK_PROCESS_DATA,       (void*)nurly_module);
    neb_deregister_callback(NEBCALLBACK_SERVICE_CHECK_DATA, (void*)nurly_module);

    nurly_work_q.purge = nurly_queue_free_item;
    nurly_queue_close(&nurly_work_q);
    nurly_config_free(&nurly_config);
    curl_global_cleanup();

    return NEB_OK;
}

void nurly_log(const char* text, ...) {
    va_list args;
    char line[1024] = "nurly: ";

    va_start(args, text);
    vsnprintf(line + 7, sizeof(line) - 7, text, args);
    va_end(args);

    write_to_all_logs(line, NSLOG_INFO_MESSAGE);
}
