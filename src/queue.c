#include "nurly.h"

void nurly_queue_put(nurly_queue_t* queue, void* data) {
    nurly_queue_item_t* item = NULL;

    if (queue == NULL || (!(queue->head == NULL && queue->tail == NULL) && (queue->head == NULL || queue->tail == NULL))) {
        nurly_log("error: queue is not initialized");
        return;
    }

    item = (nurly_queue_item_t*)malloc(sizeof(nurly_queue_item_t));
    if (item == NULL) {
        nurly_log("error: unable to allocate memory for queue item");
        return;
    }

    item->data = data;
    item->next = NULL;

    pthread_mutex_lock(&(queue->lock));
    if (queue->head == NULL && queue->tail == NULL) {
        queue->head = queue->tail = item;
    } else {
        queue->tail->next = item;
        queue->tail = item;
    }
    queue->size++;
    pthread_mutex_unlock(&(queue->lock));
    pthread_cond_broadcast(&(queue->empty));
}

void* nurly_queue_get(nurly_queue_t* queue) {
    void*               data = NULL;
    nurly_queue_item_t* item = NULL;

    if (queue == NULL || (!(queue->head == NULL && queue->tail == NULL) && (queue->head == NULL || queue->tail == NULL))) {
        nurly_log("error: queue is not initialized");
        return NULL;
    }

    pthread_mutex_lock(&(queue->lock));
    while (queue->size == 0 && !queue->done) {
        pthread_cond_wait(&(queue->empty), &(queue->lock));
    }
    if (!queue->done) {
        item = queue->head;
        data = item->data;
        queue->head = item->next;
        if (queue->head == NULL) {
            queue->tail = NULL;
        }
        NURLY_FREE(item);
        queue->size--;
    }
    pthread_mutex_unlock(&(queue->lock));

    return data;
}

int nurly_queue_len(nurly_queue_t* queue) {
    pthread_mutex_lock(&(queue->lock));
    int size = queue->size;
    pthread_mutex_unlock(&(queue->lock));

    return size;
}

int nurly_queue_has(nurly_queue_t* queue, void* data, int (*search)(void*, void*)) {
    nurly_queue_item_t* item = NULL;

    for (nurly_queue_item_t* item = queue->head; item; item = item->next) {
        if (search(item->data, data)) {
            return TRUE;
        }
    }

    return FALSE;
}

void nurly_queue_close(nurly_queue_t* queue) {
    void*               data = NULL;
    nurly_queue_item_t* item = NULL;

    pthread_mutex_lock(&(queue->lock));
    queue->done = TRUE;
    pthread_mutex_unlock(&(queue->lock));
    pthread_cond_broadcast(&(queue->empty));

    while ((item = queue->head) != NULL) {
        queue->purge(item->data);
        queue->head = item->next;
        NURLY_FREE(item);
    }
}

void nurly_queue_free_item(void* data) {
    return;
}
