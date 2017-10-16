ble\_att\_svr\_write\_local
---------------------------

.. code:: c

    int
    ble_att_svr_write_local(
              uint16_t  attr_handle,
        struct os_mbuf *om
    )

Description
~~~~~~~~~~~

Writes a locally registered attribute. This function consumes the
supplied mbuf regardless of the outcome. If the specified attribute
handle coresponds to a GATT characteristic value or descriptor, the
write is performed by calling the registered GATT access callback.

Parameters
~~~~~~~~~~

+----------------+------------------------------------------------+
| *Parameter*    | *Description*                                  |
+================+================================================+
| attr\_handle   | The 16-bit handle of the attribute to write.   |
+----------------+------------------------------------------------+
| om             | The value to write to the attribute.           |
+----------------+------------------------------------------------+

Returned values
~~~~~~~~~~~~~~~

+-----------------------------------------------------------------------+--------------------------------------------------+
| *Value*                                                               | *Condition*                                      |
+=======================================================================+==================================================+
| 0                                                                     | Success.                                         |
+-----------------------------------------------------------------------+--------------------------------------------------+
| `ATT return code <../../ble_hs_return_codes/#return-codes-att>`__     | The attribute access callback reports failure.   |
+-----------------------------------------------------------------------+--------------------------------------------------+
| `Core return code <../../ble_hs_return_codes/#return-codes-core>`__   | Unexpected error.                                |
+-----------------------------------------------------------------------+--------------------------------------------------+
