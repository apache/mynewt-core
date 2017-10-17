ble\_gattc\_read\_mult
----------------------

.. code:: c

    int
    ble_gattc_read_mult(
                uint16_t  conn_handle,
          const uint16_t *handles,
                 uint8_t  num_handles,
        ble_gatt_attr_fn *cb,
                    void *cb_arg
    )

Description
~~~~~~~~~~~

Initiates GATT procedure: Read Multiple Characteristic Values.

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
| handles        | An array of      |
|                | 16-bit attribute |
|                | handles to read. |
+----------------+------------------+
| num\_handles   | The number of    |
|                | entries in the   |
|                | "handles" array. |
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
