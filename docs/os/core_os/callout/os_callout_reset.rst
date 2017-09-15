 os\_callout\_reset 
--------------------

::

    void os_callout_reset(struct os_callout *c, int32_t timo)

Resets the callout to happen *timo* in OS ticks.

Arguments
^^^^^^^^^

+-------------+--------------------------------------+
| Arguments   | Description                          |
+=============+======================================+
| c           | Pointer to os\_callout being reset   |
+-------------+--------------------------------------+
| timo        | OS ticks the timer is being set to   |
+-------------+--------------------------------------+

Returned values
^^^^^^^^^^^^^^^

N/A

Notes
^^^^^

Example
^^^^^^^

::

    /* Re-start the timer (run every 50 msecs) */
    os_callout_reset(&g_bletest_timer.cf_c, OS_TICKS_PER_SEC / 20);
