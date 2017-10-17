ble\_gap\_adv\_set\_phys [experimental] 
----------------------------------------

.. code:: c

    int
    ble_gap_adv_set_phys(uint8_t primary_phy, uint8_t secondary_phy)

Description
~~~~~~~~~~~

Set primary and secondary PHYs for extended advertising procedure.

Parameters
~~~~~~~~~~

None

+---------------+-----------------+
| *Parameter*   | *Description*   |
+===============+=================+
+---------------+-----------------+

\| primary\_phy \| Primary PHY to use for extended advertising
procedure.

.. raw:: html

   <ul>

.. raw:: html

   <li>

BLE\_HCI\_LE\_1M

.. raw:: html

   </li>

.. raw:: html

   <li>

BLE\_HCI\_LE\_CODED

.. raw:: html

   </li>

.. raw:: html

   </ul>

\| \| secondary\_phy \| Secondary PHY to use for extended advertising
procedure.

.. raw:: html

   <ul>

.. raw:: html

   <li>

BLE\_HCI\_LE\_1M

.. raw:: html

   </li>

.. raw:: html

   <li>

BLE\_HCI\_LE\_2M

.. raw:: html

   </li>

.. raw:: html

   <li>

BLE\_HCI\_LE\_CODED

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
