ble\_gattc\_read\_by\_uuid
--------------------------

.. code:: c

    int
    ble_gattc_read_by_uuid(
                uint16_t  conn_handle,
                uint16_t  start_handle,
                uint16_t  end_handle,
        const ble_uuid_t *uuid,
        ble_gatt_attr_fn *cb,
                    void *cb_arg
    )

Description
~~~~~~~~~~~

Initiates GATT procedure: Read Using Characteristic UUID.

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
| start\_handle  | The first handle |
|                | to search        |
|                | (generally the   |
|                | handle of the    |
|                | service          |
|                | definition).     |
+----------------+------------------+
| end\_handle    | The last handle  |
|                | to search        |
|                | (generally the   |
|                | last handle in   |
|                | the service      |
|                | definition).     |
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
