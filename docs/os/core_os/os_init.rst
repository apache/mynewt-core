os\_init
--------

.. code-block:: console

    void os_init(void)

Initializes the OS. Must be called before the OS is started (i.e.
os\_start() is called).

Arguments
^^^^^^^^^

None

Returned values
^^^^^^^^^^^^^^^

None

Notes
^^^^^

The call to os\_init performs architecture and bsp initializations and
initializes the idle task.

This function does not start the OS, the OS time tick interrupt, or the
scheduler.
