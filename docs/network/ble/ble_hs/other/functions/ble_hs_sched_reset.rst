ble\_hs\_sched\_reset
---------------------

.. code:: c

    void
    ble_hs_sched_reset(int reason)

Description
~~~~~~~~~~~

Causes the host to reset the NimBLE stack as soon as possible. The
application is notified when the reset occurs via the host reset
callback.

Parameters
~~~~~~~~~~

+---------------+---------------------------------------------------------------+
| *Parameter*   | *Description*                                                 |
+===============+===============================================================+
| reason        | The host error code that gets passed to the reset callback.   |
+---------------+---------------------------------------------------------------+

Returned values
~~~~~~~~~~~~~~~

None
