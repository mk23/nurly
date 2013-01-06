#include "nurly.h"

extern nurly_queue_t nurly_work_q;
extern nebmodule*    nurly_module;
extern pthread_t     nurly_thread[NURLY_THREADS];

int nurly_process_data(int event_type, void* data) {
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

int nurly_service_check(int event_type, void* data) {
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

    nurly_queue_put(&nurly_work_q, (void*)&service_data);

    return NEBERROR_CALLBACKOVERRIDE;
}
