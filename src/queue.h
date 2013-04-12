#define NURLY_QUEUE_INITIALIZER { NULL, NULL, 0, FALSE, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, nurly_queue_free_item }

typedef struct nurly_queue_item {
    void*                    data;
    struct nurly_queue_item* next;
} nurly_queue_item_t;

typedef struct nurly_queue {
    nurly_queue_item_t* head;
    nurly_queue_item_t* tail;
    int                 size;
    int                 done;
    pthread_mutex_t     lock;
    pthread_cond_t      empty;
    void                (*purge)(void*);
} nurly_queue_t;

void  nurly_queue_put(nurly_queue_t*, void*);
void* nurly_queue_get(nurly_queue_t*);
int   nurly_queue_len(nurly_queue_t*);
int   nurly_queue_has(nurly_queue_t*, void*, int (*)(void*, void*)); // not thread safe
void  nurly_queue_close(nurly_queue_t*);
void  nurly_queue_free_item(void*);
