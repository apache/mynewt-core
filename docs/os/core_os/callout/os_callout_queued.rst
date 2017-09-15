os\_callout\_queued
-------------------

::

    int os_callout_queued(struct os_callout *c)

Tells whether the callout has been armed or not.

Arguments
^^^^^^^^^

+-------------+------------------------------------+
| Arguments   | Description                        |
+=============+====================================+
| c           | Pointer to callout being checked   |
+-------------+------------------------------------+

Returned values
^^^^^^^^^^^^^^^

0: timer is not armed non-zero: timer is armed
