#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <signal.h>

#include "err.h"
#include "cacti.h"
#include "messages.h"

// TODO: change SIGQUIT to SIGINT
#define SIG_END         SIGQUIT
#define SIG_INTERRUPT   (SIGRTMIN + 2)



typedef struct thread_pool_t {
    pthread_key_t curr_actor;
    pthread_attr_t attr;        ///< attributes of thread creation
    pthread_t tid[POOL_SIZE];   ///< ids of threads in the pool
    pthread_mutex_t lock;       ///< mutex used to determine the last thread that ends
    int ended;                  ///< number of threads that finished
    pthread_t help_tid;         ///< tid of signal handler
    sigset_t old_mask;          ///< used to restore
    sigset_t set;               ///< blocked signals
} thread_pool;


typedef struct thread_specific {
    actor_id_t actor;
} thread_specific_t;


thread_pool TP; ///< System's thread pool

/**
 * Calls new_actor function and sends MSG_HELLO to the new actor
 * @return 0 on success, -1 on failure
 */
int execute_spawn(message_t message) {
    int err;
    actor_id_t new_actor_id;

    if ((err = new_actor(&new_actor_id, (role_t*)message.data)) != 0) {
        return err;
    }

    //fprintf (stdout, "HELLO MESSAGE SENT: %ld \n", actor_id_self());
    message_t hello_message = {
            .message_type = MSG_HELLO,
            .nbytes = sizeof(actor_id_t),
            .data = (void*) actor_id_self()
    };

    return send_message(new_actor_id, hello_message);
}

/**
 * The life of a special thread (which handles the signal)
 * @param data
 * @return
 */
void *worker_signal(void* data) {
    (void)(data); // suppress unused argument warning
    int err, sig;

    if ((err = sigwait(&TP.set, &sig)) != 0) {
        syserr(err, "sig wait failed");
    }
    fprintf(stdout, "SIG RECEIVED");
    if (sig == SIG_END) {
        interrupt_all();
        fprintf(stdout, "SIG_END RECEIVED");
    }

    return NULL;
}


static void pool_clean() {
    int err;
    if ((err = pthread_key_delete(TP.curr_actor)) != 0)
        syserr(err, "key delete failed");

    if ((err = pthread_mutex_destroy (&TP.lock)) != 0)
        syserr (err, "mutex destroy failed (pool.c)");

    if ((err = pthread_attr_destroy (&TP.attr)) != 0)
        syserr(err, "attr destroy failed");

}

/**
 * The life of a thread.
 * @param data     - number of this thread
 * @return         NULL
 */
void *worker(void* data) {
    int id = *(int*) data;
    int is_last = 0, err;
    free(data);

    pthread_t tid = pthread_self();
    TP.tid[id] = tid;

    // thread specific data
    thread_specific_t *ts = malloc(sizeof(thread_specific_t));
    pthread_setspecific(TP.curr_actor, ts);

    computation_t c;

    while (1) {
        if (next_computation(&c) == -1) {
            break;
        }
        ts->actor = c.actor;

        switch (c.message.message_type) {
            case MSG_SPAWN:
                execute_spawn(c.message); break;

            case MSG_GODIE:
                kill_actor(); break;

            default:
                if (c.prompt != NULL) {
                    (*(c.prompt))(c.stateptr, c.message.nbytes, c.message.data);
                }
        }
        computation_ended(c.actor);
    }
    safe_lock(&TP.lock);
    TP.ended++;
    if (TP.ended == POOL_SIZE) {
        is_last = 1;
    }
    safe_unlock(&TP.lock);

    if (is_last) {
        // last thread, must clean the system
        pool_clean();
        messages_destroy();

        // restore old signal mask
        if ((err = pthread_sigmask(SIG_BLOCK, &TP.old_mask, NULL)) != 0)
            syserr(err, "pthread_sigmask failed");
    }

    free(ts);

    return NULL;
}

void init_tp() {
    int i, err;
    TP.ended    = 0;

    if ((err = pthread_attr_init(&TP.attr)) != 0 )
        syserr(err, "attr_init");

    if ((err = pthread_attr_setdetachstate(&TP.attr, PTHREAD_CREATE_JOINABLE)) != 0)
        syserr(err, "set detach state");

    // set up thread specific struct
    if ((err = pthread_key_create(&TP.curr_actor, NULL)) != 0)
        syserr(err, "key create failed");

    // init mutex
    if ((err = pthread_mutex_init(&TP.lock, 0)) != 0)
        syserr(err, "mutex init failed");

    // block SIGINT in this thread and all the future child threads
    sigemptyset(&TP.set);
    sigaddset(&TP.set, SIG_END);
    sigaddset(&TP.set, SIG_INTERRUPT);
    if ((err = pthread_sigmask(SIG_BLOCK, &TP.set, &TP.old_mask)) != 0)
        syserr(err, "pthread_sigmask failed");

    // create threads
    int* t_num;
    for (i = 0; i < POOL_SIZE; ++i) {
        t_num = safe_malloc(sizeof(int));
        *t_num = i;
        if ((err = pthread_create(&TP.tid[i], &TP.attr, worker, (void*) t_num)) != 0) {
            syserr(err, "create");
        }
    }

    // create a special thread
    if ((err = pthread_create(&TP.help_tid, &TP.attr, worker_signal, NULL)) != 0) {
        syserr(err, "create");
    }


}

actor_id_t actor_id_self() {
    thread_specific_t *tp = pthread_getspecific(TP.curr_actor);
    if (tp == NULL) {
        fatal("actor_id_self used incorrectly");
    }
    return tp->actor;
}

void actor_system_join(actor_id_t actor) {
    (void)(actor); // suppress unused argument warning
    int i, err;

    for (i = 0; i < POOL_SIZE; ++i) {
        if ((err = pthread_join(TP.tid[i], NULL)) != 0)
            syserr(err, "join failed");
    }

    if ((err = pthread_kill(TP.help_tid, SIG_INTERRUPT)) != 0)
        syserr(err, "pthread kill failed");
    //pthread_kill(TP.help_tid, SIG_INTERRUPT);

    fprintf (stdout, "kill sent \n");

    if ((err = pthread_join(TP.help_tid, NULL)) != 0)
        syserr(err, "join failed");

    fprintf (stdout, "join success \n");
}