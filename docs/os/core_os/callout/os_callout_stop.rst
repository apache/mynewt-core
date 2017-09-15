 os\_callout\_stop 
-------------------

::

    void os_callout_stop(struct os_callout *c)

Disarms a timer.

Arguments
^^^^^^^^^

+-------------+----------------------------------------+
| Arguments   | Description                            |
+=============+========================================+
| c           | Pointer to os\_callout being stopped   |
+-------------+----------------------------------------+

Returned values
^^^^^^^^^^^^^^^

N/A

Example
^^^^^^^

::

    struct os_callout_func g_native_cputimer;

         os_callout_stop(&g_native_cputimer.cf_c);
