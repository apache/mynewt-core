os\_started
-----------

.. code-block:: console

    int os_started(void)

Returns 'true' (1) if the os has been started; 0 otherwise.

Arguments
^^^^^^^^^

None

Returned values
^^^^^^^^^^^^^^^

Integer value with 0 meaning the OS has not been started and 1
indicating the OS has been started (i.e. os\_start() has been called).
