/*****************************************************************************
 * Compile with the following command:
 *
 *     gcc -I../include -std=c99 -shared -fPIC -o nurly.o nurly.c queue.c callbacks.c -lcurl -lrt -pthread
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
    char*                  reply_txt;
    FILE*                  reply_obj;
    size_t                 reply_len;
    CURL*                  curl_hndl;
    check_result*          check_rsp;
    nurly_service_check_t* check_req;

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,  NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    curl_hndl = curl_easy_init();
    curl_easy_setopt(curl_hndl, CURLOPT_NOPROGRESS, (long)1);
    curl_easy_setopt(curl_hndl, CURLOPT_NOSIGNAL,   (long)1);
    curl_easy_setopt(curl_hndl, CURLOPT_TIMEOUT,    (long)10);

    pthread_cleanup_push(nurly_worker_purge, (void*)curl_hndl);
    while (1) {
        check_req = (nurly_service_check_t*)nurly_queue_get(&nurly_work_q);
        if (check_req) {
            nurly_log("checking service '%s' on host '%s'...\n", check_req->service, check_req->host);

            check_rsp = (check_result*)malloc(sizeof(check_result));
            if (check_rsp) {
                init_check_result(check_rsp);

                asprintf(&(check_rsp->output_file), "%s/checkXXXXXX", temp_path);
                check_rsp->output_file_fd = mkstemp(check_rsp->output_file);
                if (check_rsp->output_file_fd) {
                    check_rsp->output_file_fp = fdopen(check_rsp->output_file_fd, "w");
                    nurly_log("check result output will be written to '%s' (fd=%d)\n", check_rsp->output_file, check_rsp->output_file_fd);

                    query_uri = curl_easy_escape(curl_hndl, check_req->command, 0);
                    asprintf(&(query_url), "%s%s", nurly_server, query_uri);

                    curl_easy_setopt(curl_hndl, CURLOPT_URL, query_url);

                    reply_obj = open_memstream(&reply_txt, &reply_len);

                    curl_easy_setopt(curl_hndl, CURLOPT_WRITEDATA, reply_obj);

                    curl_easy_perform(curl_hndl);
                    fclose(reply_obj);

                    nurly_log("check_result: %s: (%d) %s", query_url, reply_len, reply_txt);

                    curl_free(query_uri);
                    NURLY_FREE(query_url);
                    NURLY_FREE(reply_txt);

                }

                NURLY_FREE(check_rsp->output_file);
                NURLY_FREE(check_rsp);
            }

            NURLY_SERVICE_CHECK_FREE(check_req);
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
