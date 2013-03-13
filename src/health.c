#include "nurly.h"

extern int            nurly_health;
extern nurly_config_t nurly_config;

void nurly_check_health(CURL* curl_handle) {
    int    http_code = 0;
    char*  reply_txt = NULL;
    FILE*  reply_obj = NULL;
    size_t reply_len;

    char curl_error[CURL_ERROR_SIZE];

    do {
        if (nurly_config.health_url == NULL) {
            nurly_log("health_url is disabled by configuration, service checks will be unconditionally distributed");
            http_code = 200;
            break;
        }
        if ((reply_obj = open_memstream(&reply_txt, &reply_len)) == NULL) {
            nurly_log("error: unable to create memstream object");
            break;
        }
        if (curl_easy_setopt(curl_handle, CURLOPT_ERRORBUFFER, curl_error) != CURLE_OK) {
            nurly_log("error: unable to set CURLOPT_ERRORBUFFER");
            break;
        }
        if (curl_easy_setopt(curl_handle, CURLOPT_URL, nurly_config.health_url) != CURLE_OK) {
            nurly_log("error: unable to set CURLOPT_URL: %s", curl_error);
            break;
        }
        if (curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, reply_obj) != CURLE_OK) {
            nurly_log("error: unable to set CURLOPT_WRITEDATA: %s", curl_error);
            break;
        }
        if (curl_easy_perform(curl_handle) != CURLE_OK) {
            nurly_log("error: unable to perform curl: %s", curl_error);
            break;
        }
        if (curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &http_code) != CURLE_OK) {
            nurly_log("error: unable to get CURLINFO_RESPONSE_CODE: %s", curl_error);
            break;
        }
    } while (FALSE);

    NURLY_FREE(reply_txt);
    NURLY_CLOSE(reply_obj);

    if (http_code != 200 && nurly_health == TRUE) {
        nurly_log("health check failed, disabling service check distribution");
    } else if (http_code == 200 && nurly_health == FALSE) {
        nurly_log("health check passed, enabling service check distribution");
    }

    nurly_health = http_code == 200;
}
