os\_sched\_set\_current\_task
-------------------------------

.. code:: c

    void
    os_sched_set_current_task(struct os_task *t)

Sets the currently running task to 't'.

This is called from architecture specific context switching code to
update scheduler state. Call is made when state of the task 't' is made
*running*.

Arguments
^^^^^^^^^

+-------------+---------------------+
| Arguments   | Description         |
+=============+=====================+
| t           | Pointer to a task   |
+-------------+---------------------+

Returned values
^^^^^^^^^^^^^^^

N/A

Notes
^^^^^

This function simply sets the global variable holding the currently
running task. It does not perform a context switch or change the os run
or sleep list.
