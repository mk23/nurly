#include "nurly.h"

extern nurly_queue_t  nurly_queue;
extern nurly_config_t nurly_config;

static void nurly_checks_loop(CURL* curl_handle) {
    check_result* result_data = NULL;

    while (TRUE) {
        result_data = (check_result*)nurly_queue_get(&nurly_queue);
        if (result_data) {
            nurly_log("distributing service %s on %s", result_data->service_description, result_data->host_name);
        } else {
            nurly_log("queue is closed, terminating worker thread");
            break;
        }

        nurly_check_service(curl_handle, result_data);
        nurly_callback_free_result(result_data);
    }
}

static void nurly_health_loop(CURL* curl_handle) {
    while (TRUE) {
        if (!nurly_queue.done) {
            nurly_check_health(curl_handle);
            sleep(nurly_config.health_interval);
        } else {
            nurly_log("queue is closed, terminating health thread");
            break;
        }
    }
}

void* nurly_worker_start(void* data) {
    CURL*          curl_handle;
    nurly_worker_t worker_type = (nurly_worker_t)data;

    curl_handle = curl_easy_init();
    curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, (long)1);
    curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL,   (long)1);
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT,    (long)10);

    if (worker_type == NURLY_WORKER_CHECKS) {
        nurly_log("starting checks thread");
        nurly_checks_loop(curl_handle);
    } else if (worker_type == NURLY_WORKER_HEALTH) {
        nurly_log("starting health thread");
        nurly_health_loop(curl_handle);
    }

    curl_easy_cleanup(curl_handle);

    return NULL;
}
