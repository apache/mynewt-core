ble\_gap\_adv\_set\_fields
--------------------------

.. code:: c

    int
    ble_gap_adv_set_fields(const struct ble_hs_adv_fields *adv_fields)

Description
~~~~~~~~~~~

Configures the fields to include in subsequent advertisements. This is a
convenience wrapper for ble\_gap\_adv\_set\_data().

Parameters
~~~~~~~~~~

+---------------+-------------------------------------+
| *Parameter*   | *Description*                       |
+===============+=====================================+
| adv\_fields   | Specifies the advertisement data.   |
+---------------+-------------------------------------+

Returned values
~~~~~~~~~~~~~~~

+------------+----------------+
| *Value*    | *Condition*    |
+============+================+
| 0          | Success.       |
+------------+----------------+
| BLE\_HS\_E | Advertising is |
| BUSY       | in progress.   |
+------------+----------------+
| BLE\_HS\_E | The specified  |
| MSGSIZE    | data is too    |
|            | large to fit   |
|            | in an          |
|            | advertisement. |
+------------+----------------+
| `Core      | Unexpected     |
| return     | error.         |
| code <../. |                |
| ./ble_hs_r |                |
| eturn_code |                |
| s/#return- |                |
| codes-core |                |
| >`__       |                |
+------------+----------------+
