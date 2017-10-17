ble\_gatts\_find\_chr
---------------------

.. code:: c

    int
    ble_gatts_find_chr(
        const ble_uuid_t *svc_uuid,
        const ble_uuid_t *chr_uuid,
                uint16_t *out_def_handle,
                uint16_t *out_val_handle
    )

Description
~~~~~~~~~~~

Retrieves the pair of attribute handles associated with a local GATT
characteristic.

Parameters
~~~~~~~~~~

+----------------+------------------+
| *Parameter*    | *Description*    |
+================+==================+
| svc\_uuid128   | The UUID of the  |
|                | parent service.  |
+----------------+------------------+
| chr\_uuid128   | The UUID of the  |
|                | characteristic   |
|                | to look up.      |
+----------------+------------------+
| out\_def\_hand | On success,      |
| le             | populated with   |
|                | the handle of    |
|                | the              |
|                | characteristic   |
|                | definition       |
|                | attribute. Pass  |
|                | null if you      |
|                | don't need this  |
|                | value.           |
+----------------+------------------+
| out\_val\_hand | On success,      |
| le             | populated with   |
|                | the handle of    |
|                | the              |
|                | characteristic   |
|                | value attribute. |
|                | Pass null if you |
|                | don't need this  |
|                | value.           |
+----------------+------------------+

Returned values
~~~~~~~~~~~~~~~

+-------------------+---------------------------------------------------------------+
| *Value*           | *Condition*                                                   |
+===================+===============================================================+
| 0                 | Success.                                                      |
+-------------------+---------------------------------------------------------------+
| BLE\_HS\_ENOENT   | The specified service or characteristic could not be found.   |
+-------------------+---------------------------------------------------------------+
