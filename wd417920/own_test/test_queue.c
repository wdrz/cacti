#include "minunit.h"
#include "../queue.h"
#include "../cacti.h"

#include <stdbool.h>
#include <stdio.h>

int tests_run = 0;

static char *test(int *arr, int n) {
    int i;
    queue_t* kju = queue_init();

    mu_assert("error init", kju != NULL);
    mu_assert("error empty", queue_empty(kju) == 1);

    message_t m;

    for (i = 0; i < n; ++i) {
        m.data = (arr + i);
        mu_assert("error push", queue_push(kju, m) == 0);
    }

    for (i = 0; i < n; ++i) {
        mu_assert("error empty for", queue_empty(kju) == 0);
        mu_assert("error pop", *(int*)queue_pop(kju).data == arr[i]);
    }

    mu_assert("error empty", queue_empty(kju) == 1);
    mu_assert("error destroy", queue_destroy(kju) == 0);
    return 0;
}


static char *test01()
{
    int arr[] = {132, 533, 5, 70, 1202, 123, 332, 12, 1, 324};
    return test(arr, sizeof(arr)/ sizeof(int));
}

static char *test02()
{
    int arr[] = {32};
    return test(arr, sizeof(arr)/ sizeof(int));
}

static char *test03()
{
    int arr[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 213, 1234, 3124,3224, 3214, 1234, 342,2341};
    return test(arr, sizeof(arr)/ sizeof(int));
}

static char *all_tests()
{
    mu_run_test(test01);
    mu_run_test(test02);
    mu_run_test(test03);
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

