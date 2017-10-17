ble\_att\_set\_preferred\_mtu
-----------------------------

.. code:: c

    int
    ble_att_set_preferred_mtu(uint16_t mtu)

Description
~~~~~~~~~~~

Sets the preferred ATT MTU; the device will indicate this value in all
subseqeunt ATT MTU exchanges. The ATT MTU of a connection is equal to
the lower of the two peers' preferred MTU values. The ATT MTU is what
dictates the maximum size of any message sent during a GATT procedure.
The specified MTU must be within the following range: [23,
BLE\_ATT\_MTU\_MAX]. 23 is a minimum imposed by the Bluetooth
specification; BLE\_ATT\_MTU\_MAX is a NimBLE compile-time setting.

Parameters
~~~~~~~~~~

+---------------+--------------------------+
| *Parameter*   | *Description*            |
+===============+==========================+
| mtu           | The preferred ATT MTU.   |
+---------------+--------------------------+

Returned values
~~~~~~~~~~~~~~~

+-------------------+--------------------------------------------------------+
| *Value*           | *Condition*                                            |
+===================+========================================================+
| 0                 | Success.                                               |
+-------------------+--------------------------------------------------------+
| BLE\_HS\_EINVAL   | The specifeid value is not within the allowed range.   |
+-------------------+--------------------------------------------------------+
