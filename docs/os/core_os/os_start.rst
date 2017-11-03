os\_start
---------

.. code-block:: console

    void os_start(void)

Starts the OS by initializing and enabling the OS time tick and starting
the scheduler.

This function does not return.

Arguments
^^^^^^^^^

None

Returned values
^^^^^^^^^^^^^^^

None (does not return).

Notes
^^^^^

Once os\_start has been called, context is switched to the highest
priority task that was initialized prior to calling os\_start.
