 os\_eventq\_dflt\_set
----------------------

.. code:: c

       void
        os_eventq_dflt_set(struct os_eventq *evq)

Sets ``struct os_eventq`` as the default event queue

Arguments
^^^^^^^^^

+-------------+-----------------------------------------+
| Arguments   | Description                             |
+=============+=========================================+
| ``evq``     | Pointer to default event queue to use   |
+-------------+-----------------------------------------+

Returned values
^^^^^^^^^^^^^^^

None

Notes
^^^^^

Usually done at subsystem init time; before OS has been started, and
before interrupts generating events have been enabled.

Example
^^^^^^^

 This sets the default event queue used by newtmgr task.

.. code:: c

    struct os_eventq g_nmgr_evq;

    int
    nmgr_task_init(uint8_t prio, os_stack_t *stack_ptr, uint16_t stack_len)
    {
        /* variable declarations here */

        os_eventq_init(&g_nmgr_evq);
        os_eventq_dflt_set(&g_nmgr_evq);

        /* initialization continues here */
    }
