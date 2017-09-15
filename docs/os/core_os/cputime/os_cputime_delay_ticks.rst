os\_cputime\_delay\_ticks
-------------------------

.. code:: c

    void os_cputime_delay_ticks(uint32_t ticks)

Waits for a specified number of ticks to elapse before returning.

Arguments
^^^^^^^^^

+-------------+---------------------------------+
| Arguments   | Description                     |
+=============+=================================+
| ``ticks``   | Number of ticks to delay for.   |
+-------------+---------------------------------+

Returned values
^^^^^^^^^^^^^^^

N/A

Notes
^^^^^

Example
^^^^^^^

Delays for 100000 ticks:

.. code:: c

    os_cputime_delay_ticks(100000)
