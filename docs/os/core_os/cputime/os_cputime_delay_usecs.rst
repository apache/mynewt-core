os\_cputime\_delay\_usecs
-------------------------

.. code:: c

    void os_cputime_delay_usecs(uint32_t usecs)

Waits for a specified number of microseconds to elapse before returning.

Arguments
^^^^^^^^^

+-------------+----------------------------------------+
| Arguments   | Description                            |
+=============+========================================+
| ``usecs``   | Number of microseconds to delay for.   |
+-------------+----------------------------------------+

Returned values
^^^^^^^^^^^^^^^

N/A

Notes
^^^^^

Example
^^^^^^^

Delays for 250000 usecs:

.. code:: c

    os_cputime_delay_usecs(250000)
