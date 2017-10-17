NimBLE Host GATT Server Reference
---------------------------------

Introduction
~~~~~~~~~~~~

The Generic Attribute Profile (GATT) manages all activities involving
services, characteristics, and descriptors. The server half of the GATT
API handles registration and responding to GATT clients.

Header
~~~~~~

.. code:: c

    #include "host/ble_hs.h"

Definitions
~~~~~~~~~~~

`BLE host GATT server definitions <definitions/ble_gatts_defs.html>`__

Functions
~~~~~~~~~

+-------------+----------------+
| Function    | Description    |
+=============+================+
| `ble\_gatts | Queues a set   |
| \_add\_svcs | of service     |
|  <functions | definitions    |
| /ble_gatts_ | for            |
| add_svcs.md | registration.  |
| >`__        |                |
+-------------+----------------+
| `ble\_gatts | Changes        |
| \_svc\_set\ | visibility of  |
| _visibility | a service with |
|  <functions | specified      |
| /ble_gatts_ | handle.        |
| svc_set_vis |                |
| ibility.html> |                |
| `__         |                |
+-------------+----------------+
| `ble\_gatts | Adjusts a host |
| \_count\_cf | configuration  |
| g <function | object's       |
| s/ble_gatts | settings to    |
| _count_cfg. | accommodate    |
| md>`__      | the specified  |
|             | service        |
|             | definition     |
|             | array.         |
+-------------+----------------+
| `ble\_gatts | Retrieves the  |
| \_find\_chr | pair of        |
|  <functions | attribute      |
| /ble_gatts_ | handles        |
| find_chr.md | associated     |
| >`__        | with a local   |
|             | GATT           |
|             | characteristic |
|             | .              |
+-------------+----------------+
| `ble\_gatts | Retrieves the  |
| \_find\_dsc | attribute      |
|  <functions | handle         |
| /ble_gatts_ | associated     |
| find_dsc.md | with a local   |
| >`__        | GATT           |
|             | descriptor.    |
+-------------+----------------+
| `ble\_gatts | Retrieves the  |
| \_find\_svc | attribute      |
|  <functions | handle         |
| /ble_gatts_ | associated     |
| find_svc.md | with a local   |
| >`__        | GATT service.  |
+-------------+----------------+
