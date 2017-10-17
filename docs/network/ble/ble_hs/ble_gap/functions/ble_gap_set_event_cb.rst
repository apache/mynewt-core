ble\_gap\_set\_event\_cb
------------------------

.. code:: c

    int
    ble_gap_set_event_cb(
                uint16_t  conn_handle,
        ble_gap_event_fn *cb,
                    void *cb_arg
    )

Description
~~~~~~~~~~~

Configures a connection to use the specified GAP event callback. A
connection's GAP event callback is first specified when the connection
is created, either via advertising or initiation. This function replaces
the callback that was last configured.

Parameters
~~~~~~~~~~

+----------------+----------------------------------------------------+
| *Parameter*    | *Description*                                      |
+================+====================================================+
| conn\_handle   | The handle of the connection to configure.         |
+----------------+----------------------------------------------------+
| cb             | The callback to associate with the connection.     |
+----------------+----------------------------------------------------+
| cb\_arg        | An optional argument that the callback receives.   |
+----------------+----------------------------------------------------+

Returned values
~~~~~~~~~~~~~~~

+---------------------+-----------------------------------------------------+
| *Value*             | *Condition*                                         |
+=====================+=====================================================+
| 0                   | Success.                                            |
+---------------------+-----------------------------------------------------+
| BLE\_HS\_ENOTCONN   | There is no connection with the specified handle.   |
+---------------------+-----------------------------------------------------+
