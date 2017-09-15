os\_cputime\_init
-----------------

.. code:: c

    int os_cputime_init(uint32_t clock_freq)

Initializes the cputime module with the clock frequency (in HZ) to use
for the timer resolution. It configures the hardware timer specified by
the ``OS_CPUTIME_TIMER_NUM`` sysconfig value to run at the specified
clock frequency.

Arguments
^^^^^^^^^

+------------------+-----------------------------------------------------+
| Arguments        | Description                                         |
+==================+=====================================================+
| ``clock_freq``   | Clock frequency, in HZ, for the timer resolution.   |
+------------------+-----------------------------------------------------+

Returned values
^^^^^^^^^^^^^^^

0 on success and -1 on error.

Notes
^^^^^

This function must be called after os\_init is called. It should only be
called once and before any other timer API and hardware timers are used.

Example
^^^^^^^

A BSP package usually calls this function and uses the
``OS_CPUTIME_FREQUENCY`` sysconfig value for the clock frequency
argument:

.. code:: c

    int rc
    rc = os_cputime_init(MYNEWT_VAL(OS_CPUTIME_FREQUENCY));
