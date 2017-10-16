ble\_gattc\_notify
------------------

.. code:: c

    int
    ble_gattc_notify(
        uint16_t conn_handle,
        uint16_t chr_val_handle
    )

Description
~~~~~~~~~~~

Sends a characteristic notification. The content of the message is read
from the specified characteristic.

Parameters
~~~~~~~~~~

+----------------+------------------+
| *Parameter*    | *Description*    |
+================+==================+
| conn\_handle   | The connection   |
|                | over which to    |
|                | execute the      |
|                | procedure.       |
+----------------+------------------+
| chr\_val\_hand | The value        |
| le             | attribute handle |
|                | of the           |
|                | characteristic   |
|                | to include in    |
|                | the outgoing     |
|                | notification.    |
+----------------+------------------+

Returned values
~~~~~~~~~~~~~~~

+-----------------------------------------------------------------------+---------------------+
| *Value*                                                               | *Condition*         |
+=======================================================================+=====================+
| 0                                                                     | Success.            |
+-----------------------------------------------------------------------+---------------------+
| `Core return code <../../ble_hs_return_codes/#return-codes-core>`__   | Unexpected error.   |
+-----------------------------------------------------------------------+---------------------+
