#include "os.h"

#include <stdio.h>
#include <assert.h>


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


void 
task1_handler(void *arg)
{
    struct os_task *t; 
    struct os_event *ev;

    while (1) {
        t = os_sched_get_current_task();
        assert(t->t_func == task1_handler);

        os_sched_sleep(t, 10);

        os_eventq_put(&my_evq2, &my_osev2);

        ev = os_eventq_get(&my_evq1); 
        switch (ev->ev_type) {
        case OS_EVENT_T_STERLY:
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
       
        os_sched_sleep(t, 10);

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


int
main(int argc, char **argv)
{
    os_init();

    /* Perform memory pool tests */
    if (os_mempool_test()) {
        exit(0);
    }

    os_eventq_init(&my_evq1);
    os_eventq_init(&my_evq2);
    os_eventq_init(&my_evq3);
    os_task_init(&task1, "task1", task1_handler, NULL, 
            TASK1_PRIO, stack1, OS_STACK_ALIGN(1024));

    os_task_init(&task2, "task2", task2_handler, NULL, 
            TASK2_PRIO, stack2, OS_STACK_ALIGN(1024));
    os_task_init(&task3, "task3", task3_handler, NULL, 
            TASK3_PRIO, stack3, OS_STACK_ALIGN(1024));

    os_start();
}
