#include <pthread.h>

#include "blocking_queue.h"
#include "err.h"

blocking_queue_t* blocking_queue_init() {
    blocking_queue_t *bq = safe_malloc(sizeof(blocking_queue_t));

    int err;
    // init mutex
    if ((err = pthread_mutex_init(&bq->lock, 0)) != 0)
        syserr(err, "mutex init failed");
    if ((err = pthread_cond_init(&bq->ready_threads, 0)) != 0)
        syserr(err, "cond init failed");

    bq->len = 0;
    bq->interrupted = 0;
    bq->front = NULL;
    bq->back = NULL;

    return bq;
}

void blocking_queue_push(blocking_queue_t *bq, actor_id_t id) {
    int err;

    blocking_entry_t *new_entry = (blocking_entry_t*) safe_malloc(sizeof(blocking_entry_t));
    new_entry->data = id;
    new_entry->prev = NULL;


    safe_lock(&bq->lock); // "->" has higher precedence than "&"

    if (bq->len == 0) {
        bq->front = new_entry;
    } else {
        bq->back->prev = new_entry;
    }

    bq->back = new_entry;

    if (bq->len == 0)
        if ((err = pthread_cond_signal(&bq->lock)) != 0) // if no thread waits then nothing happens
            syserr(err, "cond signal failed");

    bq->len++;

    safe_unlock(&bq->lock);
}

/// returns -1 if queue was interrupted
actor_id_t blocking_queue_pop(blocking_queue_t *bq) {
    int err;
    blocking_entry_t *pop;
    actor_id_t res;

    safe_lock(&bq->lock);

    while (bq->len == 0 && interrupted != 1)
        if ((err = pthread_cond_wait(&bq->ready, &bq->lock)) != 0)
            syserr(err, "cond wait failed");

    pop = bq->front;

    if (pop != NULL) {
        res = pop->data;
        bq->front = pop->prev;

        free(pop);

        bq->len--;

        safe_unlock(&bq->lock);
        return res;
    }

    safe_unlock(&bq->lock);
    return -1;

}

void blocking_queue_signal_all(blocking_queue_t *bq) {
    int err = 0;
    safe_lock(&bq->lock);

    bq->interrupted = 1;

    if ((err = pthread_cond_broadcast(&bq->lock)) != 0)
        syserr(err, "cond wait failed");

    safe_unlock(&bq->lock);

}

int blocking_queue_empty(blocking_queue_t *bq) {
    int res;

    safe_lock(&bq->lock);
    res = bq->len;
    safe_unlock(&bq->lock);

    return res == 0;
}

/// this function should be called only when conditions (1-3) hold:
/// 1) queue is empty
/// 2) blocking queue has been signalled
/// 3) threads operating on the queue have been joined
int blocking_queue_destroy(blocking_queue_t *bq) {
    int err;
    if (!blocking_queue_empty(bq))
        return -1;

    if ((err = pthread_cond_destroy (&bq->ready)) != 0)
        syserr (err, "cond destroy failed");
    if ((err = pthread_mutex_destroy (&bq->lock)) != 0)
        syserr (err, "mutex destroy failed");

    free(bq);
    return 0;
}
