#include <stdlib.h>

#include "queue.h"
#include "err.h"
#include "cacti.h"

queue_t* queue_init() {
    //queue_t *q = (queue_t*) malloc(sizeof(queue_t));
    //if (q == NULL)
    //    return NULL;

    queue_t *q = (queue_t*) safe_malloc(sizeof(queue_t));
    q->len      = 0;
    q->capacity = ACTOR_QUEUE_LIMIT;
    q->list     = safe_malloc(sizeof(content_t) * q->capacity);
    q->back     = 0;
    q->front    = 0;
    return q;
}

int queue_empty(queue_t* q) {
  return q->len == 0;
}

//static void extend_queue(queue_t* q) {
//    capacity

//}

int queue_push(queue_t* q, content_t data) {
    if (q->len == q->capacity) {
        //if (q->capacity * 2 <= ACTOR_QUEUE_LIMIT) {
        //    void extend_queue(q);
        //} else {
            // fatal("queue limit reached");
            return -1;
        //}
    }

    q->list[ q->back ] = data;
    q->back = (q->back + 1) % q->capacity;
    q->len++;

    return 0;
}

/** queue must not be empty */
content_t queue_pop(queue_t* q) {
    if (queue_empty(q)) {
        fatal("queue is empty");
    }

    content_t res = q->list[ q->front ];
    q->front = (q->front + 1) % q->capacity;
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
