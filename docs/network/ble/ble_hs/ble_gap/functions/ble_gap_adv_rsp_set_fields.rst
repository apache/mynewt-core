ble\_gap\_adv\_rsp\_set\_fields
-------------------------------

.. code:: c

    int
    ble_gap_adv_rsp_set_fields(const struct ble_hs_adv_fields *rsp_fields)

Description
~~~~~~~~~~~

Configures the fields to include in subsequent scan responses. This is a
convenience wrapper for ble\_gap\_adv\_rsp\_set\_data().

Parameters
~~~~~~~~~~

+---------------+-------------------------------------+
| *Parameter*   | *Description*                       |
+===============+=====================================+
| adv\_fields   | Specifies the scan response data.   |
+---------------+-------------------------------------+

Returned values
~~~~~~~~~~~~~~~

+-----------------------------------------------------------------------+-------------------------------+
| *Value*                                                               | *Condition*                   |
+=======================================================================+===============================+
| 0                                                                     | Success.                      |
+-----------------------------------------------------------------------+-------------------------------+
| BLE\_HS\_EBUSY                                                        | Advertising is in progress.   |
+-----------------------------------------------------------------------+-------------------------------+
| `Core return code <../../ble_hs_return_codes/#return-codes-core>`__   | Unexpected error.             |
+-----------------------------------------------------------------------+-------------------------------+
