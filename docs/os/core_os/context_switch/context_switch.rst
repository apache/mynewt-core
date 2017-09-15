Scheduler/Context Switching
===========================

.. toctree::
   :caption: Functions
   :maxdepth: 1
   :glob:

   os_*


Scheduler's job is to maintain the list of tasks and decide which one
should be running next.

Description
-----------

Task states can be *running*, *ready to run* or *sleeping*.

When task is *running*, CPU is executing in that task's context. The
program counter (PC) is pointing to instructions task wants to execute
and stack pointer (SP) is pointing to task's stack.

Task which is *ready to run* wants to get on the CPU to do its work.

Task which is *sleeping* has no more work to do. It's waiting for
someone else to wake it up.

Scheduler algorithm is simple: from among the tasks which are ready to
run, pick the the one with highest priority (lowest numeric value in
task's t\_prio field), and make its state *running*.

Tasks which are either *running* or *ready to run* are kept in linked
list ``g_os_run_list``. This list is ordered by priority.

Tasks which are *sleeping* are kept in linked list ``g_os_sleep_list``.

Scheduler has a CPU architecture specific component; this code is
responsible for swapping in the task which should be *running*. This
process is called context switch. During context switching the state of
the CPU (e.g. registers) for the currently *running* task is stored and
the new task is swapped in.

List of Functions
-----------------

The functions available in context\_switch are:

+--------------+----------------+
| **Function** | **Description**|
|              |                |
+==============+================+
|`os\_sched <os| Performs       |
|_sched.html>`_| context switch |
|_             | if needed.     |
+--------------+----------------+
| `os\_arch\_c | Change the     |
| tx\_sw <os_a | state of task  |
| rch_ctx_sw.m | given task to  |
| d>`__        | *running*.     |
+--------------+----------------+
| `os\_sched\_ | Performs task  |
| ctx\_sw\_hoo | accounting     |
| k <os_sched_ | when context   |
| ctx_sw_hook. | switching.     |
| md>`__       |                |
+--------------+----------------+
| `os\_sched\_ | Returns the    |
| get\_current | pointer to     |
| \_task <os_s | task which is  |
| ched_get_cur | currently      |
| rent_task.md | *running*.     |
| >`__         |                |
+--------------+----------------+
| `os\_sched\_ | Insert task    |
| insert <os_s | into           |
| ched_insert. | scheduler's    |
| md>`__       | ready to run   |
|              | list.          |
+--------------+----------------+
| `os\_sched\_ | Returns the    |
| next\_task < | pointer to     |
| os_sched_nex | highest        |
| t_task.html> | priority task  |
| `__          | from the list  |
|              | which are      |
|              | *ready to      |
|              | run*.          |
+--------------+----------------+
| `os\_sched\_ | Inform         |
| os\_timer\_e | scheduler that |
| xp <os_sched | OS time has    |
| _os_timer_ex | moved forward. |
| p.html>`__   |                |
+--------------+----------------+
| `os\_sched\_ | Stops a task   |
| remove <os_s | and removes it |
| ched_remove. | from all the   |
| md>`__       | OS task lists. |
+--------------+----------------+
| `os\_sched\_ | Inform         |
| resort <os_s | scheduler that |
| ched_resort. | the priority   |
| md>`__       | of the given   |
|              | task has       |
|              | changed and    |
|              | the *ready to  |
|              | run* list      |
|              | should be      |
|              | re-sorted.     |
+--------------+----------------+
| `os\_sched\_ | Sets the given |
| set\_current | task to        |
| \_task <os_s | *running*.     |
| ched_set_cur |                |
| rent_task.md |                |
| >`__         |                |
+--------------+----------------+
| `os\_sched\_ | The given      |
| sleep <os_sc | task's state   |
| hed_sleep.md | is changed     |
| >`__         | from *ready to |
|              | run* to        |
|              | *sleeping*.    |
+--------------+----------------+
| `os\_sched\_ | Called to make |
| wakeup <os_s | task *ready to |
| ched_wakeup. | run*. If task  |
| md>`__       | is *sleeping*, |
|              | it is woken    |
|              | up.            |
+--------------+----------------+
