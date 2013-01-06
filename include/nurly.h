#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <curl/curl.h>

#include "queue.h"
#include "nagios/nagios.h"
#include "nagios/broker.h"
#include "nagios/neberrors.h"
#include "nagios/nebmodules.h"
#include "nagios/nebcallbacks.h"
#include "nagios/nebstructs.h"

#define NURLY_VERSION "1.0.0"
#define NURLY_THREADS 20

typedef struct nurly_worker {
    int   id;
    CURL* curl;
} nurly_worker_t;

typedef struct timespec timespec_t;

/* utility */
void nurly_log(const char*, ...);

/* callbacks */
int nurly_process_data(int, void*);
int nurly_host_check(int, void*);
int nurly_service_check(int, void*);

/* threads */
void* nurly_worker_start(void*);
void  nurly_worker_purge(void*);
