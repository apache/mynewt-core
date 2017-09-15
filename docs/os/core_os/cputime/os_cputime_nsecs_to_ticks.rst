os\_cputime\_nsecs\_to\_ticks
-----------------------------

.. code:: c

    uint32_t os_cputime_nsecs_to_ticks(uint32_t nsecs)

Converts a specified number of nanoseconds to cputime ticks.

Arguments
^^^^^^^^^

+-------------+----------------------------------------------+
| Arguments   | Description                                  |
+=============+==============================================+
| ``nsecs``   | Number of nanoseconds to convert to ticks.   |
+-------------+----------------------------------------------+

Returned values
^^^^^^^^^^^^^^^

The number of ticks in ``nsecs`` nanoseconds.

Notes
^^^^^

Example
^^^^^^^

.. code:: c

    uint32_t num_ticks;
    num_ticks = os_cputime_nsecs_to_ticks(1000000);
