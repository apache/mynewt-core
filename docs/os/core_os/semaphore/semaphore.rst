Semaphore
=========

A semaphore is a structure used for gaining exclusive access (much like
a mutex), synchronizing task operations and/or use in a
"producer/consumer" roles. Semaphores like the ones used by the myNewt
OS are called "counting" semaphores as they are allowed to have more
than one token (explained below).

Description
------------

A semaphore is a fairly simple construct consisting of a queue for
waiting tasks and the number of tokens currently owned by the semaphore.
A semaphore can be obtained as long as there are tokens in the
semaphore. Any task can add tokens to the semaphore and any task can
request the semaphore, thereby removing tokens. When creating the
semaphore, the initial number of tokens can be set as well.

When used for exclusive access to a shared resource the semaphore only
needs a single token. In this case, a single task "creates" the
semaphore by calling *os\_sem\_init* with a value of one (1) for the
token. When a task desires exclusive access to the shared resource it
requests the semaphore by calling *os\_sem\_pend*. If there is a token
the requesting task will acquire the semaphore and continue operation.
If no tokens are available the task will be put to sleep until there is
a token. A common "problem" with using a semaphore for exclusive access
is called *priority inversion*. Consider the following scenario: a high
and low priority task both share a resource which is locked using a
semaphore. If the low priority task obtains the semaphore and then the
high priority task requests the semaphore, the high priority task is now
blocked until the low priority task releases the semaphore. Now suppose
that there are tasks between the low priority task and the high priority
task that want to run. These tasks will preempt the low priority task
which owns the semaphore. Thus, the high priority task is blocked
waiting for the low priority task to finish using the semaphore but the
low priority task cannot run since other tasks are running. Thus, the
high priority tasks is "inverted" in priority; in effect running at a
much lower priority as normally it would preempt the other (lower
priority) tasks. If this is an issue a mutex should be used instead of a
semaphore.

Semaphores can also be used for task synchronization. A simple example
of this would be the following. A task creates a semaphore and
initializes it with no tokens. The task then waits on the semaphore, and
since there are no tokens, the task is put to sleep. When other tasks
want to wake up the sleeping task they simply add a token by calling
*os\_sem\_release*. This will cause the sleeping task to wake up
(instantly if no other higher priority tasks want to run).

The other common use of a counting semaphore is in what is commonly
called a "producer/consumer" relationship. The producer adds tokens (by
calling *os\_sem\_release*) and the consumer consumes them by calling
*os\_sem\_pend*. In this relationship, the producer has work for the
consumer to do. Each token added to the semaphore will cause the
consumer to do whatever work is required. A simple example could be the
following: every time a button is pressed there is some work to do (ring
a bell). Each button press causes the producer to add a token. Each
token consumed rings the bell. There will exactly the same number of
bell rings as there are button presses. In other words, each call to
*os\_sem\_pend* subtracts exactly one token and each call to
*os\_sem\_release* adds exactly one token.

Data structures
----------------

.. code:: c

    struct os_sem
    {
        SLIST_HEAD(, os_task) sem_head;     /* chain of waiting tasks */
        uint16_t    _pad;
        uint16_t    sem_tokens;             /* # of tokens */
    };

+---------------+-----------------------------------------------------+
| Element       | Description                                         |
+===============+=====================================================+
| sem\_head     | Queue head for list of tasks waiting on semaphore   |
+---------------+-----------------------------------------------------+
| \_pad         | Padding for alignment                               |
+---------------+-----------------------------------------------------+
| sem\_tokens   | Current number of tokens                            |
+---------------+-----------------------------------------------------+

API
----

.. doxygengroup:: OSSem
    :content-only:


