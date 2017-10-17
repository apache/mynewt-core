ble\_gatts\_count\_cfg
----------------------

.. code:: c

    int
    ble_gatts_count_cfg(const struct ble_gatt_svc_def *defs)

Description
~~~~~~~~~~~

Adjusts a host configuration object's settings to accommodate the
specified service definition array. This function adds the counts to the
appropriate fields in the supplied configuration object without clearing
them first, so it can be called repeatedly with different inputs to
calculate totals. Be sure to zero the GATT server settings prior to the
first call to this function.

Parameters
~~~~~~~~~~

+---------------+------------------------------------------------------------------------+
| *Parameter*   | *Description*                                                          |
+===============+========================================================================+
| defs          | The service array containing the resource definitions to be counted.   |
+---------------+------------------------------------------------------------------------+
| cfg           | The resource counts are accumulated in this configuration object.      |
+---------------+------------------------------------------------------------------------+

Returned values
~~~~~~~~~~~~~~~

+-------------------+-----------------------------------------------------------+
| *Value*           | *Condition*                                               |
+===================+===========================================================+
| 0                 | Success.                                                  |
+-------------------+-----------------------------------------------------------+
| BLE\_HS\_EINVAL   | The svcs array contains an invalid resource definition.   |
+-------------------+-----------------------------------------------------------+
