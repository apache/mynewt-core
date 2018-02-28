Apache Mynewt Operating System Kernel
======================================

.. toctree::
   :hidden:

   context_switch/context_switch
   task/task
   mutex/mutex
   semaphore/semaphore
   event_queue/event_queue
   callout/callout
   heap/heap
   memory_pool/memory_pool
   mbuf/mbuf
   cputime/os_cputime
   time/os_time
   sanity/sanity

The Mynewt Core OS is a multitasking, preemptive real-time operating
system combining a scheduler with typical RTOS features such as mutexes,
semaphores, memory pools, etc. The Mynewt Core OS also provides a number
of useful utilities such as a task watchdog, networking stack memory
buffers and time management API. Each of these features is described in
detail in its own section of the manual.

A multitasking, preemptive operating system is one in which a number of
different tasks can be instantiated and assigned a priority, with higher
priority tasks running before lower priority tasks. Furthermore, if a
lower priority task is running and a higher priority task wants to run,
the lower priority task is halted and the higher priority task is
allowed to run. In other words, the lower priority task is preempted by
the higher priority task.

Why use an OS?
~~~~~~~~~~~~~~

You may ask yourself "why do I need a multitasking preemptive OS"? The
answer may indeed be that you do not. Some applications are simple and
only require a polling loop. Others are more complex and may require
that certain jobs are executed in a timely manner or before other jobs
are executed. If you have a simple polling loop, you cannot move on to
service a job until the current job is done being serviced. With the
Mynewt OS, the application developer need not worry about certain jobs
taking too long or not executing in a timely fashion; the OS provides
mechanisms to deal with these situations. Another benefit of using an OS
is that it helps shield application developers from other application
code being written; the developer does not have to worry (or has to
worry less) about other application code behaving badly and causing
undesirable behavior or preventing their code from executing properly.
Other benefits of using an OS (and the Mynewt OS in particular) is that
it also provides features that the developer would otherwise need to
create on his/her own.

Core OS Features
~~~~~~~~~~~~~~~~

-  :doc:`Scheduler/context switching <context_switch/context_switch>`
-  :doc:`Time <time/os_time>`
-  :doc:`Tasks <task/task>`
-  :doc:`Event queues/callouts <event_queue/event_queue>`
-  :doc:`Semaphores <semaphore/semaphore>`
-  :doc:`Mutexes <mutex/mutex>`
-  :doc:`Memory pools <memory_pool/memory_pool>`
-  :doc:`Heap <heap/heap>`
-  :doc:`Mbufs <mbuf/mbuf>`
-  :doc:`Sanity <sanity/sanity>`
-  :doc:`Callouts <callout/callout>`
-  :doc:`Porting OS to other platforms <porting/port_os>`

Basic OS Application Creation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Creating an application using the Mynewt Core OS is a relatively simple
task. The main steps are:

1. Install the basic Newt Tool structure (build structure) for your
   application.
2. Create your BSP (Board Support Package).
3. In your application ``main()`` function, call the ``sysinit()``
   function to initialize the system and packages, perform application
   specific initialization, then wait and dispatch events from the OS
   default event queue in an infinite loop. (See :doc:`../modules/sysinitconfig/sysinitconfig`
   for more details.)

Initializing application modules and tasks can get somewhat complicated
with RTOS's similar to Mynewt. Care must be taken that the API provided
by a task are initialized prior to being called by another (higher
priority) task.

For example, take a simple application with two tasks (tasks 1 and 2,
with task 1 higher priority than task 2). Task 2 provides an API which
has a semaphore lock and this semaphore is initialized by task 2 when
the task handler for task 2 is called. Task 1 is expected to call this
API.

Consider the sequence of events when the OS is started. The scheduler
starts running and picks the highest priority task (task 1 in this
case). The task handler function for task 1 is called and will keep
running until it yields execution. Before yielding, code in the task 1
handler function calls the API provided by task 2. The semaphore being
accessed in the task 2 API has yet to be initialized since the task 2
handler function has not had a chance to run! This will lead to
undesirable behavior and will need to be addressed by the application
developer. Note that the Mynewt OS does guard against internal API being
called before the OS has started (they will return error) but it does
not safeguard application defined objects from access prior to
initialization.

Example
~~~~~~~

One way to avoid initialization issues like the one described above is
for the application to initialize the objects that are accessed by
multiple tasks before it initializes the tasks with the OS.

The code example shown below illustrates this concept. The application
initializes system and packages, calls an application specific "task
initialization" function, and dispatches events from the default event
queue. The application task initialization function is responsible for
initializing all the data objects that each task exposes to the other
tasks. The tasks themselves are also initialized at this time (by
calling :c:func:`os_task_init()`).

In the example, each task works in a ping-pong like fashion: task 1
wakes up, adds a token to semaphore 1 and then waits for a token from
semaphore 2. Task 2 waits for a token on semaphore 1 and once it gets
it, adds a token to semaphore 2. Notice that the semaphores are
initialized by the application specific task initialization functions
and not inside the task handler functions. If task 2 (being lower in
priority than task 1) had called :c:func:`os_sem_init()` for task2_sem inside
task2_handler(), task 1 would have called :c:func:`os_sem_pend()` using
task2_sem before task2_sem was initialized.

.. code:: c

        struct os_sem task1_sem;
        struct os_sem task2_sem;

        /* Task 1 handler function */
        void
        task1_handler(void *arg)
        {
            while (1) {
                /* Release semaphore to task 2 */
                os_sem_release(&task1_sem);

                /* Wait for semaphore from task 2 */
                os_sem_pend(&task2_sem, OS_TIMEOUT_NEVER);
            }
        }

        /* Task 2 handler function */
        void
        task2_handler(void *arg)
        {
            struct os_task *t;

            while (1) {
                /* Wait for semaphore from task1 */
                os_sem_pend(&task1_sem, OS_TIMEOUT_NEVER);

                /* Release task2 semaphore */
                os_sem_release(&task2_sem);
            }
        }


        /* Initialize task 1 exposed data objects */
        void
        task1_init(void)
        {
            /* Initialize task1 semaphore */
            os_sem_init(&task1_sem, 0);
        }

        /* Initialize task 2 exposed data objects */
        void
        task2_init(void)
        {
            /* Initialize task1 semaphore */
            os_sem_init(&task2_sem, 0);
        }

        /**
         * init_app_tasks
         *
         * This function performs initializations that are required before tasks run.
         *
         * @return int 0 success; error otherwise.
         */
        static int
        init_app_tasks(void)
        {
            /*
             * Call task specific initialization functions to initialize any shared objects
             * before initializing the tasks with the OS.
             */
            task1_init();
            task2_init();

            /*
             * Initialize tasks 1 and 2 with the OS.
             */
            os_task_init(&task1, "task1", task1_handler, NULL, TASK1_PRIO,
                         OS_WAIT_FOREVER, task1_stack, TASK1_STACK_SIZE);

            os_task_init(&task2, "task2", task2_handler, NULL, TASK2_PRIO,
                         OS_WAIT_FOREVER, task2_stack, TASK2_STACK_SIZE);

            return 0;
        }

        /**
         * main
         *
         * The main function for the application. This function initializes the system and packages,
         * calls the application specific task initialization function, then waits and dispatches
         * events from the OS default event queue in an infinite loop.
         */
        int
        main(int argc, char **arg)
        {

            /* Perform system and package initialization */
            sysinit();

            /* Initialize application specific tasks */
            init_app_tasks();

            while (1) {
               os_eventq_run(os_eventq_dflt_get());
            }
            /* main never returns */
    }
