ble\_gattc\_disc\_all\_chrs
---------------------------

.. code:: c

    int
    ble_gattc_disc_all_chrs(
               uint16_t  conn_handle,
               uint16_t  start_handle,
               uint16_t  end_handle,
        ble_gatt_chr_fn *cb,
                   void *cb_arg
    )

Description
~~~~~~~~~~~

Initiates GATT procedure: Discover All Characteristics of a Service.

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
