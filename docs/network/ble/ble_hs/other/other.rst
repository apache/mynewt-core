NimBLE Host Other Reference
---------------------------

Introduction
~~~~~~~~~~~~

This section is a reference on miscellaneous parts of the NimBLE host
which don't fit anywhere else!

Header
~~~~~~

.. code:: c

    #include "host/ble_hs.h"

Definitions
~~~~~~~~~~~

None.

Functions
~~~~~~~~~

+-------------+----------------+
| Function    | Description    |
+=============+================+
| `ble\_eddys | Configures the |
| tone\_set\_ | device to      |
| adv\_data\_ | advertise      |
| uid <functi | eddystone UID  |
| ons/ble_edd | beacons.       |
| ystone_set_ |                |
| adv_data_ui |                |
| d.html>`__    |                |
+-------------+----------------+
| `ble\_eddys | Configures the |
| tone\_set\_ | device to      |
| adv\_data\_ | advertise      |
| url <functi | eddystone URL  |
| ons/ble_edd | beacons.       |
| ystone_set_ |                |
| adv_data_ur |                |
| l.html>`__    |                |
+-------------+----------------+
| `ble\_hs\_e | Designates the |
| vq\_set <fu | specified      |
| nctions/ble | event queue    |
| _hs_evq_set | for NimBLE     |
| .html>`__     | host work.     |
+-------------+----------------+
| `ble\_hs\_m | Allocates an   |
| buf\_att\_p | mbuf suitable  |
| kt <functio | for an ATT     |
| ns/ble_hs_m | command        |
| buf_att_pkt | packet.        |
| .html>`__     |                |
+-------------+----------------+
| `ble\_hs\_m | Allocates a an |
| buf\_from\_ | mbuf and fills |
| flat <funct | it with the    |
| ions/ble_hs | contents of    |
| _mbuf_from_ | the specified  |
| flat.html>`__ | flat buffer.   |
+-------------+----------------+
| `ble\_hs\_m | Copies the     |
| buf\_to\_fl | contents of an |
| at <functio | mbuf into the  |
| ns/ble_hs_m | specified flat |
| buf_to_flat | buffer.        |
| .html>`__     |                |
+-------------+----------------+
| `ble\_hs\_s | Causes the     |
| ched\_reset | host to reset  |
|  <functions | the NimBLE     |
| /ble_hs_sch | stack as soon  |
| ed_reset.md | as possible.   |
| >`__        |                |
+-------------+----------------+
| `ble\_hs\_s | Indicates      |
| ynced <func | whether the    |
| tions/ble_h | host has       |
| s_synced.md | synchronized   |
| >`__        | with the       |
|             | controller.    |
+-------------+----------------+
| `ble\_ibeac | Configures the |
| on\_set\_ad | device to      |
| v\_data <fu | advertise      |
| nctions/ble | iBeacons.      |
| _ibeacon_se |                |
| t_adv_data. |                |
| md>`__      |                |
+-------------+----------------+
| `ble\_uuid\ | Compares two   |
| _cmp <funct | Bluetooth      |
| ions/ble_uu | UUIDs.         |
| id_cmp.html>` |                |
| __          |                |
+-------------+----------------+
| `ble\_uuid\ | Constructs a   |
| _init\_from | UUID object    |
| \_buf <func | from a byte    |
| tions/ble_u | array.         |
| uid_init_fr |                |
| om_buf.html>` |                |
| __          |                |
+-------------+----------------+
| `ble\_uuid\ | Converts the   |
| _to\_str <f | specified UUID |
| unctions/bl | to its string  |
| e_uuid_to_s | representation |
| tr.html>`__   | .              |
+-------------+----------------+
| `ble\_uuid\ | Converts the   |
| _u16 <funct | specified      |
| ions/ble_uu | 16-bit UUID to |
| id_u16.html>` | a uint16\_t.   |
| __          |                |
+-------------+----------------+
