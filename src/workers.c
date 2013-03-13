#include "nurly.h"

extern nurly_queue_t nurly_queue;

static void nurly_worker_loop(CURL* curl_handle) {
    check_result* result_data = NULL;

    while (TRUE) {
        result_data = (check_result*)nurly_queue_get(&nurly_queue);
        if (result_data) {
            nurly_log("checking service '%s' on host '%s' ...", result_data->service_description, result_data->host_name);
        } else {
            nurly_log("queue is closed, terminating worker thread");
            break;
        }

        nurly_check_service(curl_handle, result_data);
        nurly_callback_free_result(result_data);
    }
}

void* nurly_worker_start(void* data) {
    CURL* curl_handle;

    curl_handle = curl_easy_init();
    curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, (long)1);
    curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL,   (long)1);
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT,    (long)10);

    nurly_worker_loop(curl_handle);

    curl_easy_cleanup(curl_handle);

    return NULL;
}
