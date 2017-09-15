os\_time\_delay
---------------

.. code:: c

    void os_time_delay(int32_t ticks) 

Arguments
^^^^^^^^^

+-------------+-----------------------------------------------------------------------+
| Arguments   | Description                                                           |
+=============+=======================================================================+
| ticks       | Number of ticks to delay. Less than or equal to zero means no delay   |
+-------------+-----------------------------------------------------------------------+

Returned values
^^^^^^^^^^^^^^^

N/A

Notes
^^^^^

Passing ``OS_TIMEOUT_NEVER`` to this function will not block
indefinitely but will return immediately. Passing delays larger than 1/2
the OS time epoch should be avoided; behavior is unpredictable.

Example
^^^^^^^

.. code:: c

        /* delay 3 seconds */
        int32_t delay = OS_TICKS_PER_SEC * 3;
        os_time_delay(delay);
