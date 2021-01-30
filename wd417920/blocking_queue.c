#include <pthread.h>
#include <stdlib.h>

#include "blocking_queue.h"
#include "err.h"

blocking_queue_t* blocking_queue_init() {
    int err;
    blocking_queue_t *bq = malloc(sizeof(blocking_queue_t));
    if (bq == NULL) return NULL;

    // init mutex
    if ((err = pthread_mutex_init(&bq->lock, 0)) != 0)
        syserr(err, "mutex init failed");
    if ((err = pthread_cond_init(&bq->ready, 0)) != 0)
        syserr(err, "cond init failed");

    bq->len = 0;
    bq->interrupted = 0;
    bq->front = NULL;
    bq->back = NULL;

    return bq;
}

int blocking_queue_push(blocking_queue_t *bq, actor_id_t id) {
    int err;

    blocking_entry_t *new_entry = malloc(sizeof(blocking_entry_t));
    if (new_entry == NULL) return -1;

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
        if ((err = pthread_cond_signal(&bq->ready)) != 0) // if no thread waits then nothing happens
            syserr(err, "cond signal failed");

    bq->len++;

#ifdef DEBUG
    fprintf(stdout,"BQ push: %ld\n", id);
#endif

    safe_unlock(&bq->lock);

    return 0;
}

/// returns -1 if queue was interrupted
int blocking_queue_pop(blocking_queue_t *bq, actor_id_t *actor) {
    int err;
    blocking_entry_t* volatile pop;
    actor_id_t res;

    safe_lock(&bq->lock);

    while (bq->len == 0 && bq->interrupted != 1)
        if ((err = pthread_cond_wait(&bq->ready, &bq->lock)) != 0)
            syserr(err, "cond wait failed");

    if (bq->interrupted == 1) {
        safe_unlock(&bq->lock);
        return -1;
    }

    pop = bq->front;
    res = pop->data;
    bq->front = pop->prev;

    free(pop);

    bq->len--;
#ifdef DEBUG
    fprintf(stdout,"BQ pop: %ld\n", res);
#endif
    safe_unlock(&bq->lock);
    *actor = res;
    return 0;

}

void blocking_queue_signal_all(blocking_queue_t *bq) {
    int err;
    safe_lock(&bq->lock);

    bq->interrupted = 1;

    if ((err = pthread_cond_broadcast(&bq->ready)) != 0)
        syserr(err, "cond broadcast failed");

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
