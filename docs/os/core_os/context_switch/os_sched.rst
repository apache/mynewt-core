os\_sched
-----------

.. code:: c

    void os_sched(struct os_task *next_t)

Performs context switch if needed. If *next\_t* is set, that task will
be made *running*. If *next\_t* is NULL, highest priority *ready to run*
is swapped in. This function can be called when new tasks were made
*ready to run* or if the current task is moved to *sleeping* state.

This function will call the architecture specific routine to swap in the
new task.

Arguments
^^^^^^^^^

+-------------+--------------------------------------------------+
| Arguments   | Description                                      |
+=============+==================================================+
| next\_t     | Pointer to task which must run next (optional)   |
+-------------+--------------------------------------------------+

Returned values
^^^^^^^^^^^^^^^

N/A

Notes
^^^^^

Interrupts must be disabled when calling this.

Example
^^^^^^^

.. code:: c

    os_error_t
    os_mutex_release(struct os_mutex *mu)
    {
        ...
        OS_EXIT_CRITICAL(sr);

        /* Re-schedule if needed */
        if (resched) {
            os_sched(rdy);
        }

        return OS_OK;

    }
