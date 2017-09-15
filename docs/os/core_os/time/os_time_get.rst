os\_time\_get
-------------

.. code:: c

    os_time_t os_time_get(void) 

Arguments
^^^^^^^^^

N/A

Returned values
^^^^^^^^^^^^^^^

The current value of the OS time

Notes
^^^^^

See the `Special Notes <os_time>`__ on OS time epoch and comparison

Example
^^^^^^^

.. code:: c

       os_time_t now = os_time_get();
