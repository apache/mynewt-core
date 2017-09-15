 os\_sanity\_task\_checkin
--------------------------

.. code:: c

    int os_sanity_task_checkin(struct os_task *t)

Used by a task to check in to the sanity task. This informs the sanity
task that *task* is still alive and working normally.

 #### Arguments

+-------------+-------------------+
| Arguments   | Description       |
+=============+===================+
| ``t``       | Pointer to task   |
+-------------+-------------------+

 #### Returned values

``OS_OK``: sanity check-in successful

All other error codes indicate an error.

 #### Example

.. code:: c

        int rc;

        rc = os_sanity_task_checkin(&my_task); 
        assert(rc == OS_OK);

