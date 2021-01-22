#ifndef BLOCKING_QUEUE_H
#define BLOCKING_QUEUE_H

#include <pthread.h>

#include "cacti.h"


typedef struct blocking_entry {
    actor_id_t data;
    blocking_entry *prev;
} blocking_entry_t;

typedef struct blocking_queue {
    int len;
    blocking_entry_t *back;
    blocking_entry_t *front;

    pthread_mutex_t lock;
    pthread_cond_t ready;

    int interrupted;
} blocking_queue_t;

extern blocking_queue_t* blocking_queue_init();

extern void blocking_queue_push(blocking_queue_t *bq, actor_id_t id);

/** TODO: THIS SIGNATURE HAS CHANGED -- MODIFY FUNCTION IS SOURCE FILE !!
 * A blocking function, that waits until the queue bq is non empty and both removes
 * and returns its back element.
 * @param[in] bq        - a queue on which the operation shall be performed,
 * @param[out] actor    - a pointer to element removed from the queue,
 * @return              - 0 if operation was successful, -1 if there are no elements in this queue
 *                        neither now nor in the future.
 */
extern int blocking_queue_pop(blocking_queue_t *bq, actor_id_t *actor);

extern void blocking_queue_signal_all(blocking_queue_t *bq);

extern int blocking_queue_empty(blocking_queue_t *bq);

extern int blocking_queue_destroy(blocking_queue_t *bq);

#endif //BLOCKING_QUEUE_H
