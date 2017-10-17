ble\_gap\_set\_prefered\_le\_phy
--------------------------------

.. code:: c

    int
    ble_gap_set_prefered_le_phy(
                uint16_t conn_handle,
                 uint8_t tx_phys_mask,
                 uint8_t rx_phys_mask,
                 uint16_t phy_opts
    )

Description
~~~~~~~~~~~

Set prefered LE PHY mask for given connection.

Parameters
~~~~~~~~~~

+----------------+------------------+
| *Parameter*    | *Description*    |
+================+==================+
| conn\_handle   | The handle       |
|                | corresponding to |
|                | the connection   |
|                | where new PHY    |
|                | mask should be   |
|                | applied.         |
+----------------+------------------+

\| tx\_phys\_mask \| Prefered tx PHY mask is a bit mask of:

.. raw:: html

   <ul>

.. raw:: html

   <li>

BLE\_GAP\_LE\_PHY\_1M\_MASK

.. raw:: html

   </li>

.. raw:: html

   <li>

BLE\_GAP\_LE\_PHY\_2M\_MASK

.. raw:: html

   </li>

.. raw:: html

   <li>

BLE\_GAP\_LE\_PHY\_CODED\_MASK

.. raw:: html

   </li>

.. raw:: html

   </ul>

\| \| rx\_phys\_mask \| Prefered rx PHY mask is a bit mask of:

.. raw:: html

   <ul>

.. raw:: html

   <li>

BLE\_GAP\_LE\_PHY\_1M\_MASK

.. raw:: html

   </li>

.. raw:: html

   <li>

BLE\_GAP\_LE\_PHY\_2M\_MASK

.. raw:: html

   </li>

.. raw:: html

   <li>

BLE\_GAP\_LE\_PHY\_CODED\_MASK

.. raw:: html

   </li>

.. raw:: html

   </ul>

\| \| phy\_opts \| PHY options for coded PHY. One of:

.. raw:: html

   <ul>

.. raw:: html

   <li>

BLE\_GAP\_LE\_PHY\_CODED\_ANY

.. raw:: html

   </li>

.. raw:: html

   <li>

BLE\_GAP\_LE\_PHY\_CODED\_S2

.. raw:: html

   </li>

.. raw:: html

   <li>

BLE\_GAP\_LE\_PHY\_CODED\_S8

.. raw:: html

   </li>

.. raw:: html

   </ul>

\| ### Returned values

+-----------------------------------------------------------------------+---------------------+
| *Value*                                                               | *Condition*         |
+=======================================================================+=====================+
| 0                                                                     | Success.            |
+-----------------------------------------------------------------------+---------------------+
| `Core return code <../../ble_hs_return_codes/#return-codes-core>`__   | Unexpected error.   |
+-----------------------------------------------------------------------+---------------------+
