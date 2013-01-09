/*****************************************************************************
 * Compile with the following command:
 *
 *     gcc -O0 -ggdb -std=c99 -D_GNU_SOURCE=1 -I../include -shared -fPIC -o nurly.o nurly.c queue.c callbacks.c -lcurl -lrt -pthread
 *
 *****************************************************************************/

#include "nurly.h"

/* specify event broker API version (required) */
NEB_API_VERSION(CURRENT_NEB_API_VERSION)

/* global variables from nagios */
extern char*    temp_path;
extern int      event_broker_options;

/* global variables for nurly */
char*           nurly_server;
nurly_queue_t   nurly_work_q = NURLY_QUEUE_INITIALIZER;
nebmodule*      nurly_module = NULL;
pthread_t       nurly_thread[NURLY_THREADS];

int nebmodule_init(int flags, char* args, nebmodule* handle) {
    nurly_server = args;
    nurly_module = handle;

    if (!(event_broker_options & BROKER_PROGRAM_STATE)) {
        nurly_log("need BROKER_PROGRAM_STATE (%i or -1) in event_broker_options enabled to work", BROKER_PROGRAM_STATE);
        return NEB_ERROR;
    }
    if (!(event_broker_options & BROKER_SERVICE_CHECKS)) {
        nurly_log("need BROKER_SERVICE_CHECKS (%i or -1) in event_broker_options enabled to work", BROKER_SERVICE_CHECKS);
        return NEB_ERROR;
    }

    nurly_log("starting nurly to %s", nurly_server);

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
    char*                  query_uri;
    char*                  query_url;

    long                   reply_code;
    char*                  reply_text;
    char*                  reply_data;
    FILE*                  reply_file;
    size_t                 reply_size;

    CURL*                  curl_handle;
    timeval_t              finish_time;
    check_result*          check_data;
    nurly_service_check_t* check_info;

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,  NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    curl_handle = curl_easy_init();
    curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, (long)1);
    curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL,   (long)1);
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT,    (long)10);

    pthread_cleanup_push(nurly_worker_purge, (void*)curl_handle);
    while (1) {
        check_info = (nurly_service_check_t*)nurly_queue_get(&nurly_work_q);
        if (check_info) {
            nurly_log("checking service '%s' on host '%s'...\n", check_info->service, check_info->host);

            check_data = (check_result*)malloc(sizeof(check_result));
            if (check_data) {
                init_check_result(check_data);
                check_data->host_name           = check_info->host;
                check_data->service_description = check_info->service;
                check_data->check_type          = SERVICE_CHECK_ACTIVE;
                check_data->check_options       = CHECK_OPTION_NONE;
                check_data->scheduled_check     = TRUE;
                check_data->reschedule_check    = TRUE;

                asprintf(&(check_data->output_file), "%s/checkXXXXXX", temp_path);
                check_data->output_file_fd = mkstemp(check_data->output_file);
                if (check_data->output_file_fd) {
                    check_data->output_file_fp = fdopen(check_data->output_file_fd, "w");
                    nurly_log("check result output will be written to '%s' (fd=%d)\n", check_data->output_file, check_data->output_file_fd);

                    query_uri = curl_easy_escape(curl_handle, check_info->command, 0);
                    asprintf(&(query_url), "%s%s", nurly_server, query_uri);

                    curl_easy_setopt(curl_handle, CURLOPT_URL, query_url);

                    reply_file = open_memstream(&reply_text, &reply_size);

                    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, reply_file);

                    curl_easy_perform(curl_handle);
                    fclose(reply_file);

                    reply_data = escape_newlines(reply_text);

                    curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &reply_code);
                    if ((reply_code = (reply_code - 200) % 220) > 3) {
                        reply_code = 3;
                    }

                    /* get the check finish time */
                    gettimeofday(&finish_time, NULL);

                    fprintf(check_data->output_file_fp, "### Active Check Result File ###\n");
                    fprintf(check_data->output_file_fp, "file_time=%lu\n", (unsigned long)check_info->start_time.tv_sec);
                    fprintf(check_data->output_file_fp, "\n");

                    fprintf(check_data->output_file_fp, "### Nagios Service Check Result ###\n");
//                    fprintf(check_data->output_file_fp, "# Time: %s",               ctime(&check_info->start_time.tv_sec));
                    fprintf(check_data->output_file_fp, "host_name=%s\n",           check_data->host_name);
                    fprintf(check_data->output_file_fp, "service_description=%s\n", check_data->service_description);
                    fprintf(check_data->output_file_fp, "check_type=%d\n",          check_data->check_type);
                    fprintf(check_data->output_file_fp, "check_options=%d\n",       check_data->check_options);
                    fprintf(check_data->output_file_fp, "scheduled_check=%d\n",     check_data->scheduled_check);
                    fprintf(check_data->output_file_fp, "reschedule_check=%d\n",    check_data->reschedule_check);
                    fprintf(check_data->output_file_fp, "latency=%f\n",             check_info->latency + NURLY_TIMEDIFF(finish_time, check_info->start_time));
                    fprintf(check_data->output_file_fp, "start_time=%lu.%lu\n",     check_info->start_time.tv_sec, check_info->start_time.tv_usec);
                    fprintf(check_data->output_file_fp, "finish_time=%lu.%lu\n",    finish_time.tv_sec, finish_time.tv_usec);
                    fprintf(check_data->output_file_fp, "exited_ok=%d\n",           TRUE);
                    fprintf(check_data->output_file_fp, "return_code=%lu\n",        reply_code);
                    fprintf(check_data->output_file_fp, "output=%s\n",              reply_data);

                    /* close the temp file */
                    fflush(check_data->output_file_fp);
                    fclose(check_data->output_file_fp);

                    /* move check result to queue directory */
                    move_check_result_to_queue(check_data->output_file);

                    curl_free(query_uri);
                    NURLY_FREE(query_url);
                    NURLY_FREE(reply_text);
                    NURLY_FREE(reply_data);
                }

                NURLY_FREE(check_data->output_file);
                NURLY_FREE(check_data);
            }

            NURLY_SERVICE_CHECK_FREE(check_info);
        }

//            curl_escaped = curl_easy_escape(worker_data.curl, command_line, 0);
//            curl_free(curl_escaped);
//
        sleep(1);
    }
    pthread_cleanup_pop(0);

    return NULL;
}

void nurly_worker_purge(void* data) {
    curl_easy_cleanup((CURL*)data);
}

void nurly_log(const char* text, ...) {
    va_list args;
    char line[1024] = "nurly: ";

    va_start(args, text);
    vsnprintf(line + 7, sizeof(line) - 7, text, args);
    va_end(args);

    write_to_all_logs(line, NSLOG_INFO_MESSAGE);
}
