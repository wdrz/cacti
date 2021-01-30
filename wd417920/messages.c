#include <stdlib.h>
#include <pthread.h>
#include <string.h>

#include "messages.h"
#include "err.h"
#include "queue.h"
#include "blocking_queue.h"

//#define DEBUG 1

/**
 * A representation of a single actor
 */
typedef struct actor {
    role_t *role;                ///< array of callbacks
    queue_t* volatile messages;  ///< queue of messages
    pthread_mutex_t lock;        ///< a mutex associated with this actor
    volatile int processed_now;  ///< 0 if an actor callback has been invoked on a thread
    void *stateptr;              ///< a state of an actor
    volatile int goodbye;        ///< 1 if actor has processed or its queue of messages contains MSQ_GODIE, 0 o/w
} actor_t;

/**
 * A representation of system of actors
 */
typedef struct actors {
    pthread_mutex_t lock;                    ///< a mutex associated with the system
    volatile int num;                        ///< number of actors in the system
    actor_t** volatile actors;               ///< dynamic array of pointers to actors
    volatile int capacity;                   ///< space allocated for actors array
    blocking_queue_t* volatile waiting;      ///< a blocking queue of actors that have pending messages
    volatile int n_processed_now;            ///< number of actors processed by the thread pool at the moment
    volatile int n_finished;                 ///< number actors that processed MSG_GODIE and all messages after it
    volatile int interrupted;                ///< 1 if system was interrupted, 0 o/w

} actors_t;

static actors_t AC; ///< The system of actors

/**
 * Allocates memory for a new actor and initializes it
 * @param role      an array of callbacks
 * @return          pointer to a new actor
 */
static actor_t* generate_actor(role_t *const role) {
    int err;

    actor_t* created_actor = safe_malloc(sizeof(actor_t));

    created_actor->role          = role;
    created_actor->messages      = queue_init();
    created_actor->processed_now = 0;
    created_actor->goodbye       = 0;
    created_actor->stateptr      = NULL;

    if ((err = pthread_mutex_init(&created_actor->lock, 0)) != 0)
        syserr(err, "mutex init failed");

    return created_actor;
}

/**
 * Initiates the system of actors.
 * @param actor     output parameter, assigns an id of first actor in the system
 * @param role      array of callbacks for the first actor in the system
 * @return          0 if operation is successful, -1 o/w
 */
int init_actors_system(actor_id_t *actor, role_t *const role) {
    int err;
    actor_id_t id_first = 0;

    AC.num              = 1;
    AC.n_processed_now  = 0;
    AC.n_finished       = 0;
    AC.interrupted      = 0;
    AC.capacity         = 4;
    AC.waiting          = blocking_queue_init();
    AC.actors           = safe_malloc(AC.capacity * sizeof(actor_t*));
    AC.actors[0]        = generate_actor(role);
    *actor              = id_first;

    if ((err = pthread_mutex_init(&AC.lock, 0)) != 0)
        syserr(err, "mutex init failed");

    // implicitly send hello message

    message_t hello_message = { .message_type = MSG_HELLO };
    return send_message(id_first, hello_message);
}

/** TODO: change desc
 * Creates a new actor and adds it to the running system.
 * Sends MSG_HELLO to this new actor.
 * @param role     - array of callbacks for the new actor,
 * @return           0 on success, -1 on failure.
 */
void execute_spawn(void **stateptr, size_t nbytes, void *data) {
    (void)(stateptr); // suppress unused argument warning
    (void)(nbytes);  // suppress unused argument warning

    role_t *role = data;
    safe_lock(&AC.lock);

    if (AC.capacity == AC.num) {
        AC.capacity      *= 2;
        AC.actors         = realloc(AC.actors, AC.capacity * sizeof(actor_t*));
    }

    if (AC.actors == NULL) {
        fatal("Realloc failed");
    }

    AC.actors[AC.num]     = generate_actor(role);
    actor_id_t actor      = AC.num++;

    safe_unlock(&AC.lock);

    if (send_message(actor, (message_t){
            .message_type = MSG_HELLO,
            .nbytes = sizeof(actor_id_t),
            .data = (void*) actor_id_self()
    }) != 0) {
        fatal("send message HELLO failed");
    }
}

/**
 * Sends message to an actor.
 * @param actor         receiver
 * @param message       message
 * @return              -2 if actor is incorrect, -1 if actor does not accept messages, 0 o/w
 */
int send_message(actor_id_t actor, message_t message) {
    int add_to_queue;
    safe_lock(&AC.lock); // system lock

#ifdef DEBUG
    fprintf(stdout, "\033[0;31msend_message to %ld (%ld) \033[0m \n", actor, message.message_type);
#endif

    if (AC.interrupted) { // check if system has been interrupted
        safe_unlock(&AC.lock); // system unlock
        return -1;
    }

    if (actor >= AC.num || actor < 0) { // check if actor's id is correct
        safe_unlock(&AC.lock); // system unlock
        return -2;
    }

    actor_t* actor_temp = AC.actors[actor];
    safe_unlock(&AC.lock); // system unlock


    safe_lock(&actor_temp->lock); // actor lock

    if (actor_temp->goodbye == 1) { // check if actor has processed MSG_GODIE
        safe_unlock(&actor_temp->lock); // actor unlock
        return -1;
    }

    add_to_queue = !actor_temp->processed_now
            && queue_empty(actor_temp->messages);
    queue_push(actor_temp->messages, message);

    safe_unlock(&actor_temp->lock); // actor unlock

    if (add_to_queue) {
        blocking_queue_push(AC.waiting, actor); // this queue is synchronised
    }

    return 0;
}

/**
 * This function is called by a thread from the pool, that has finished processing a callback.
 * Assumes that the parameter is correct
 * @param actor    - id of an actor that was being processed by a calling thread up until now
 */
void computation_ended(actor_id_t actor) {
    int messages_pending;

    safe_lock(&AC.lock); // system lock

#ifdef DEBUG
    fprintf(stdout, "computation_ended %ld \n", actor);
#endif

    actor_t* actor_temp = AC.actors[actor];
    AC.n_processed_now--;
    safe_unlock(&AC.lock); // system unlock

    safe_lock(&actor_temp->lock); // actor lock
    actor_temp->processed_now = 0;

    messages_pending = !queue_empty(actor_temp->messages);

    // checks if an actor has finished its life
    if (!messages_pending && actor_temp->goodbye) {
        AC.n_finished++;

        if (AC.n_finished == AC.num) {
            blocking_queue_signal_all(AC.waiting);
        }

    }

    safe_unlock(&actor_temp->lock); // actor unlock

    // check if actor has pending messages and if so, add it to waiting queue
    if (messages_pending) {
        blocking_queue_push(AC.waiting, actor); // this queue is synchronised
    }

}

/**
 * A blocking function that waits until a computation is available and returns it.
 * @param[out] result    - pointer to a computation that must be handled by a calling thread
 * @return               - 0 if a next computation has been returned, -1 if all actors are done.
 */
int next_computation(computation_t *result) {
    actor_id_t actor_id;
    if (blocking_queue_pop(AC.waiting, &actor_id) != 0) { // blocking instruction
        return -1;
    }

    // Computations shall continue

#ifdef DEBUG
    fprintf(stdout, "next_computation continues: %ld \n", actor_id);
#endif
    actor_t *actor_temp = AC.actors[actor_id]; // does not need to be synchronised

    safe_lock(&actor_temp->lock); // actor lock
    message_t message = queue_pop(actor_temp->messages);
    size_t mt = message.message_type;

    if (mt == MSG_GODIE) {
        actor_temp->goodbye = 1;

    } else if (mt >= actor_temp->role->nprompts && mt != MSG_SPAWN) {
        fatal("message_type out of range");
    }


    computation_t result_cpy = {
            .prompt     = (mt == MSG_SPAWN) ? execute_spawn : ((mt == MSG_GODIE)
                    ? NULL : actor_temp->role->prompts[ message.message_type ]),
            .actor      = actor_id,
            .stateptr   = &actor_temp->stateptr,
            .message    = message
    };
    memcpy(result, &result_cpy, sizeof(computation_t));

    actor_temp->processed_now = 1;
    safe_unlock(&actor_temp->lock); // actor unlock

    safe_lock(&AC.lock); // system lock
    AC.n_processed_now++;
    safe_unlock(&AC.lock); // system unlock
    return 0;
}

/**
 * Marks all system of actors as if all actors do not accept signals anymore
 * which causes threads to end as soon as they finish currently executed callback.
 */
void interrupt_all() {
    safe_lock(&AC.lock); // system lock
    AC.interrupted = 1;
    blocking_queue_signal_all(AC.waiting);
    safe_unlock(&AC.lock); // system unlock
}

/**
 * Deallocates everything it allocated. Should be run only after all actors are done.
 * @return
 */
int messages_destroy() {
    int err, i;

    // destroy main mutex
    if ((err = pthread_mutex_destroy(&AC.lock)) != 0)
        syserr(err, "mutex destroy failed");

    // destroy major queue
    blocking_queue_destroy(AC.waiting);

    // destroy minor mutexes and queues associated with actors
    for (i = 0; i < AC.num; ++i) {
        queue_destroy(AC.actors[i]->messages);
        if ((err = pthread_mutex_destroy(&AC.actors[i]->lock)) != 0) {
            syserr(err, "mutex destroy failed");
        }
        free(AC.actors[i]);
    }

    // destroy the list of actors
    free(AC.actors);

    return 0;
}