ble\_gattc\_indicate\_custom
----------------------------

.. code:: c

    int
    ble_gattc_indicate_custom(
              uint16_t  conn_handle,
              uint16_t  chr_val_handle,
        struct os_mbuf *txom
    )

Description
~~~~~~~~~~~

Sends a characteristic indication. The content of the message is read
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
|                | indication.      |
+----------------+------------------+
| txom           | The data to      |
|                | include in the   |
|                | indication.      |
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
