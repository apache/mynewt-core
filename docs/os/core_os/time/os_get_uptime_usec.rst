os\_get\_uptime\_usec
---------------------

.. code:: c

    int64_t os_get_uptime_usec(void)

Gets the time duration, in microseconds, since boot.

Arguments
^^^^^^^^^

N/A

Returned values
^^^^^^^^^^^^^^^

Time since boot in microseconds.

Notes
^^^^^

Example
^^^^^^^

.. code:: c

       int64_t time_since_boot;
       time_since_boot = os_get_uptime_usec();
