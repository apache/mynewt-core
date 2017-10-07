Sanity
======

The Sanity task is a software watchdog task, which runs periodically to
check system state, and ensure that everything is still operating
properly.

In a typical system design, there are multiple stages of watchdog:

-  Internal Watchdog

-  External Watchdog

-  Sanity Watchdog

The *Internal Watchdog* is typically an MCU watchdog, which is tickled
in the core of the OS. The internal watchdog is tickled frequently, and
is meant to be an indicator the OS is running.

The *External Watchdog* is a watchdog that's typically run slower. The
purpose of an external watchdog is to provide the system with a hard
reset when it has lost its mind.

The *Sanity Watchdog* is the least frequently run watchdog, and is meant
as an application watchdog.

This document is about the operation of the Mynewt Sanity Watchdog.

Description
-----------

Sanity Task
-----------

Mynewt OS uses the OS Idle task to check sanity. The ``SANITY_INTERVAL``
syscfg setting specifies the interval in seconds to perform the sanity
checks.

By default, every operating system task provides the frequency it will
check in with the sanity task, with the ``sanity_itvl`` parameter in the
``os_task_init()`` function:

.. code:: c

    int os_task_init(struct os_task *t, char *name, os_task_func_t func, 
        void *arg, uint8_t prio, os_time_t sanity_itvl, os_stack_t *bottom,
        uint16_t stack_size);

``sanity_itvl`` is the time in OS time ticks that the task being created
must register in with the sanity task.

Checking in with Sanity Task
-----------------------------

The task must then register in with the sanity task every
``sanity_itvl`` seconds. In order to do that, the task should call the
``os_sanity_task_checkin`` function, which will reset the sanity check
associated with this task. Here is an example of a task that uses a
callout to checkin with the sanity task every 50 seconds:

.. code:: c

    #define TASK1_SANITY_CHECKIN_ITVL (50 * OS_TICKS_PER_SEC) 
    struct os_eventq task1_evq;

    static void
    task1(void *arg)
    {
        struct os_task *t;
        struct os_event *ev;
        struct os_callout c;
        
        /* Get current OS task */
        t = os_sched_get_current_task();

        /* Initialize the event queue. */
        os_eventq_init(&task1_evq);

        /* Initialize the callout */
        os_callout_init(&c, &task1_evq, NULL);

        /* reset the callout to checkin with the sanity task 
         * in 50 seconds to kick off timing.
         */
        os_callout_reset(&c, TASK1_SANITY_CHECKIN_ITVL);

        while (1) {
            ev = os_eventq_get(&task1_evq);

            /* The sanity timer has reset */
            if (ev->ev_arg == &c) {
                os_sanity_task_checkin(t);
            } else {
                /* not expecting any other events */
                assert(0);
            }
        }
        
        /* Should never reach */
        assert(0);
    }

Registering a Custom Sanity Check
-----------------------------------

If a particular task wants to further hook into the sanity framework to
perform other checks during the sanity task's operation, it can do so by
registering a ``struct os_sanity_check`` using the
``os_sanity_check_register`` function.

.. code:: c

    static int 
    mymodule_perform_sanity_check(struct os_sanity_check *sc, void *arg)
    {
        /* Perform your checking here.  In this case, we check if there 
         * are available buffers in mymodule, and return 0 (all good)
         * if true, and -1 (error) if not.
         */
        if (mymodule_has_buffers()) {
            return (0);
        } else {
            return (-1);
        }
    }

    static int 
    mymodule_register_sanity_check(void)
    {
        struct os_sanity_check sc;

        os_sanity_check_init(&sc);
        /* Only assert() if mymodule_perform_sanity_check() fails 50 
         * times.  SANITY_TASK_INTERVAL is defined by the user, and 
         * is the frequency at which the sanity_task runs in seconds.
         */
        OS_SANITY_CHECK_SETFUNC(&sc, mymodule_perform_sanity_check, NULL, 
            50 * SANITY_TASK_INTERVAL);

        rc = os_sanity_check_register(&sc);
        if (rc != 0) {
            goto err;
        } 

        return (0);
    err:
        return (rc);
    }

In the above example, every time the custom sanity check
``mymodule_perform_sanity_check`` returns successfully (0), the sanity
check is reset. In the ``OS_SANITY_CHECK_SETFUNC`` macro, the sanity
checkin interval is specified as 50 \* SANITY\_TASK\_INTERVAL (which is
the interval at which the sanity task runs.) This means that the
``mymodule_perform_sanity_check()`` function needs to fail 50 times
consecutively before the sanity task will crash the system.

**TIP:** When checking things like memory buffers, which can be
temporarily be exhausted, it's a good idea to have the sanity check fail
multiple consecutive times before crashing the system. This will avoid
crashing for temporary failures.

Data structures
---------------

OS Sanity Check
----------------

.. code:: c

    struct os_sanity_check {
        os_time_t sc_checkin_last;
        os_time_t sc_checkin_itvl;
        os_sanity_check_func_t sc_func;
        void *sc_arg; 

        SLIST_ENTRY(os_sanity_check) sc_next;
    };

+--------------+----------------+
| **Element**  | **Description* |
|              | *              |
+==============+================+
| ``sc_checkin | The last time  |
| _last``      | this sanity    |
|              | check checked  |
|              | in with the    |
|              | sanity task,   |
|              | in OS time     |
|              | ticks.         |
+--------------+----------------+
| ``sc_checkin | How frequently |
| _itvl``      | the sanity     |
|              | check is       |
|              | supposed to    |
|              | check in with  |
|              | the sanity     |
|              | task, in OS    |
|              | time ticks.    |
+--------------+----------------+
| ``sc_func``  | If not         |
|              | ``NULL``, call |
|              | this function  |
|              | when running   |
|              | the sanity     |
|              | task. If the   |
|              | function       |
|              | returns 0,     |
|              | reset the      |
|              | sanity check.  |
+--------------+----------------+
| ``sc_arg``   | Argument to    |
|              | pass to        |
|              | ``sc_func``    |
|              | when calling   |
|              | it.            |
+--------------+----------------+
| ``sc_next``  | Sanity checks  |
|              | are chained in |
|              | the sanity     |
|              | task when      |
|              | ``os_sanity_ch |
|              | eck_register() |
|              | ``             |
|              | is called.     |
+--------------+----------------+

API
-----------------

.. doxygengroup:: OSSanity
    :content-only:
