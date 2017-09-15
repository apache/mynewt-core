ble\_gap\_conn\_find
--------------------

.. code:: c

    int
    ble_gap_conn_find(
                        uint16_t  handle,
        struct ble_gap_conn_desc *out_desc
    )

Description
~~~~~~~~~~~

Searches for a connection with the specified handle. If a matching
connection is found, the supplied connection descriptor is filled
correspondingly.

Parameters
~~~~~~~~~~

+----------------+------------------+
| *Parameter*    | *Description*    |
+================+==================+
| handle         | The connection   |
|                | handle to search |
|                | for.             |
+----------------+------------------+
| out\_desc      | On success, this |
|                | is populated     |
|                | with information |
|                | relating to the  |
|                | matching         |
|                | connection. Pass |
|                | NULL if you      |
|                | don't need this  |
|                | information.     |
+----------------+------------------+

Returned values
~~~~~~~~~~~~~~~

+---------------------+-------------------------------------+
| *Value*             | *Condition*                         |
+=====================+=====================================+
| 0                   | Success.                            |
+---------------------+-------------------------------------+
| BLE\_HS\_ENOTCONN   | No matching connection was found.   |
+---------------------+-------------------------------------+
