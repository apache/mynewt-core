ble\_gap\_read\_le\_phy
-----------------------

.. code:: c

    int
    ble_gap_read_le_phy(
            uint16_t conn_handle,
             uint8_t *tx_phy,
             uint8_t *rx_phy
    )

Description
~~~~~~~~~~~

Read PHY on given connection.

Parameters
~~~~~~~~~~

+----------------+------------------+
| *Parameter*    | *Description*    |
+================+==================+
| conn\_handle   | The handle       |
|                | corresponding to |
|                | the connection   |
|                | to on which PHY  |
|                | is read.         |
+----------------+------------------+

\| tx\_phy \| Tx PHY. One of:

.. raw:: html

   <ul>

.. raw:: html

   <li>

BLE\_GAP\_LE\_PHY\_1M

.. raw:: html

   </li>

.. raw:: html

   <li>

BLE\_GAP\_LE\_PHY\_2M

.. raw:: html

   </li>

.. raw:: html

   <li>

BLE\_GAP\_LE\_PHY\_CODED

.. raw:: html

   </li>

.. raw:: html

   </ul>

\| \| rx\_phy \| Rx PHY. One of:

.. raw:: html

   <ul>

.. raw:: html

   <li>

BLE\_GAP\_LE\_PHY\_1M

.. raw:: html

   </li>

.. raw:: html

   <li>

BLE\_GAP\_LE\_PHY\_2M

.. raw:: html

   </li>

.. raw:: html

   <li>

BLE\_GAP\_LE\_PHY\_CODED

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
