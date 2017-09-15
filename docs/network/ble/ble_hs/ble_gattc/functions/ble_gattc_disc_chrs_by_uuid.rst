ble\_gattc\_disc\_chrs\_by\_uuid
--------------------------------

.. code:: c

    int
    ble_gattc_disc_chrs_by_uuid(
                uint16_t  conn_handle,
                uint16_t  start_handle,
                uint16_t  end_handle,
        const ble_uuid_t *uuid,
         ble_gatt_chr_fn *cb,
                    void *cb_arg
    )

Description
~~~~~~~~~~~

Initiates GATT procedure: Discover Characteristics by UUID.

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
| start\_handle  | The handle to    |
|                | begin the search |
|                | at (generally    |
|                | the service      |
|                | definition       |
|                | handle).         |
+----------------+------------------+
| end\_handle    | The handle to    |
|                | end the search   |
|                | at (generally    |
|                | the last handle  |
|                | in the service). |
+----------------+------------------+
| chr\_uuid128   | The 128-bit UUID |
|                | of the           |
|                | characteristic   |
|                | to discover.     |
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
