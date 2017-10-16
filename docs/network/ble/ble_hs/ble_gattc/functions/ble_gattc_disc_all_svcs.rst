ble\_gattc\_disc\_all\_svcs
---------------------------

.. code:: c

    int
    ble_gattc_disc_all_svcs(
                    uint16_t  conn_handle,
        ble_gatt_disc_svc_fn *cb,
                        void *cb_arg
    )

Description
~~~~~~~~~~~

Initiates GATT procedure: Discover All Primary Services.

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

None
