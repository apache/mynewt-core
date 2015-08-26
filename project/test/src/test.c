#include "os/os.h"
#include "ffs_test.h"
#include "boot_test.h"

#include <stdio.h>
#include <unistd.h>
#include <assert.h>

/* 
 * Test numbers
 *  0 -> eventq testing.
 *  1 to 6 -> mutex testing
 */
#define TEST_NUM   (0)

#define TASK1_PRIO (1) 
#define TASK2_PRIO (2) 
#define TASK3_PRIO (3)

struct os_task task1;
os_stack_t stack1[OS_STACK_ALIGN(1028)]; 

struct os_task task2;
os_stack_t stack2[OS_STACK_ALIGN(1028)];

struct os_task task3;
os_stack_t stack3[OS_STACK_ALIGN(1028)];

/* External functions */
int os_mempool_test(void);
void os_mutex_test(int test_num);
void os_sem_test(int test_num);

struct os_eventq my_evq1;
struct os_eventq my_evq2;
struct os_eventq my_evq3;

#define OS_EVENT_T_STERLY (OS_EVENT_T_PERUSER) 

struct os_event my_osev1 = 
    OS_EVENT_INITIALIZER(my_osev1, OS_EVENT_T_STERLY, NULL);
struct os_event my_osev2 = 
    OS_EVENT_INITIALIZER(my_osev2, OS_EVENT_T_STERLY, NULL);


struct os_callout my_osc1 = 
    OS_CALLOUT_INITIALIZER(&my_osc1);

int verbose;

void 
task1_handler(void *arg)
{
    struct os_task *t; 
    struct os_event *ev;

    while (1) {
        t = os_sched_get_current_task();
        assert(t->t_func == task1_handler);

        os_time_delay(10);

        os_eventq_put(&my_evq2, &my_osev2);

        ev = os_eventq_get(&my_evq1); 
        switch (ev->ev_type) {
        case OS_EVENT_T_STERLY:
            os_task_sanity_checkin(NULL);  
            printf("Pong!\n");
            fflush(stdout);
            os_eventq_put(&my_evq2, &my_osev2);
            break;
        default:
            assert(0);
        }
    }
}

void 
task2_handler(void *arg) 
{
    struct os_task *t;
    struct os_event *ev; 


    while (1) {
        t = os_sched_get_current_task();
        assert(t->t_func == task2_handler);
       
        os_time_delay(10);

        os_eventq_put(&my_evq1, &my_osev1);

        ev = os_eventq_get(&my_evq2); 
        if (ev == NULL) {
            break;
        }
        switch (ev->ev_type) {
        case OS_EVENT_T_STERLY:
            printf("Ping!\n");
            fflush(stdout);
            os_eventq_put(&my_evq1, &my_osev1);
            break;
        default:
            assert(0);
        }
    }
}

void
task3_handler(void *arg)
{
    struct os_task *t;
    struct os_event *ev;

    os_callout_init(&my_osc1);
    while (1) {
        t = os_sched_get_current_task();
        assert(t->t_func == task3_handler);

        os_callout_reset(&my_osc1, 100, &my_evq3, NULL);

        ev = os_eventq_get(&my_evq3);
        switch (ev->ev_type) {
            case OS_EVENT_T_TIMER:
                printf("timer event!: %u\n", os_time_get());
                break;
            default:
                assert(0);
        }
    }
}

static void
usage(int rc)
{
    printf("Usage: test [-hv] [-t test_num]\n");
    printf("\t-h: help\n");
    printf("\t-t: Test number.\n");
    printf("\t-v: verbose mode\n");
    printf("\tEx: test -v -t 1\n");
    exit(rc);
}

int
main(int argc, char **argv)
{
    int test_num;
    int ch;
    char *endptr = NULL;

    os_init();

    /* Perform memory pool tests */
    if (os_mempool_test()) {
        exit(0);
    }

    ffs_test();

    boot_test();

    test_num = 0;

    while ((ch = getopt(argc, argv, "t:v")) != -1) {
        switch (ch) {
            case 'v':
                verbose = 1;
            break;

            case 't':
                test_num = strtoul(optarg, &endptr, 0);
                printf("test_num=%u\n", test_num);
            break;

            case 'h':
            case -1:
            default:
                usage(-1);
            break;
        }
    }

    /* Perform specified test */
    if (test_num == 0) {
        os_eventq_init(&my_evq1);
        os_eventq_init(&my_evq2);
        os_eventq_init(&my_evq3);

        os_task_init(&task1, "task1", task1_handler, NULL, 
                TASK1_PRIO, 10, stack1, OS_STACK_ALIGN(1024));

        os_task_init(&task2, "task2", task2_handler, NULL, 
                TASK2_PRIO, OS_WAIT_FOREVER, stack2, OS_STACK_ALIGN(1024));
        os_task_init(&task3, "task3", task3_handler, NULL, 
                TASK3_PRIO, OS_WAIT_FOREVER, stack3, OS_STACK_ALIGN(1024));
    } else if (test_num < 10) {
        os_mutex_test(test_num);
    } else if (test_num < 20) {
        os_sem_test(test_num);
    } else {
        printf("\nInvalid test number!\n");
        exit(0);
    }

    printf("Starting OS\n");
    fflush(stdout);
    os_start();
}

