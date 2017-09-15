os\_sched\_insert
-------------------

.. code:: c

    os_error_t
    os_sched_insert(struct os_task *t)

Insert task into scheduler's *ready to run* list.

Arguments
^^^^^^^^^

+-------------+-------------------+
| Arguments   | Description       |
+=============+===================+
| t           | Pointer to task   |
+-------------+-------------------+

Returned values
^^^^^^^^^^^^^^^

Returns OS\_EINVAL if task state is not *READY*. Returns 0 on success.

Notes
^^^^^

You probably don't need to call this.
