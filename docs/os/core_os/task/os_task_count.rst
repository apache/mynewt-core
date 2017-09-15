 os\_task\_count
----------------

.. code:: c

    uint8_t os_task_count(void);

Returns the number of tasks that have been created.

 #### Arguments

None

 #### Returned values

unsigned 8-bit integer representing number of tasks created

 #### Example

.. code:: c


        uint8_t num_tasks;

        num_tasks = os_task_count();
