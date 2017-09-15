ble\_gap\_wl\_set
-----------------

.. code:: c

    int
    ble_gap_wl_set(
        const ble_addr_t *addrs,
                 uint8_t  white_list_count
    )

Description
~~~~~~~~~~~

Overwrites the controller's white list with the specified contents.

Parameters
~~~~~~~~~~

+----------------------+--------------------------------------------+
| *Parameter*          | *Description*                              |
+======================+============================================+
| addrs                | The entries to write to the white list.    |
+----------------------+--------------------------------------------+
| white\_list\_count   | The number of entries in the white list.   |
+----------------------+--------------------------------------------+

Returned values
~~~~~~~~~~~~~~~

+-----------------------------------------------------------------------+---------------------+
| *Value*                                                               | *Condition*         |
+=======================================================================+=====================+
| 0                                                                     | Success.            |
+-----------------------------------------------------------------------+---------------------+
| `Core return code <../../ble_hs_return_codes/#return-codes-core>`__   | Unexpected error.   |
+-----------------------------------------------------------------------+---------------------+
