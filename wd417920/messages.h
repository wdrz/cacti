#ifndef MESSAGES_H
#define MESSAGES_H

#include "cacti.h"

typedef struct computation {
    actor_id_t actor;

    act_t prompt;
    void **stateptr;

    message_t *message;
} computation_t;

extern int init_messages(actor_id_t *actor, role_t *const role);

extern int new_actor(actor_id_t *actor, role_t *const role);

extern int add_message(actor_id_t actor, message_t message);

extern computation_t* next_computation();

extern void computation_ended(actor_id_t actor);

#endif //MESSAGES_H
