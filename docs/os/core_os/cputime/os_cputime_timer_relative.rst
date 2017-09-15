os\_cputime\_timer\_relative
----------------------------

.. code:: c

    void os_cputime_timer_relative(struct hal_timer *timer, uint32_t usecs)

Sets a timer to expire in the specified number of microseconds from the
current time. The callback function for the timer is called when the
timer expires.

Arguments
^^^^^^^^^

+-------------+-------------------------------------------------------------------+
| Arguments   | Description                                                       |
+=============+===================================================================+
| ``timer``   | Pointer to an initialized hal\_timer.                             |
+-------------+-------------------------------------------------------------------+
| ``usecs``   | The number of microseconds to set the timer to expire from now.   |
+-------------+-------------------------------------------------------------------+

Returned values
^^^^^^^^^^^^^^^

N/A

Notes
^^^^^

``timer`` must be initialized using the ``os_cputime_timer_init()``
function before setting up a timer.

Example
^^^^^^^

\`

.. code:: c

    struct hal_timer mytimer;
         ...

    os_cputime_timer_relative(&mytimer, 100);

