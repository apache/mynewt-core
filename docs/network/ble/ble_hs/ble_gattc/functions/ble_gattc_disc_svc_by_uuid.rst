ble\_gattc\_disc\_svc\_by\_uuid
-------------------------------

.. code:: c

    int
    ble_gattc_disc_svc_by_uuid(
                    uint16_t  conn_handle,
            const ble_uuid_t *uuid,
        ble_gatt_disc_svc_fn *cb,
                        void *cb_arg
    )

Description
~~~~~~~~~~~

Initiates GATT procedure: Discover Primary Service by Service UUID.

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
| service\_uuid1 | The 128-bit UUID |
| 28             | of the service   |
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
