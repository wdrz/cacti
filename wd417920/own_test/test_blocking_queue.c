#include "minunit.h"
#include "../cacti.h"
#include "../blocking_queue.h"
#include "../err.h"

#include <stdbool.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

int tests_run = 0;


void *worker1(void* data) {
    blocking_queue_t *bq = data;

    mu_assert("error push", blocking_queue_push(bq, 12) == 0);
    sleep(3);

    mu_assert("error push", blocking_queue_push(bq, 100) == 0);
    sleep(1);

    mu_assert("error push", blocking_queue_push(bq, 65) == 0);
    sleep(1);

    mu_assert("error push", blocking_queue_push(bq, 36) == 0);
    sleep(1);

    mu_assert("error push", blocking_queue_push(bq, 99) == 0);
    sleep(1);

    //blocking_queue_signal_all(bq);
    return 0;
}

void *worker2(void* data) {
    blocking_queue_t *bq = data;
    actor_id_t actor;

    mu_assert("error pop", blocking_queue_pop(bq, &actor) == 0);
    mu_assert("error pop", actor == 12);

    sleep(1);
    mu_assert("error pop", blocking_queue_pop(bq, &actor) == 0);
    mu_assert("error pop", actor == 65);

    mu_assert("error pop", blocking_queue_pop(bq, &actor) == 0);
    mu_assert("error pop", actor == 36);

    return 0;
}

void *worker3(void* data) {
    blocking_queue_t *bq = data;
    actor_id_t actor;

    sleep(1);
    mu_assert("error pop", blocking_queue_pop(bq, &actor) == 0);
    mu_assert("error pop", actor == 100);
    return 0;
}

void *worker4(void* data) {
    blocking_queue_t *bq = data;

    mu_assert("error push", blocking_queue_push(bq, 12) == 0);
    sleep(5);

    blocking_queue_signal_all(bq);
    return 0;
}


void *worker5(void* data) {
    blocking_queue_t *bq = data;
    actor_id_t actor;

    sleep(1);
    if (blocking_queue_pop(bq, &actor) == 0) {
        mu_assert("error pop", actor == 12);
        printf("*");
    } else {
        printf("-");
    }

    return 0;
}


static char *test_concurrent()
{
    int i, err;
    pthread_attr_t attr;

    if ((err = pthread_attr_init(&attr)) != 0 )
        syserr(err, "attr_init");

    if ((err = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE)) != 0)
        syserr(err, "set detach state");

    pthread_t tid[3];
    blocking_queue_t *bq = blocking_queue_init();
    mu_assert("error init", bq != NULL);

    if ((err = pthread_create(&tid[0], &attr, worker1, (void*) bq)) != 0) {
        syserr(err, "create 1");
    }

    if ((err = pthread_create(&tid[1], &attr, worker2, (void*) bq)) != 0) {
        syserr(err, "create 2");
    }

    if ((err = pthread_create(&tid[2], &attr, worker2, (void*) bq)) != 0) {
        syserr(err, "create 3");
    }

    for (i = 0; i < 3; ++i) {
        if ((err = pthread_join(tid[i], NULL)) != 0)
            syserr (err, "join failed");
    }

    if ((err = pthread_attr_destroy (&attr)) != 0)
        syserr (err, "cond destroy failed");

    return 0;
}

static char *test_signal_all()
{
    int i, err;
    pthread_attr_t attr;

    if ((err = pthread_attr_init(&attr)) != 0 )
        syserr(err, "attr_init");

    if ((err = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE)) != 0)
        syserr(err, "set detach state");

    pthread_t tid[11];
    blocking_queue_t *bq = blocking_queue_init();
    mu_assert("error init", bq != NULL);


    if ((err = pthread_create(&tid[10], &attr, worker4, (void*) bq)) != 0) {
        syserr(err, "create");
    }

    for (i = 0; i < 10; i++) {
        if ((err = pthread_create(&tid[i], &attr, worker5, (void*) bq)) != 0) {
            syserr(err, "create");
        }
    }

    for (i = 0; i < 11; ++i) {
        if ((err = pthread_join(tid[i], NULL)) != 0)
            syserr (err, "join failed");
    }

    if ((err = pthread_attr_destroy (&attr)) != 0)
        syserr (err, "cond destroy failed");

    return 0;
}


static char *test_basic() {
    int i;
    blocking_queue_t * kju = blocking_queue_init();

    mu_assert("error init", kju != NULL);
    mu_assert("error empty", blocking_queue_empty(kju) == 1);

    int arr[] = {132, 533, 5, 70, 1202, 123, 332, 12, 1, 324};
    int n = sizeof(arr)/ sizeof(int);

    for (i = 0; i < n; ++i) {
        mu_assert("error push", blocking_queue_push(kju,arr[i]) == 0);
    }

    for (i = 0; i < n; ++i) {
        mu_assert("error empty for", blocking_queue_empty(kju) == 0);
        actor_id_t actor;
        mu_assert("error pop", blocking_queue_pop(kju, &actor) == 0);
        mu_assert("error pop", actor == arr[i]);
    }

    mu_assert("error empty", blocking_queue_empty(kju) == 1);
    mu_assert("error destroy", blocking_queue_destroy(kju) == 0);
    return 0;
}

static char *all_tests()
{
    mu_run_test(test_basic);
    mu_run_test(test_concurrent);
    mu_run_test(test_signal_all);
    return 0;
}

int main()
{
    char *result = all_tests();
    if (result != 0)
    {
        printf(__FILE__ ": %s\n", result);
    }
    else
    {
        printf(__FILE__ ": ALL TESTS PASSED\n");
    }
    printf(__FILE__ ": Tests run: %d\n", tests_run);

    return result != 0;
}

