ble\_gattc\_notify\_custom
--------------------------

.. code:: c

    int
    ble_gattc_notify_custom(
              uint16_t  conn_handle,
              uint16_t  chr_val_handle,
        struct os_mbuf *txom
    )

Description
~~~~~~~~~~~

Sends a "free-form" characteristic notification. This function consumes
the supplied mbuf regardless of the outcome.

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
| chr\_val\_hand | The attribute    |
| le             | handle to        |
|                | indicate in the  |
|                | outgoing         |
|                | notification.    |
+----------------+------------------+
| txom           | The value to     |
|                | write to the     |
|                | characteristic.  |
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
