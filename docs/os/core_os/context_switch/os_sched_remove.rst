os\_sched\_remove
-------------------

.. code:: c

    int os_sched_remove(struct os_task *t)

Stops and removes task ``t``.

The function removes the task from the ``g_os_task_list`` task list. It
also removes the task from one of the following task list:

-  ``g_os_run_list`` if the task is in the Ready state.
-  ``g_os_sleep_list`` if the task is in the Sleep state.

Arguments
^^^^^^^^^

+-------------+--------------------------------------------------------------------+
| Arguments   | Description                                                        |
+=============+====================================================================+
| ``t``       | Pointer to the ``os_task`` structure for the task to be removed.   |
+-------------+--------------------------------------------------------------------+

Returned values
^^^^^^^^^^^^^^^

OS\_OK - The task is removed successfully.

Notes
^^^^^

This function must be called with all interrupts disabled.

Example
^^^^^^^

The ``os_task_remove`` function calls the ``os_sched_remove`` function
to remove a task:

.. code:: c


    int
    os_task_remove(struct os_task *t)
    {
        int rc;
        os_sr_t sr;

        /* Add error checking code to ensure task can removed. */


        /* Call os_sched_remove to remove the task. */
        OS_ENTER_CRITICAL(sr);
        rc = os_sched_remove(t);
        OS_EXIT_CRITICAL(sr);
        return rc;
    }
