ble\_gatts\_add\_svcs
---------------------

.. code:: c

    int
    ble_gatts_add_svcs(const struct ble_gatt_svc_def *svcs)

Description
~~~~~~~~~~~

Queues a set of service definitions for registration. All services
queued in this manner get registered when ble\_hs\_init() is called.

Parameters
~~~~~~~~~~

+----------------+------------------+
| *Parameter*    | *Description*    |
+================+==================+
| svcs           | An array of      |
|                | service          |
|                | definitions to   |
|                | queue for        |
|                | registration.    |
|                | This array must  |
|                | be terminated    |
|                | with an entry    |
|                | whose 'type'     |
|                | equals 0.        |
+----------------+------------------+

Returned values
~~~~~~~~~~~~~~~

+-------------------+--------------------+
| *Value*           | *Condition*        |
+===================+====================+
| 0                 | Success.           |
+-------------------+--------------------+
| BLE\_HS\_ENOMEM   | Heap exhaustion.   |
+-------------------+--------------------+
