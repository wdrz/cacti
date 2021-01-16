#ifndef POOL_H
#define POOL_H

#include "cacti.h"

extern void operate(actor_id_t actor, act_t prompt, void **stateptr, size_t nbytes, void *data);

extern void init_tp();

extern actor_id_t actor_id_self();

#endif //POOL_H
