#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include <curl/curl.h>

#include "nagios/nagios.h"
#include "nagios/broker.h"
#include "nagios/neberrors.h"
#include "nagios/nebmodules.h"
#include "nagios/nebcallbacks.h"
#include "nagios/nebstructs.h"

#define NURLY_VERSION "1.0.0"
#define NURLY_THREADS 20

typedef struct nurly_worker {
    CURL* curl;
} nurly_worker_t;

int nebmodule_init(int, char*, nebmodule*);

int nebmodule_deinit(int, int);
