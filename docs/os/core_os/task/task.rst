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
:c:func:`os_time_delay()`, it waits on an event queue which is empty or
attempts to obtain a mutex or a semaphore that is currently owned by
another task.

Note that other sections of the manual describe these OS features in
more detail.

Description
-----------

In order to create a task two data structures need to be defined: the
task object (struct os_task) and its associated stack. Determining the
stack size can be a bit tricky; generally developers should not declare
large local variables on the stack so that task stacks can be of limited
size. However, all applications are different and the developer must
choose the stack size accordingly. NOTE: be careful when declaring your
stack! The stack is in units of :c:type:`os_stack_t` sized elements (generally
32-bits). Looking at the example given below and assuming ``os_stack_t``
is defined to be a 32-bit unsigned value, "my_task_stack" will use 256
bytes.

A task must also have an associated "task function". This is the
function that will be called when the task is first run. This task
function should never return!

In order to inform the Mynewt OS of the new task and to have it added to
the scheduler, the :c:func:`os_task_init()` function is called. Once
:c:func:`os_task_init()` is called, the task is made ready to run and is added
to the active task list. Note that a task can be initialized (started)
before or after the os has started (i.e. before :c:func:`os_start()` is
called) but must be initialized after the os has been initialized (i.e.
:c:func:`os_init()` has been called). In most of the examples and current Mynewt
projects, the os is initialized, tasks are initialized, and the the os
is started. Once the os has started, the highest priority task will be
the first task set to run.

Information about a task can be obtained using the
:c:func:`os_task_info_get_next()` API. Developers can walk the list of tasks
to obtain information on all created tasks. This information is of type
:c:data:`os_task_info`.

The following is a very simple example showing a single application
task. This task simply toggles an LED at a one second interval.

.. code-block:: c

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

Statically defined tasks
------------------------

Many packages need a task that only processes events from its own event
queue. Package ``util/static_task`` provides
``STATIC_EVENTQ_TASK_DEFINE(name, prio, stack_size)`` that defines such
a task ``name_task``, its stack ``name_stack`` and a statically
initialized event queue ``name_evq``. No task handler or initialization
code is needed: all tasks defined this way are created by
:c:func:`static_task_init()`, which sysinit calls at stage
``STATIC_TASK_SYSINIT_STAGE`` (by default after all other packages are
initialized). Events can be put on ``name_evq`` at any time, also from
earlier sysinit stages; they are processed once the task starts.
Tasks are started in alphabetical order of their names. A package using
the macros adds ``@apache-mynewt-core/util/static_task`` to its
``pkg.deps`` and includes ``static_task/static_task.h``.

``STATIC_TASK_DEFINE(name, prio, stack_size, func, arg)`` does the same for
a task with its own handler and no event queue.
``STATIC_EVENTQ_TASK_DECLARE(name)`` declares the symbols for use in other
files.

.. code-block:: c

    /* foo.h */
    STATIC_EVENTQ_TASK_DECLARE(foo);

    /* foo.c */
    STATIC_EVENTQ_TASK_DEFINE(foo, MYNEWT_VAL(FOO_TASK_PRIO),
                              MYNEWT_VAL(FOO_TASK_STACK_SIZE));

    static void
    foo_event_cb(struct os_event *ev)
    {
        /* Executed in foo_task context */
    }

    static struct os_event foo_event = {
        .ev_cb = foo_event_cb,
    };

    void
    foo_irq_handler(void)
    {
        os_eventq_put(&foo_evq, &foo_event);
    }

The definition is placed in a link table, so it is only linked in when
its object file is pulled from the package library; this is the case
when other code in the same file is referenced. Otherwise set
``pkg.whole_archive: true`` in the package's ``pkg.yml``.


API
----

.. doxygengroup:: OSTask
    :content-only:
    :members:
