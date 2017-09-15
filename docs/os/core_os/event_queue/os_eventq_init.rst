 os\_eventq\_init
-----------------

.. code:: c

       void
        os_eventq_init(struct os_eventq *evq)

Initializes an event queue.

Arguments
^^^^^^^^^

+-------------+--------------------------------------------+
| Arguments   | Description                                |
+=============+============================================+
| ``evq``     | Pointer to the event queue to initialize   |
+-------------+--------------------------------------------+

Returned values
^^^^^^^^^^^^^^^

None

Notes
^^^^^

Called during OS, package, and application initialization and before
interrupts generating events have been enabled.

Example
^^^^^^^

 This example shows the ``app/bletest`` application initializing the
``g_bletest_evq`` event queue.

.. code:: c

    struct os_eventq g_bletest_evq;

    int
    main(void)
    {
        /* variable declarations here */

        os_eventq_init(&g_bletest_evq);

        /* initialization continues here */

    }
