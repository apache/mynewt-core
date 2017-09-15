ble\_gattc\_read\_long
----------------------

.. code:: c

    int
    ble_gattc_read_long(
                uint16_t  conn_handle,
                uint16_t  handle,
                uint16_t  offset,
        ble_gatt_attr_fn *cb,
                    void *cb_arg
    )

Description
~~~~~~~~~~~

Initiates GATT procedure: Read Long Characteristic Values.

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
| handle         | The handle of    |
|                | the              |
|                | characteristic   |
|                | value to read.   |
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
