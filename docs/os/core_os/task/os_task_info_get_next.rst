 os\_task\_info\_get\_next
--------------------------

.. code:: c

    struct os_task *os_task_info_get_next(const struct os_task *prev, struct os_task_info *oti);

Populates the os task info structure pointed to by *oti* with task
information. The task populating the *oti* structure is either the first
task on the task list if *prev* is NULL, or the next task in the task
list (the next pointer of *prev*).

If there are no tasks initialized, NULL is returned. Otherwise, the task
structure used to populate *oti* is returned.

 #### Arguments

+--------------+----------------+
| Arguments    | Description    |
+==============+================+
| prev         | Pointer to     |
|              | previous task  |
|              | in task list.  |
|              | If NULL, use   |
|              | first task on  |
|              | list           |
+--------------+----------------+
| oti          | Pointer to     |
|              | ``os_task_info |
|              | ``             |
|              | structure      |
|              | where task     |
|              | information    |
|              | will be stored |
+--------------+----------------+

 #### Returned values

Returns a pointer to the os task structure that was used to populate the
task information structure. NULL means that no tasks were created.

 #### Example

.. code:: c


    void 
    get_task_info(void)
    {
        struct os_task *prev_task; 
        struct os_task_info oti; 

        console_printf("Tasks: \n");
        prev_task = NULL;
        while (1) {
            prev_task = os_task_info_get_next(prev_task, &oti);
            if (prev_task == NULL) {
                break;
            }

            console_printf("  %s (prio: %u, tid: %u, lcheck: %lu, ncheck: %lu, "
                    "flags: 0x%x, ssize: %u, susage: %u, cswcnt: %lu, "
                    "tot_run_time: %lums)\n",
                    oti.oti_name, oti.oti_prio, oti.oti_taskid, 
                    (unsigned long)oti.oti_last_checkin,
                    (unsigned long)oti.oti_next_checkin, oti.oti_flags,
                    oti.oti_stksize, oti.oti_stkusage, (unsigned long)oti.oti_cswcnt,
                    (unsigned long)oti.oti_runtime);

        }
    }

