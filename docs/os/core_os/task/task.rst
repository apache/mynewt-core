Task
====

A task, along with the scheduler, forms the basis of the Mynewt OS. A
task consists of two basic elements: a task stack and a task function.
The task function is basically a forever loop, waiting for some "event"
to wake it up. There are two methods used to signal a task that it has
work to do: event queues and semaphores (see the appropriate manual
sections for descriptions of these features).

The Mynewt OS is a multi-tasking, preemptive OS. Every task is assigned
a task priority (from 0 to 255), with 0 being the highest priority task.
If a higher priority task than the current task wants to run, the
scheduler preempts the currently running task and switches context to
the higher priority task. This is just a fancy way of saying that the
processor stack pointer now points to the stack of the higher priority
task and the task resumes execution where it left off.

Tasks run to completion unless they are preempted by a higher priority
task. The developer must insure that tasks eventually "sleep"; otherwise
lower priority tasks will never get a chance to run (actually, any task
lower in priority than the task that never sleeps). A task will be put
to sleep in the following cases: it puts itself to sleep using
``os_time_delay()``, it waits on an event queue which is empty or
attempts to obtain a mutex or a semaphore that is currently owned by
another task.

Note that other sections of the manual describe these OS features in
more detail.

Description
-----------

In order to create a task two data structures need to be defined: the
task object (struct os\_task) and its associated stack. Determining the
stack size can be a bit tricky; generally developers should not declare
large local variables on the stack so that task stacks can be of limited
size. However, all applications are different and the developer must
choose the stack size accordingly. NOTE: be careful when declaring your
stack! The stack is in units of ``os_stack_t`` sized elements (generally
32-bits). Looking at the example given below and assuming ``os_stack_t``
is defined to be a 32-bit unsigned value, "my\_task\_stack" will use 256
bytes.

A task must also have an associated "task function". This is the
function that will be called when the task is first run. This task
function should never return!

In order to inform the Mynewt OS of the new task and to have it added to
the scheduler, the ``os_task_init()`` function is called. Once
``os_task_init()`` is called, the task is made ready to run and is added
to the active task list. Note that a task can be initialized (started)
before or after the os has started (i.e. before ``os_start()`` is
called) but must be initialized after the os has been initialized (i.e.
'os\_init' has been called). In most of the examples and current Mynewt
projects, the os is initialized, tasks are initialized, and the the os
is started. Once the os has started, the highest priority task will be
the first task set to run.

Information about a task can be obtained using the
``os_task_info_get_next()`` API. Developers can walk the list of tasks
to obtain information on all created tasks. This information is of type
``os_task_info`` and is described below.

The following is a very simple example showing a single application
task. This task simply toggles an LED at a one second interval.

.. code:: c

    /* Create a simple "project" with a task that blinks a LED every second */

    /* Define task stack and task object */
    #define MY_TASK_PRI         (OS_TASK_PRI_HIGHEST) 
    #define MY_STACK_SIZE       (64) 
    struct os_task my_task; 
    os_stack_t my_task_stack[MY_STACK_SIZE]; 

    /* This is the task function */
    void my_task_func(void *arg) {
        /* Set the led pin as an output */
        hal_gpio_init_out(LED_BLINK_PIN, 1);

        /* The task is a forever loop that does not return */
        while (1) {
            /* Wait one second */ 
            os_time_delay(1000);

            /* Toggle the LED */ 
            hal_gpio_toggle(LED_BLINK_PIN);
        }
    }

    /* This is the main function for the project */
    int main(int argc, char **argv) 
    {

        /* Perform system and package initialization */
        sysinit();

        /* Initialize the task */
        os_task_init(&my_task, "my_task", my_task_func, NULL, MY_TASK_PRIO, 
                     OS_WAIT_FOREVER, my_task_stack, MY_STACK_SIZE);

        /*  Process events from the default event queue.  */
        while (1) {
           os_eventq_run(os_eventq_dflt_get());
        }
        /* main never returns */  
    }

Data structures
---------------

.. code:: c

    /* The highest and lowest task priorities */
    #define OS_TASK_PRI_HIGHEST         (0)
    #define OS_TASK_PRI_LOWEST          (0xff)

    /* Task states */
    typedef enum os_task_state {
        OS_TASK_READY = 1, 
        OS_TASK_SLEEP = 2
    } os_task_state_t;

    /* Task flags */
    #define OS_TASK_FLAG_NO_TIMEOUT     (0x0001U)
    #define OS_TASK_FLAG_SEM_WAIT       (0x0002U)
    #define OS_TASK_FLAG_MUTEX_WAIT     (0x0004U)

    typedef void (*os_task_func_t)(void *);

    #define OS_TASK_MAX_NAME_LEN (32)

.. code:: c

    struct os_task {
        os_stack_t *t_stackptr;
        os_stack_t *t_stacktop;

        uint16_t t_stacksize;
        uint16_t t_flags;

        uint8_t t_taskid;
        uint8_t t_prio;
        uint8_t t_state;
        uint8_t t_pad;

        char *t_name;
        os_task_func_t t_func;
        void *t_arg;

        void *t_obj;

        struct os_sanity_check t_sanity_check; 

        os_time_t t_next_wakeup;
        os_time_t t_run_time;
        uint32_t t_ctx_sw_cnt;

        /* Global list of all tasks, irrespective of run or sleep lists */
        STAILQ_ENTRY(os_task) t_os_task_list;

        /* Used to chain task to either the run or sleep list */ 
        TAILQ_ENTRY(os_task) t_os_list;

        /* Used to chain task to an object such as a semaphore or mutex */
        SLIST_ENTRY(os_task) t_obj_list;
    };

+--------------+----------------+
| **Element**  | **Description* |
|              | *              |
+==============+================+
| t\_stackptr  | Current stack  |
|              | pointer        |
+--------------+----------------+
| t\_stacktop  | The address of |
|              | the top of the |
|              | task stack.    |
|              | The stack      |
|              | grows downward |
+--------------+----------------+
| t\_stacksize | The size of    |
|              | the stack, in  |
|              | units of       |
|              | os\_stack\_t   |
|              | (not bytes!)   |
+--------------+----------------+
| t\_flags     | Task flags     |
|              | (see flag      |
|              | definitions)   |
+--------------+----------------+
| t\_taskid    | A numeric id   |
|              | assigned to    |
|              | each task      |
+--------------+----------------+
| t\_prio      | The priority   |
|              | of the task.   |
|              | The lower the  |
|              | number, the    |
|              | higher the     |
|              | priority       |
+--------------+----------------+
| t\_state     | The task state |
|              | (see state     |
|              | definitions)   |
+--------------+----------------+
| t\_pad       | padding (for   |
|              | alignment)     |
+--------------+----------------+
| t\_name      | Name of task   |
+--------------+----------------+
| t\_func      | Pointer to     |
|              | task function  |
+--------------+----------------+
| t\_obj       | Generic object |
|              | used by        |
|              | mutexes and    |
|              | semaphores     |
|              | when the task  |
|              | is waiting on  |
|              | a mutex or     |
|              | semaphore      |
+--------------+----------------+
| t\_sanity\_c | Sanity task    |
| heck         | data structure |
+--------------+----------------+
| t\_next\_wak | OS time when   |
| eup          | task is next   |
|              | scheduled to   |
|              | wake up        |
+--------------+----------------+
| t\_run\_time | The amount of  |
|              | os time ticks  |
|              | this task has  |
|              | been running   |
+--------------+----------------+
| t\_ctx\_sw\_ | The number of  |
| cnt          | times that     |
|              | this task has  |
|              | been run       |
+--------------+----------------+
| t\_os\_task\ | List pointer   |
| _list        | for global     |
|              | task list. All |
|              | tasks are      |
|              | placed on this |
|              | list           |
+--------------+----------------+
| t\_os\_list  | List pointer   |
|              | used by either |
|              | the active     |
|              | task list or   |
|              | the sleeping   |
|              | task list      |
+--------------+----------------+
| t\_obj\_list | List pointer   |
|              | for tasks      |
|              | waiting on a   |
|              | semaphore or   |
|              | mutex          |
+--------------+----------------+

.. code:: c

    struct os_task_info {
        uint8_t oti_prio;
        uint8_t oti_taskid;
        uint8_t oti_state;
        uint8_t oti_flags;
        uint16_t oti_stkusage;
        uint16_t oti_stksize;
        uint32_t oti_cswcnt;
        uint32_t oti_runtime;
        os_time_t oti_last_checkin;
        os_time_t oti_next_checkin;

        char oti_name[OS_TASK_MAX_NAME_LEN];
    };

+--------------+----------------+
| **Element**  | **Description* |
|              | *              |
+==============+================+
| oti\_prio    | Task priority  |
+--------------+----------------+
| oti\_taskid  | Task id        |
+--------------+----------------+
| oti\_state   | Task state     |
+--------------+----------------+
| oti\_flags   | Task flags     |
+--------------+----------------+
| oti\_stkusag | Amount of      |
| e            | stack used by  |
|              | the task (in   |
|              | os\_stack\_t   |
|              | units)         |
+--------------+----------------+
| oti\_stksize | The size of    |
|              | the stack (in  |
|              | os\_stack\_t   |
|              | units)         |
+--------------+----------------+
| oti\_cswcnt  | The context    |
|              | switch count   |
+--------------+----------------+
| oti\_runtime | The amount of  |
|              | time that the  |
|              | task has run   |
|              | (in os time    |
|              | ticks)         |
+--------------+----------------+
| oti\_last\_c | The time (os   |
| heckin       | time) at which |
|              | this task last |
|              | checked in to  |
|              | the sanity     |
|              | task           |
+--------------+----------------+
| oti\_next\_c | The time (os   |
| heckin       | time) at which |
|              | this task last |
|              | checked in to  |
|              | the sanity     |
|              | task           |
+--------------+----------------+
| oti\_name    | Name of the    |
|              | task           |
+--------------+----------------+

API
-------

.. doxygengroup:: OSTask
    :content-only:
