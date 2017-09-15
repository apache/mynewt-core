 os\_task\_init
---------------

.. code:: c

    int os_task_init(struct os_task *t, char *name, os_task_func_t func, void *arg, 
                     uint8_t prio, os_time_t sanity_itvl, os_stack_t *stack_bottom, 
                     uint16_t stack_size)

Called to create a task. This adds the task object to the list of ready
to run tasks.

 #### Arguments

+--------------+----------------+
| Arguments    | Description    |
+==============+================+
| t            | Pointer to     |
|              | task           |
+--------------+----------------+
| name         | Task name      |
+--------------+----------------+
| func         | Task function  |
+--------------+----------------+
| arg          | Generic        |
|              | argument to    |
|              | pass to task   |
+--------------+----------------+
| prio         | Priority of    |
|              | task           |
+--------------+----------------+
| sanity\_itvl | The interval   |
|              | at which the   |
|              | sanity task    |
|              | will check to  |
|              | see if this    |
|              | task is sill   |
|              | alive          |
+--------------+----------------+
| stack\_botto | Pointer to     |
| m            | bottom of      |
|              | stack.         |
+--------------+----------------+
| stack\_size  | The size of    |
|              | the stack.     |
|              | NOTE: this is  |
|              | not in bytes!  |
|              | It is the      |
|              | number of      |
|              | ``os_stack_t`` |
|              | elements       |
|              | allocated      |
|              | (generally     |
|              | 32-bits each)  |
+--------------+----------------+

 #### Returned values

OS\_OK: task initialization successful.

All other error codes indicate an internal error.

 #### Example

.. code:: c


        /* Create the task */ 
        int rc;

        os_stack_t my_task_stack[MY_STACK_SIZE];

        rc = os_task_init(&my_task, "my_task", my_task_func, NULL, MY_TASK_PRIO, 
                          OS_WAIT_FOREVER, my_task_stack, MY_STACK_SIZE);
        assert(rc == OS_OK);
