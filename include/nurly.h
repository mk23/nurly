#include <stdio.h>
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

#define NURLY_VERSION "1.0.0"
#define NURLY_THREADS 1

#define NURLY_FREE(p) \
    do { if (p) { free(p); p = NULL; } } while (0)
#define NURLY_FREE_CHECK_RESULT(p) \
    do { if (p) { free_check_result(p); } NURLY_FREE(p); } while (0)
#define NURLY_FREE_SERVICE_CHECK(p) \
    do { if (p) { NURLY_FREE(p->host); NURLY_FREE(p->service); NURLY_FREE(p->command); } NURLY_FREE(p); } while (0)

#define NURLY_CLOSE(f) \
    do { if (f) { fclose(f) ; f = NULL; } } while (0)

#define NURLY_TIMESTAMP(t) \
    (double)t.tv_sec + (double)t.tv_usec / 1000000
#define NURLY_TIMEDIFF(a,b) \
    (NURLY_TIMESTAMP(a) - NURLY_TIMESTAMP(b))

typedef struct timeval timeval_t;

typedef struct nurly_service_check {
    char*     host;
    char*     service;
    char*     command;
    float     latency;
    timeval_t start_time;
} nurly_service_check_t;

/* utility */
void nurly_log(const char*, ...);

/* callbacks */
int nurly_callback_process_data(int, void*);
int nurly_callback_service_check(int, void*);

/* threads */
void* nurly_worker_start(void*);
void  nurly_worker_purge(void*);

/* checks */
void nurly_check_service(CURL*, nurly_service_check_t*);

