#include "nurly.h"

extern char* temp_path;
extern char* nurly_server;

static check_result* nurly_check_result_init(nurly_service_check_t* check_info) {
    char*         output_file = NULL;
    check_result* result_data = NULL;

    do {
        if (asprintf(&output_file, "%s/checkXXXXXX", temp_path) == -1) {
            nurly_log("error: unable to allocate memory for check file name");
            break;
        }
        if ((result_data = (check_result*)malloc(sizeof(check_result))) == NULL) {
            nurly_log("error: unable to allocate memory for check result item");
            break;
        }

        init_check_result(result_data);
        result_data->host_name           = check_info->host;
        result_data->service_description = check_info->service;
        result_data->latency             = check_info->latency;
        result_data->start_time          = check_info->start_time;
        result_data->check_type          = SERVICE_CHECK_ACTIVE;
        result_data->check_options       = CHECK_OPTION_NONE;
        result_data->scheduled_check     = TRUE;
        result_data->reschedule_check    = TRUE;
        result_data->output_file         = output_file;

        if ((result_data->output_file_fd = mkstemp(result_data->output_file)) == -1) {
            nurly_log("error: unable to create check output file");
            break;
        } else {
            result_data->output_file_fp = fdopen(result_data->output_file_fd, "w");
        }

        return result_data;
    } while (FALSE);

    NURLY_FREE(output_file);
    NURLY_FREE_CHECK_RESULT(result_data);
    return NULL;
}

static int nurly_check_curl_output(check_result* result_data, CURL* curl_handle, char* command_line) {
    int    ret = FALSE;
    long   http_code;
    char*  query_uri = NULL;
    char*  query_url = NULL;
    char*  reply_txt = NULL;
    FILE*  reply_obj = NULL;
    size_t reply_len;

    char curl_error[CURL_ERROR_SIZE];

    do {
        if ((reply_obj = open_memstream(&reply_txt, &reply_len)) == NULL) {
            nurly_log("error: unable to create memstream object");
            break;
        }
        if ((query_uri = curl_easy_escape(curl_handle, command_line, 0)) == NULL) {
            nurly_log("error: unable to allocate memory for escaped command line");
            break;
        }
        if (asprintf(&(query_url), "%s%s", nurly_server, query_uri) == -1) {
            nurly_log("error: unable to allocate memory for full command url");
            break;
        }
        if (curl_easy_setopt(curl_handle, CURLOPT_ERRORBUFFER, curl_error) != CURLE_OK) {
            nurly_log("error: unable to set CURLOPT_ERRORBUFFER");
            break;
        }
        if (curl_easy_setopt(curl_handle, CURLOPT_URL, query_url) != CURLE_OK) {
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

        NURLY_CLOSE(reply_obj);

        if ((result_data->output = escape_newlines(reply_txt)) == NULL) {
            nurly_log("error: unable to allocate memory for escaped output");
            break;
        }

        switch (http_code) {
            case 200:
                result_data->return_code = STATE_OK;
                break;
            case 221:
                result_data->return_code = STATE_WARNING;
                break;
            case 222:
                result_data->return_code = STATE_CRITICAL;
                break;
            default:
                result_data->return_code = STATE_UNKNOWN;
                break;
        }

        gettimeofday(&(result_data->finish_time), NULL);
        result_data->exited_ok = TRUE;

        ret = TRUE;
    } while (FALSE);

    curl_free(query_uri);
    NURLY_FREE(query_url);
    NURLY_FREE(reply_txt);
    NURLY_CLOSE(reply_obj);

    return ret;
}

static int nurly_check_save_output(check_result* result_data) {
    char start_time[26];

    fprintf(result_data->output_file_fp, "### Active Check Result File ###\n");
    fprintf(result_data->output_file_fp, "file_time=%lu\n",          (unsigned long)result_data->start_time.tv_sec);
    fprintf(result_data->output_file_fp, "\n");

    fprintf(result_data->output_file_fp, "### Nagios Service Check Result ###\n");
    fprintf(result_data->output_file_fp, "# Time: %s",               ctime_r(&(result_data->start_time.tv_sec), start_time));
    fprintf(result_data->output_file_fp, "host_name=%s\n",           result_data->host_name);
    fprintf(result_data->output_file_fp, "service_description=%s\n", result_data->service_description);
    fprintf(result_data->output_file_fp, "check_type=%d\n",          result_data->check_type);
    fprintf(result_data->output_file_fp, "check_options=%d\n",       result_data->check_options);
    fprintf(result_data->output_file_fp, "scheduled_check=%d\n",     result_data->scheduled_check);
    fprintf(result_data->output_file_fp, "reschedule_check=%d\n",    result_data->reschedule_check);
    fprintf(result_data->output_file_fp, "latency=%f\n",             result_data->latency + NURLY_TIMEDIFF(result_data->finish_time, result_data->start_time));
    fprintf(result_data->output_file_fp, "start_time=%lu.%lu\n",     result_data->start_time.tv_sec, result_data->start_time.tv_usec);
    fprintf(result_data->output_file_fp, "finish_time=%lu.%lu\n",    result_data->finish_time.tv_sec, result_data->finish_time.tv_usec);
    fprintf(result_data->output_file_fp, "exited_ok=%d\n",           result_data->exited_ok);
    fprintf(result_data->output_file_fp, "return_code=%d\n",         result_data->return_code);
    fprintf(result_data->output_file_fp, "output=%s\n",              result_data->output);

    /* close the temp file */
    NURLY_CLOSE(result_data->output_file_fp);

    return TRUE;
}

void nurly_check_service(CURL* curl_handle, nurly_service_check_t* check_info) {
    check_result* result_data = NULL;

    do {
        if ((result_data = nurly_check_result_init(check_info)) == NULL) {
            break;
        }
        if (!nurly_check_curl_output(result_data, curl_handle, check_info->command)) {
            break;
        }
        if (!nurly_check_save_output(result_data)) {
            break;
        }

        /* move check result to queue directory */
        move_check_result_to_queue(result_data->output_file);
    } while (FALSE);

    NURLY_FREE_CHECK_RESULT(result_data);
    NURLY_FREE_SERVICE_CHECK(check_info);
}
