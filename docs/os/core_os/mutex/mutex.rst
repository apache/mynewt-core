Mutex
=====

Mutex is short for "mutual exclusion"; a mutex provides mutually
exclusive access to a shared resource. A mutex provides *priority
inheritance* in order to prevent *priority inversion*. Priority
inversion occurs when a higher priority task is waiting on a resource
owned by a lower priority task. Using a mutex, the lower priority task
will inherit the highest priority of any task waiting on the mutex.

Description
-------------

The first order of business when using a mutex is to declare the mutex
globally. The mutex needs to be initialized before it is used (see the
examples). It is generally a good idea to initialize the mutex before
tasks start running in order to avoid a task possibly using the mutex
before it is initialized.

When a task wants exclusive access to a shared resource it needs to
obtain the mutex by calling *os\_mutex\_pend*. If the mutex is currently
owned by a different task (a lower priority task), the requesting task
will be put to sleep and the owners priority will be elevated to the
priority of the requesting task. Note that multiple tasks can request
ownership and the current owner is elevated to the highest priority of
any task waitin on the mutex. When the task is done using the shared
resource, it needs to release the mutex by called *os\_mutex\_release*.
There needs to be one release per call to pend. Note that nested calls
to *os\_mutex\_pend* are allowed but there needs to be one release per
pend.

The following example will illustrate how priority inheritance works. In
this example, the task number is the same as its priority. Remember that
the lower the number, the higher the priority (i.e. priority 0 is higher
priority than priority 1). Suppose that task 5 gets ownership of a mutex
but is preempted by task 4. Task 4 attempts to gain ownership of the
mutex but cannot as it is owned by task 5. Task 4 is put to sleep and
task 5 is temporarily raised to priority 4. Before task 5 can release
the mutex, task 3 runs and attempts to acquire the mutex. At this point,
both task 3 and task 4 are waiting on the mutex (sleeping). Task 5 now
runs at priority 3 (the highest priority of all the tasks waiting on the
mutex). When task 5 finally releases the mutex it will be preempted as
two higher priority tasks are waiting for it.

Note that when multiple tasks are waiting on a mutex owned by another
task, once the mutex is released the highest priority task waiting on
the mutex is run.

Data structures
----------------

.. code:: c

    struct os_mutex
    {
        SLIST_HEAD(, os_task) mu_head;
        uint8_t     _pad;
        uint8_t     mu_prio;
        uint16_t    mu_level;
        struct os_task *mu_owner;
    };

+--------------+----------------+
| Element      | Description    |
+==============+================+
| mu\_head     | Queue head for |
|              | list of tasks  |
|              | waiting on     |
|              | mutex          |
+--------------+----------------+
| \_pad        | Padding        |
+--------------+----------------+
| mu\_prio     | Default        |
|              | priority of    |
|              | owner of       |
|              | mutex. Used to |
|              | reset priority |
|              | of task when   |
|              | mutex released |
+--------------+----------------+
| mu\_level    | Call nesting   |
|              | level (for     |
|              | nested calls)  |
+--------------+----------------+
| mu\_owner    | Pointer to     |
|              | task structure |
|              | which owns     |
|              | mutex          |
+--------------+----------------+

API
----

.. doxygengroup:: OSMutex
    :content-only:


