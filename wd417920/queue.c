#include <stdlib.h>

#include "queue.h"
#include "err.h"

queue_t* queue_init() {
    //queue_t *q = (queue_t*) malloc(sizeof(queue_t));
    //if (q == NULL)
    //    return NULL;

    queue_t *q = (queue_t*) safe_malloc(sizeof(queue_t));
    q->len = 0;
    q->back = NULL;
    q->front = NULL;
    return q;
}

int empty(queue_t* q) {
  return q->len == 0;
}

void queue_push(queue_t* q, void* data) {
    entry_t *entry = (entry_t*) safe_malloc(sizeof(entry_t));
    entry->data = data;
    entry->prev = NULL;

    if (empty(q)) {
        q->front = entry;
    } else {
        q->back->prev = entry;
    }

    q->back = entry;
    q->len++;
}

/** Returns null pointer if queue is empty */
void* queue_pop(queue_t* q) {
    if (empty(q)) {
        return NULL;
    }
    entry_t *entry = q->front;
    void *res = entry->data;
    q->front = entry->prev;
    free(entry);

    return res;
}

int queue_destroy(queue_t* q) {
    if (empty(q)) {
        free(q);
        return 0;
    }

    return 1;
}
