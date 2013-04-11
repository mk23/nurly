#include "config.h"

#include <stdio.h>
#include <regex.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

#include <curl/curl.h>

#include "queue.h"
#include "nagios/nagios.h"
#include "nagios/broker.h"
#include "nagios/neberrors.h"
#include "nagios/nebmodules.h"
#include "nagios/nebcallbacks.h"
#include "nagios/nebstructs.h"

#define NURLY_VERSION PACKAGE_VERSION

#define NURLY_FREE(p) \
    do { if (p) { free(p); p = NULL; } } while (FALSE)
#define NURLY_CLOSE(f) \
    do { if (f) { fclose(f) ; f = NULL; } } while (FALSE)

#define NURLY_TIMESTAMP(t) \
    (double)t.tv_sec + (double)t.tv_usec / 1000000
#define NURLY_TIMEDIFF(a,b) \
    (NURLY_TIMESTAMP(a) - NURLY_TIMESTAMP(b))

#define NURLY_CONFIG_INITIALIZER { NULL, NULL, 2, 20, NURLY_QUEUE_INITIALIZER, NURLY_QUEUE_INITIALIZER }

typedef struct timeval timeval_t;

typedef struct nurly_config {
    char*         checks_url;
    char*         health_url;
    int           health_interval;
    int           worker_threads;
    nurly_queue_t skip_hosts;
    nurly_queue_t skip_services;
} nurly_config_t;

typedef enum nurly_worker {
    NURLY_WORKER_CHECKS,
    NURLY_WORKER_HEALTH
} nurly_worker_t;

/* utility */
void nurly_log(const char*, ...);

/* configs */
int  nurly_config_read(char*, nurly_config_t*);
void nurly_config_free(nurly_config_t*);

/* callbacks */
int  nurly_callback_process_data(int, void*);
int  nurly_callback_service_check(int, void*);
void nurly_callback_free_result(void*);

/* threads */
void* nurly_worker_start(void*);

/* checks */
void nurly_check_service(CURL*, check_result*);

/* health */
void nurly_check_health(CURL*);
