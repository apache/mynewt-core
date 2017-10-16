ble\_eddystone\_set\_adv\_data\_uid
-----------------------------------

.. code:: c

    int
    ble_eddystone_set_adv_data_uid(
        struct ble_hs_adv_fields *adv_fields,
                            void *uid
    )

Description
~~~~~~~~~~~

Configures the device to advertise eddystone UID beacons.

Parameters
~~~~~~~~~~

+----------------+------------------+
| *Parameter*    | *Description*    |
+================+==================+
| adv\_fields    | The base         |
|                | advertisement    |
|                | fields to        |
|                | transform into   |
|                | an eddystone     |
|                | beacon. All      |
|                | configured       |
|                | fields are       |
|                | preserved; you   |
|                | probably want to |
|                | clear this       |
|                | struct before    |
|                | calling this     |
|                | function.        |
+----------------+------------------+
| uid            | The 16-byte UID  |
|                | to advertise.    |
+----------------+------------------+

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
