ble\_gattc\_write\_no\_rsp
--------------------------

.. code:: c

    int
    ble_gattc_write_no_rsp(
              uint16_t  conn_handle,
              uint16_t  attr_handle,
        struct os_mbuf *txom
    )

Description
~~~~~~~~~~~

Initiates GATT procedure: Write Without Response. This function consumes
the supplied mbuf regardless of the outcome.

Parameters
~~~~~~~~~~

+----------------+-------------------------------------------------------+
| *Parameter*    | *Description*                                         |
+================+=======================================================+
| conn\_handle   | The connection over which to execute the procedure.   |
+----------------+-------------------------------------------------------+
| attr\_handle   | The handle of the characteristic value to write to.   |
+----------------+-------------------------------------------------------+
| txom           | The value to write to the characteristic.             |
+----------------+-------------------------------------------------------+

Returned values
~~~~~~~~~~~~~~~

+-----------------------------------------------------------------------+---------------------+
| *Value*                                                               | *Condition*         |
+=======================================================================+=====================+
| 0                                                                     | Success.            |
+-----------------------------------------------------------------------+---------------------+
| `Core return code <../../ble_hs_return_codes/#return-codes-core>`__   | Unexpected error.   |
+-----------------------------------------------------------------------+---------------------+
