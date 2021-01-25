#ifndef BLOCKING_QUEUE_H
#define BLOCKING_QUEUE_H

#include <pthread.h>

#include "cacti.h"


typedef struct blocking_entry {
    volatile actor_id_t data;
    struct blocking_entry* volatile prev;
} blocking_entry_t;

typedef struct blocking_queue {
    volatile int len;
    blocking_entry_t* volatile back;
    blocking_entry_t* volatile front;

    pthread_mutex_t lock;
    pthread_cond_t ready;

    volatile int interrupted;
} blocking_queue_t;

extern blocking_queue_t* blocking_queue_init();

extern int blocking_queue_push(blocking_queue_t *bq, actor_id_t id);

/**
 * A blocking function, that waits until the queue bq is non empty and both removes
 * and returns its back element.
 * @param[in] bq        - a queue on which the operation shall be performed,
 * @param[out] actor    - a pointer to element removed from the queue,
 * @return              - 0 if operation was successful, -1 if the queue has been interrupted
 */
extern int blocking_queue_pop(blocking_queue_t *bq, actor_id_t *actor);

extern void blocking_queue_signal_all(blocking_queue_t *bq);

extern int blocking_queue_empty(blocking_queue_t *bq);

extern int blocking_queue_destroy(blocking_queue_t *bq);

#endif //BLOCKING_QUEUE_H
