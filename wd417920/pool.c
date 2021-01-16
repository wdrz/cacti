#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include "err.h"
#include "cacti.h"
#include "pool.h"
#include "messages.h"

typedef struct thread_pool_t {
    pthread_t tid[POOL_SIZE];
} thread_pool;

thread_pool TP;

void worker(void* data) {
    int id = *((int*) data);
    free(data);

    pthread_t tid = pthread_self();
    TP.tid[id] = tid;

    computation c;

    while(1) {
        //safe_lock(&TP->lock); // is it necessary??

        /*while (!COM->pending)
            if ((err = pthread_cond_wait(&TP->ready, &TP->lock)) != 0)
                syserr(err, "cond wait failed");*/

        if (next_computation(&c) == -1) {
            break;
        }


        //safe_unlock(&TP->lock); // is it necessary??




        (*(c.prompt))(c.stateptr, c.message->nbytes, c.message->data);

        computation_ended(c.actor);
    }
}

void init_tp() {
    int i, err;
    pthread_attr_t attr;



    if ((err = pthread_attr_init(&attr)) != 0 )
        syserr(err, "attr_init");

    if ((err = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE)) != 0)
        syserr(err, "setdetachstate");

    TP.pool_working = 1;
    COM.pending = 0;

    // create threads
    for (i = 0; i < POOL_SIZE; ++i) {
        int* id = (int*)safe_malloc(sizeof(int));
        *id = i;

        if ((err = pthread_create(&TP->threads[i], &attr, &worker, id)) != 0) {
            syserr(err, "create");
        }
    }
}

/*
void operate(actor_id_t actor, act_t prompt, void **stateptr, size_t nbytes, void *data) {
    if ((err = pthread_mutex_lock(&TP->lock)) != 0) {
        syserr(err, "lock failed");
    }

    COM.pending = 1;
    COM.actor = actor;
    COM.data = data;
    COM.nbytes = nbytes;
    COM.prompt = prompt;
    COM.stateptr = stateptr;

    if ((err = pthread_cond_signal(&TP->lock)) != 0) {
        syserr(err, "cond signal failed");
    }

    if ((err = pthread_mutex_unlock(&TP->lock)) != 0) {
        syserr(err, "unlock failed");
    }
}*/


int interrupt_all() {
    int i, err;
    TP.pool_working = 0;
    for (i = 0; i < POOL_SIZE; i++) {
        if ((err = pthread_join(TP.tid[i], NULL)) != 0) {
            syserr(err, "join failed");
        }
    }
}

actor_id_t actor_id_self() {
    pthread_t tid = pthread_self();
    int i;
    for (i = 0; i < POOL_SIZE; ++i) {
        if (pthread_equal(tid, TP.tid[i])) {
            return (actor_id_t) i;
        }
    }
    fatal("actor_id_self used incorrectly");
    return -1;
}