ble\_gattc\_write\_flat
-----------------------

.. code:: c

    int
    ble_gattc_write_flat(
                uint16_t  conn_handle,
                uint16_t  attr_handle,
              const void *data,
                uint16_t  data_len,
        ble_gatt_attr_fn *cb,
                    void *cb_arg
    )

Description
~~~~~~~~~~~

Initiates GATT procedure: Write Characteristic Value (flat buffer
version).

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
| attr\_handle   | The handle of    |
|                | the              |
|                | characteristic   |
|                | value to write   |
|                | to.              |
+----------------+------------------+
| value          | The value to     |
|                | write to the     |
|                | characteristic.  |
+----------------+------------------+
| value\_len     | The number of    |
|                | bytes to write.  |
+----------------+------------------+
| cb             | The function to  |
|                | call to report   |
|                | procedure status |
|                | updates; null    |
|                | for no callback. |
+----------------+------------------+
| cb\_arg        | The optional     |
|                | argument to pass |
|                | to the callback  |
|                | function.        |
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
