ble\_gap\_security\_initiate
----------------------------

.. code:: c

    int
    ble_gap_security_initiate(uint16_t conn_handle)

Description
~~~~~~~~~~~

Initiates the GAP encryption procedure.

Parameters
~~~~~~~~~~

+----------------+----------------------------------------------------------+
| *Parameter*    | *Description*                                            |
+================+==========================================================+
| conn\_handle   | The handle corresponding to the connection to encrypt.   |
+----------------+----------------------------------------------------------+

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
| BLE\_HS\_E | An encrpytion  |
| ALREADY    | procedure for  |
|            | this           |
|            | connection is  |
|            | already in     |
|            | progress.      |
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
