#ifndef MESSAGES_H
#define MESSAGES_H

#include "cacti.h"

typedef struct computation {
    actor_id_t actor;

    act_t prompt;
    void ** stateptr;

    message_t message;
} computation_t;

extern int init_actors_system(actor_id_t *actor, role_t *role);

extern int new_actor(actor_id_t *actor, role_t *role);

extern int send_message(actor_id_t actor, message_t message);

extern int next_computation(computation_t* c);

extern void computation_ended(actor_id_t actor);

extern int kill_actor();

extern int messages_destroy();

#endif //MESSAGES_H
