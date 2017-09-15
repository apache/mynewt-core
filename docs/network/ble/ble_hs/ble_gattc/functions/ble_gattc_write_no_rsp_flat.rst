ble\_gattc\_write\_no\_rsp\_flat
--------------------------------

.. code:: c

    int
    ble_gattc_write_no_rsp_flat(
          uint16_t  conn_handle,
          uint16_t  attr_handle,
        const void *data,
          uint16_t  data_len
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
| value          | The value to write to the characteristic.             |
+----------------+-------------------------------------------------------+
| value\_len     | The number of bytes to write.                         |
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
