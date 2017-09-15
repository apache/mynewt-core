ble\_gap\_update\_params
------------------------

.. code:: c

    int
    ble_gap_update_params(
                               uint16_t  conn_handle,
        const struct ble_gap_upd_params *params
    )

Description
~~~~~~~~~~~

Initiates a connection parameter update procedure.

Parameters
~~~~~~~~~~

+----------------+---------------------------------------------------------+
| *Parameter*    | *Description*                                           |
+================+=========================================================+
| conn\_handle   | The handle corresponding to the connection to update.   |
+----------------+---------------------------------------------------------+
| params         | The connection parameters to attempt to update to.      |
+----------------+---------------------------------------------------------+

Returned values
~~~~~~~~~~~~~~~

+------------+----------------+
| *Value*    | *Condition*    |
+============+================+
| 0          | Success.       |
+------------+----------------+
| BLE\_HS\_E | The there is   |
| NOTCONN    | no connection  |
|            | with the       |
|            | specified      |
|            | handle.        |
+------------+----------------+
| BLE\_HS\_E | A connection   |
| ALREADY    | update         |
|            | procedure for  |
|            | this           |
|            | connection is  |
|            | already in     |
|            | progress.      |
+------------+----------------+
| BLE\_HS\_E | Requested      |
| INVAL      | parameters are |
|            | invalid.       |
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
