#include "nurly.h"

extern nurly_queue_t nurly_work_q;

static void nurly_worker_loop(CURL* curl_handle) {
    nurly_service_check_t* check_info = NULL;

    curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, (long)1);
    curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL,   (long)1);
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT,    (long)10);

    while (TRUE) {
        check_info = (nurly_service_check_t*)nurly_queue_get(&nurly_work_q);
        if (check_info) {
            nurly_log("checking service '%s' on host '%s' ...", check_info->service, check_info->host);
        } else {
            nurly_log("internal queue error getting service check");
            break;
        }

        nurly_check_service(curl_handle, check_info);
    }
}

void* nurly_worker_start(void* data) {
    CURL*                  curl_handle;

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,  NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    curl_handle = curl_easy_init();

    pthread_cleanup_push(nurly_worker_purge, (void*)curl_handle);
    nurly_worker_loop(curl_handle);
    pthread_cleanup_pop(0);

    return NULL;
}

void nurly_worker_purge(void* data) {
    curl_easy_cleanup((CURL*)data);
}
