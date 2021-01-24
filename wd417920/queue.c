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

int queue_empty(queue_t* q) {
  return q->len == 0;
}

int queue_push(queue_t* q, content_t data) {
    entry_t *entry = malloc(sizeof(entry_t));
    if (entry == NULL) return -1;

    entry->data = data;
    entry->prev = NULL;

    if (queue_empty(q)) {
        q->front = entry;
    } else {
        q->back->prev = entry;
    }

    q->back = entry;
    q->len++;
    return 0;
}

/** queue must not be empty */
content_t queue_pop(queue_t* q) {
    if (queue_empty(q)) {
        fatal("queue is empty");
    }
    entry_t *entry = q->front;
    content_t res = entry->data;
    q->front = entry->prev;
    free(entry);
    q->len--;
    return res;
}

int queue_destroy(queue_t* q) {
    if (queue_empty(q)) {
        free(q);
        return 0;
    }

    return -1;
}
