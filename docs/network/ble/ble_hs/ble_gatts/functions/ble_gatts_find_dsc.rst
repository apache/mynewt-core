ble\_gatts\_find\_dsc
---------------------

.. code:: c

    int
    ble_gatts_find_dsc(
        const ble_uuid_t *svc_uuid,
        const ble_uuid_t *chr_uuid,
        const ble_uuid_t *dsc_uuid,
                uint16_t *out_handle
    )

Description
~~~~~~~~~~~

Retrieves the attribute handle associated with a local GATT descriptor.

Parameters
~~~~~~~~~~

+----------------+------------------+
| *Parameter*    | *Description*    |
+================+==================+
| svc\_uuid128   | The UUID of the  |
|                | grandparent      |
|                | service.         |
+----------------+------------------+
| chr\_uuid128   | The UUID of the  |
|                | parent           |
|                | characteristic.  |
+----------------+------------------+
| dsc\_uuid128   | The UUID of the  |
|                | descriptor ro    |
|                | look up.         |
+----------------+------------------+
| out\_handle    | On success,      |
|                | populated with   |
|                | the handle of    |
|                | the descripytor  |
|                | attribute. Pass  |
|                | null if you      |
|                | don't need this  |
|                | value.           |
+----------------+------------------+

Returned values
~~~~~~~~~~~~~~~

+------------+----------------+
| *Value*    | *Condition*    |
+============+================+
| 0          | Success.       |
+------------+----------------+
| BLE\_HS\_E | The specified  |
| NOENT      | service,       |
|            | characteristic |
|            | ,              |
|            | or descriptor  |
|            | could not be   |
|            | found.         |
+------------+----------------+
