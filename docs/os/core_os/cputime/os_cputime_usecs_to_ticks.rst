os\_cputime\_usecs\_to\_ticks
-----------------------------

.. code:: c

    uint32_t os_cputime_usecs_to_ticks(uint32_t usecs)

Converts a specified number of microseconds to cputime ticks.

Arguments
^^^^^^^^^

+-------------+-----------------------------------------------+
| Arguments   | Description                                   |
+=============+===============================================+
| ``usecs``   | Number of microseconds to convert to ticks.   |
+-------------+-----------------------------------------------+

Returned values
^^^^^^^^^^^^^^^

The number of ticks in ``usecs`` nanoseconds.

Notes
^^^^^

Example
^^^^^^^

.. code:: c

    uint32_t num_ticks;
    num_ticks = os_cputime_usecs_to_ticks(100);
