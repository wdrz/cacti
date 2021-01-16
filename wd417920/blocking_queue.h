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

extern actor_id_t blocking_queue_pop(blocking_queue_t *bq);

extern void blocking_queue_signal_all(blocking_queue_t *bq);

extern int blocking_queue_empty(blocking_queue_t *bq);

extern int blocking_queue_destroy(blocking_queue_t *bq);

#endif //BLOCKING_QUEUE_H
