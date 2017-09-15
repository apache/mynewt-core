ble\_gap\_set\_priv\_mode
-------------------------

.. code:: c

    int
    ble_gap_set_priv_mode(
            const ble_addr_t *peer_addr,
                     uint8_t  priv_mode
    )

Description
~~~~~~~~~~~

Set privacy mode for given device.

If device mode is used, then Nimble accepts both the peer's identity
address and a resolvable private address.

If network mode is used, then Nimble accepts only resolvable private
address.

Parameters
~~~~~~~~~~

+----------------+------------------+
| *Parameter*    | *Description*    |
+================+==================+
| peer\_addr     | The identity     |
|                | address of the   |
|                | peer for which   |
|                | privacy mode     |
|                | shall be         |
|                | changed.         |
+----------------+------------------+

\| priv\_mode \| The privacy mode. One of:

.. raw:: html

   <ul>

.. raw:: html

   <li>

BLE\_GAP\_PRIVATE\_MODE\_NETWORK

.. raw:: html

   </li>

.. raw:: html

   <li>

BLE\_GAP\_PRIVATE\_MODE\_DEVICE

.. raw:: html

   </li>

.. raw:: html

   </ul>

\|

Returned values
~~~~~~~~~~~~~~~

+-----------------------------------------------------------------------+---------------------+
| *Value*                                                               | *Condition*         |
+=======================================================================+=====================+
| 0                                                                     | Success.            |
+-----------------------------------------------------------------------+---------------------+
| `Core return code <../../ble_hs_return_codes/#return-codes-core>`__   | Unexpected error.   |
+-----------------------------------------------------------------------+---------------------+
