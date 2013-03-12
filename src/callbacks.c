#include "nurly.h"

extern char*         temp_path;
extern nurly_queue_t nurly_queue;
extern nebmodule*    nurly_module;
extern pthread_t     nurly_thread[NURLY_THREADS];

int nurly_callback_process_data(int event_type, void* data) {
    nebstruct_process_data* process_data;

    process_data = (nebstruct_process_data*)data;
    if (process_data == NULL) {
        nurly_log("error: received invalid process data");
        return NEB_ERROR;
    }

    if (process_data->type == NEBTYPE_PROCESS_EVENTLOOPSTART) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        nurly_queue.purge = nurly_callback_free_result;

        for (long i = 0; i < NURLY_THREADS; i++) {
            pthread_create(&nurly_thread[i], NULL, nurly_worker_start, (void*)i);
        }

        neb_register_callback(NEBCALLBACK_SERVICE_CHECK_DATA, nurly_module, 0, nurly_callback_service_check);

        nurly_log("initialization complete, version: %s", NURLY_VERSION);
    }

    return NEB_OK;
}

int nurly_callback_service_check(int event_type, void* data) {
    check_result*                 result_data;
    nebstruct_service_check_data* service_data;

    if ((service_data = (nebstruct_service_check_data*)data) == NULL) {
        nurly_log("error: received invalid service data");
        return NEB_ERROR;
    }

    /* ignore non-initiate service checks */
    if (service_data->type != NEBTYPE_SERVICECHECK_INITIATE) {
        return NEB_OK;
    }

    do {
        if ((result_data = (check_result*)malloc(sizeof(check_result))) == NULL) {
            nurly_log("error: unable to allocate memory for check result item");
            break;
        } else {
            init_check_result(result_data);
        }

        if ((result_data->host_name = strdup(service_data->host_name)) == NULL) {
            nurly_log("error: unable to allocate memory for host string");
            break;
        }
        if ((result_data->service_description = strdup(service_data->service_description)) == NULL) {
            nurly_log("error: unable to allocate memory for service string");
            break;
        }

        /* intentional hack: hijacking an unused pointer slot for the command line */
        if ((result_data->next = (check_result*)strdup(service_data->command_line)) == NULL) {
            nurly_log("error: unable to allocate memory for command string");
            break;
        }

        if (asprintf(&(result_data->output_file), "%s/checkXXXXXX", temp_path) == -1) {
            nurly_log("error: unable to allocate memory for check file name");
            break;
        }
        if ((result_data->output_file_fd = mkstemp(result_data->output_file)) == -1) {
            nurly_log("error: unable to create check output file");
            break;
        } else {
            result_data->output_file_fp = fdopen(result_data->output_file_fd, "w");
        }

        result_data->object_check_type   = HOST_CHECK;
        result_data->check_type          = SERVICE_CHECK_ACTIVE;
        result_data->check_options       = CHECK_OPTION_NONE;
        result_data->scheduled_check     = TRUE;
        result_data->reschedule_check    = TRUE;
        result_data->start_time          = service_data->start_time;
        result_data->latency             = service_data->latency;

        nurly_queue_put(&nurly_queue, (void*)result_data);

        return NEBERROR_CALLBACKOVERRIDE;
    } while (FALSE);

    nurly_callback_free_result(result_data);
    return NEB_ERROR;
}

void nurly_callback_free_result(void* result_data) {
    if (result_data != NULL) {
        NURLY_FREE(((check_result*)result_data)->next);
        free_check_result((check_result*)result_data);
    }
    NURLY_FREE(result_data);
}
