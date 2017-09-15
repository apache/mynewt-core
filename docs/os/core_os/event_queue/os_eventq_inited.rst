 os\_eventq\_inited
-------------------

.. code:: c

       int
        os_eventq_inited(const struct os_eventq *evq)

Indicates whether an event queue has been initialized.

Arguments
^^^^^^^^^

+-------------+---------------------------------------+
| Arguments   | Description                           |
+=============+=======================================+
| ``evq``     | Pointer to the event queue to check   |
+-------------+---------------------------------------+

Returned values
^^^^^^^^^^^^^^^

-  1 if the event queue has been initialized and ready for use.
-  0 if the event queue has not been initialized.

Notes
^^^^^

You do not need to call this function prior to using an event queue if
you have called the ``os_eventq_init()`` function to initialize the
queue.

Example
^^^^^^^

 This example checks whether an event queue has been initialized.

.. code:: c

    struct os_eventq g_my_evq;

    int
    my_task_init(uint8_t prio, os_stack_t *stack_ptr, uint16_t stack_len)
    {
        /* variable declarations here */

        if(os_eventq_inited(&g_my_evq))
        {
            /* deal with the event queue */
        };

    }
