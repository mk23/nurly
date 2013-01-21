#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

#include <curl/curl.h>

#include "config.h"
#include "queue.h"
#include "nagios/nagios.h"
#include "nagios/broker.h"
#include "nagios/neberrors.h"
#include "nagios/nebmodules.h"
#include "nagios/nebcallbacks.h"
#include "nagios/nebstructs.h"

#define NURLY_VERSION PACKAGE_VERSION
#define NURLY_THREADS 20

#define NURLY_FREE(p) \
    do { if (p) { free(p); p = NULL; } } while (FALSE)
#define NURLY_CLOSE(f) \
    do { if (f) { fclose(f) ; f = NULL; } } while (FALSE)

#define NURLY_TIMESTAMP(t) \
    (double)t.tv_sec + (double)t.tv_usec / 1000000
#define NURLY_TIMEDIFF(a,b) \
    (NURLY_TIMESTAMP(a) - NURLY_TIMESTAMP(b))

typedef struct timeval timeval_t;

/* utility */
void nurly_log(const char*, ...);

/* callbacks */
int  nurly_callback_process_data(int, void*);
int  nurly_callback_service_check(int, void*);
void nurly_callback_free_result(void*);

/* threads */
void* nurly_worker_start(void*);

/* checks */
void nurly_check_service(CURL*, check_result*);
