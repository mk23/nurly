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
    nurly_service_check_t*        check_data;
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

    check_data = (nurly_service_check_t*)malloc(sizeof(nurly_service_check_t));
    if (check_data == NULL) {
        nurly_log("error: unable to allocate memory for service check item");
        return NEB_ERROR;
    }

//    nurly_log("host_name:           %s", service_data->host_name);
//    nurly_log("service_description: %s", service_data->service_description);
//    nurly_log("service_type:        %d", service_data->check_type);
//    nurly_log("service_options:     %d", CHECK_OPTION_NONE);
//    nurly_log("scheduled_check:     %d", CHECK_OPTION_NONE);
//    nurly_log("reschedule_check:    %d", CHECK_OPTION_NONE);
//    nurly_log("latency:             %f", service_data->latency);
//    nurly_log("start_time:          %lu.%lu", service_data->start_time.tv_sec, service_data->start_time.tv_usec);
    check_data->host    = strdup(service_data->host_name);
    check_data->service = strdup(service_data->service_description);
    check_data->command = strdup(service_data->command_line);
    check_data->latency = service_data->latency;
    check_data->start_time = service_data->start_time;

    nurly_queue_put(&nurly_work_q, (void*)check_data);

    return NEBERROR_CALLBACKOVERRIDE;
}
