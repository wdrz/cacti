#include <stdlib.h>
#include <pthread.h>
#include <string.h>

#include "messages.h"
#include "err.h"
#include "queue.h"
#include "blocking_queue.h"

/**
 * A representation of a single actor
 */
typedef struct actor {
    role_t *role;          ///< array of callbacks
    queue_t* volatile messages;           ///< queue of messages
    pthread_mutex_t lock;        ///< a mutex associated with this actor

    int processed_now;           ///< 0 if an actor callback has been invoked on a thread
    void ** stateptr;             ///< a state of an actor

    //volatile int dead;         ///< 1 if actor processed MSG_GODIE, 0 o/w
    volatile int goodbye;        ///< 1 if actor is dead or its queue of messages contains MSQ_GODIE, 0 o/w
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
    volatile int n_dead;                     ///< number of dead actors
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
    //created_actor->dead          = 0;
    created_actor->stateptr      = malloc(sizeof(void*));

    if ((err = pthread_mutex_init(&created_actor->lock, 0)) != 0)
        syserr(err, "mutex init failed");

    return created_actor;
}

/**
 * Initiates system of actors.
 * @param actor     output parameter, assigns an id of first actor in the system
 * @param role      array of callbacks for the first actor in the system
 * @return          0 if operation is successful, -1 o/w
 */
int init_actors_system(actor_id_t *actor, role_t *const role) {
    int err;
    AC.num              = 1;
    AC.n_processed_now  = 0;
    AC.n_dead           = 0;
    AC.capacity         = 4;
    AC.waiting          = blocking_queue_init();
    AC.actors           = malloc(AC.capacity * sizeof(actor_t*));
    AC.actors[0]        = generate_actor(role);
    *actor              = 0;

    blocking_queue_push(AC.waiting, 0);

    if ((err = pthread_mutex_init(&AC.lock, 0)) != 0)
        syserr(err, "mutex init failed");

    return 0;
}

/**
 * Creates a new actor and adds it to the running system
 * @param actor     output parameter, assigns an id of the new actor
 * @param role      array of callbacks for new actor
 * @return          0 if operation is successful, -1 o/w
 */
int new_actor(actor_id_t *actor, role_t *const role) {
    safe_lock(&AC.lock);

    if (AC.capacity == AC.num) {
        AC.capacity      *= 2;
        AC.actors         = realloc(AC.actors, AC.capacity * sizeof(actor_t*));
    }

    AC.actors[AC.num]     = generate_actor(role);
    *actor                = AC.num++;

    safe_unlock(&AC.lock);
    return 0;
}

/**
 * Checks if actor has been queued for access to the thread pool.
 * @param actor_temp    a pointer to actor id (input parameter)
 * @return              0 if actor is not processed right now, but does have some pending messages
 */
static inline int is_actor_queued(actor_t* actor_temp) {
    return actor_temp->processed_now == 0 && queue_empty(actor_temp->messages) == 0;
}

/**
 * Sends message to an actor.
 * @param actor         receiver
 * @param message       message
 * @return              -2 if actor is incorrect, -1 if actor does not accept messages, 0 o/w
 */
int send_message(actor_id_t actor, message_t message) {
    safe_lock(&AC.lock); // system lock
    if (actor >= AC.num || actor < 0) {
        safe_unlock(&AC.lock); // system unlock
        return -2;
    }
    actor_t* actor_temp = AC.actors[actor];
    safe_unlock(&AC.lock); // system unlock

    safe_lock(&actor_temp->lock); // actor lock

    if (actor_temp->goodbye == 1) {
        safe_unlock(&actor_temp->lock); // actor unlock
        return -1;
    }

    queue_push(actor_temp->messages, message);

    if (!is_actor_queued(actor_temp)) {
        blocking_queue_push(AC.waiting, actor); // this queue is synchronised
    }

    if (message.message_type == MSG_GODIE) {
        actor_temp->goodbye = 1;
    }

    safe_unlock(&actor_temp->lock); // actor unlock
    return 0;
}

/**
 * TODO: NOT SURE IF NECESSARY !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * @return TODO: modify this function
 */
/*static int check_if_actors_finished_n_handle() {
    safe_lock(&AC.lock);
    if (AC.num == AC.n_dead) {
        blocking_queue_signal_all(AC.waiting);

        safe_unlock(&AC.lock);
        return 1;
    }
    safe_unlock(&AC.lock);
    return 0;
}*/


/**
 * A blocking function that waits until a computation is available and returns it.
 * @param[out] res           - pointer to a computation that must be handled by a calling thread
 * @param[in] first          - 1 if calling thread has not performed any computation before, 0 o/w
 * @param[in] prev_actor_id  - id of an actor that was being processed by a calling thread up until now
 * @return                   - 0 if a computation has been returned, -1 if all actors are done
 */
/*int next_computation_temp(computation_t *res, int first, actor_id_t prev_actor_id) {

    if (first == 0) {
        safe_lock(&AC->lock); // system lock
        actor_t* prev_actor = AC.actors[prev_actor_id];
        safe_unlock(&AC->lock); // system unlock

        safe_lock(&(prev_actor->lock));

        prev_actor->processed_now = 0;
        if (!is_actor_queued(prev_actor)) {
            blocking_queue_push(AC.waiting, actor); // this queue is synchronised
        }
        safe_unlock(&(actor_temp->lock));

    }
}*/



/**
 * This function is called by a thread from the pool, that has finished processing a callback.
 * Assumes that the parameter is correct
 * @param actor    - id of an actor that was being processed by a calling thread up until now
 */
void computation_ended(actor_id_t actor) {
    int is_queued;

    safe_lock(&AC.lock); // system lock
    actor_t* actor_temp = AC.actors[actor];
    AC.n_processed_now--;
    safe_unlock(&AC.lock); // system unlock

    safe_lock(&actor_temp->lock); // actor lock
    is_queued = is_actor_queued(actor_temp);
    actor_temp->processed_now = 0;
    safe_unlock(&(actor_temp->lock)); // actor unlock

    if (!is_queued) {
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

    actor_t *actor_temp = AC.actors[actor_id];

    safe_lock(&actor_temp->lock); // actor lock

    computation_t result_cpy = {
            .prompt = actor_temp->role->prompts[ result->message.message_type ],
            .actor = actor_id,
            .stateptr = actor_temp->stateptr,
            .message = queue_pop(actor_temp->messages)
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
 *
 * @param actor_id
 * @return             -1 if all actors are dead, 0 o/w.
 */
int kill_actor() {
    safe_lock(&AC.lock); // system lock
    AC.n_dead++;
    if (AC.n_dead == AC.num) {
        blocking_queue_signal_all(AC.waiting);
        safe_unlock(&AC.lock); // system unlock
        return -1;
    }

    safe_unlock(&AC.lock); // system unlock
    return 0;
}

int messages_destroy() {
    int err, i;

    if ((err = pthread_mutex_destroy(&AC.lock)) != 0)
        syserr(err, "mutex destroy failed");

    blocking_queue_destroy(AC.waiting);

    for (i = 0; i < AC.num; ++i) {
        queue_destroy(AC.actors[i]->messages);
        if ((err = pthread_mutex_destroy(&AC.actors[i]->lock)) != 0)
            syserr(err, "mutex destroy failed");

        free(AC.actors[i]);
    }

    return 0;
}