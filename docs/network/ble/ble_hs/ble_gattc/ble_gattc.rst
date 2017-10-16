NimBLE Host GATT Client Reference
---------------------------------

Introduction
~~~~~~~~~~~~

The Generic Attribute Profile (GATT) manages all activities involving
services, characteristics, and descriptors. The client half of the GATT
API initiates GATT procedures.

Header
~~~~~~

.. code:: c

    #include "host/ble_hs.h"

Definitions
~~~~~~~~~~~

`BLE host GATT client definitions <definitions/ble_gattc_defs.html>`__

Functions
~~~~~~~~~

+-------------+----------------+
| Function    | Description    |
+=============+================+
| `ble\_gattc | Initiates GATT |
| \_disc\_all | procedure:     |
| \_chrs <fun | Discover All   |
| ctions/ble_ | Characteristic |
| gattc_disc_ | s              |
| all_chrs.md | of a Service.  |
| >`__        |                |
+-------------+----------------+
| `ble\_gattc | Initiates GATT |
| \_disc\_all | procedure:     |
| \_dscs <fun | Discover All   |
| ctions/ble_ | Characteristic |
| gattc_disc_ | Descriptors.   |
| all_dscs.md |                |
| >`__        |                |
+-------------+----------------+
| `ble\_gattc | Initiates GATT |
| \_disc\_all | procedure:     |
| \_svcs <fun | Discover All   |
| ctions/ble_ | Primary        |
| gattc_disc_ | Services.      |
| all_svcs.md |                |
| >`__        |                |
+-------------+----------------+
| `ble\_gattc | Initiates GATT |
| \_disc\_chr | procedure:     |
| s\_by\_uuid | Discover       |
|  <functions | Characteristic |
| /ble_gattc_ | s              |
| disc_chrs_b | by UUID.       |
| y_uuid.html>` |                |
| __          |                |
+-------------+----------------+
| `ble\_gattc | Initiates GATT |
| \_disc\_svc | procedure:     |
| \_by\_uuid  | Discover       |
| <functions/ | Primary        |
| ble_gattc_d | Service by     |
| isc_svc_by_ | Service UUID.  |
| uuid.html>`__ |                |
+-------------+----------------+
| `ble\_gattc | Initiates GATT |
| \_exchange\ | procedure:     |
| _mtu <funct | Exchange MTU.  |
| ions/ble_ga |                |
| ttc_exchang |                |
| e_mtu.html>`_ |                |
| _           |                |
+-------------+----------------+
| `ble\_gattc | Initiates GATT |
| \_find\_inc | procedure:     |
| \_svcs <fun | Find Included  |
| ctions/ble_ | Services.      |
| gattc_find_ |                |
| inc_svcs.md |                |
| >`__        |                |
+-------------+----------------+
| `ble\_gattc | Sends a        |
| \_indicate  | characteristic |
| <functions/ | indication.    |
| ble_gattc_i |                |
| ndicate.html> |                |
| `__         |                |
+-------------+----------------+
| `ble\_gattc | Sends a        |
| \_indicate\ | characteristic |
| _custom <fu | indication.    |
| nctions/ble |                |
| _gattc_indi |                |
| cate_custom |                |
| .html>`__     |                |
+-------------+----------------+
| `ble\_gattc | Sends a        |
| \_notify <f | characteristic |
| unctions/bl | notification.  |
| e_gattc_not |                |
| ify.html>`__  |                |
+-------------+----------------+
| `ble\_gattc | Sends a        |
| \_notify\_c | "free-form"    |
| ustom <func | characteristic |
| tions/ble_g | notification.  |
| attc_notify |                |
| _custom.html> |                |
| `__         |                |
+-------------+----------------+
| `ble\_gattc | Initiates GATT |
| \_read <fun | procedure:     |
| ctions/ble_ | Read           |
| gattc_read. | Characteristic |
| md>`__      | Value.         |
+-------------+----------------+
| `ble\_gattc | Initiates GATT |
| \_read\_by\ | procedure:     |
| _uuid <func | Read Using     |
| tions/ble_g | Characteristic |
| attc_read_b | UUID.          |
| y_uuid.html>` |                |
| __          |                |
+-------------+----------------+
| `ble\_gattc | Initiates GATT |
| \_read\_lon | procedure:     |
| g <function | Read Long      |
| s/ble_gattc | Characteristic |
| _read_long. | Values.        |
| md>`__      |                |
+-------------+----------------+
| `ble\_gattc | Initiates GATT |
| \_read\_mul | procedure:     |
| t <function | Read Multiple  |
| s/ble_gattc | Characteristic |
| _read_mult. | Values.        |
| md>`__      |                |
+-------------+----------------+
| `ble\_gattc | Initiates GATT |
| \_write <fu | procedure:     |
| nctions/ble | Write          |
| _gattc_writ | Characteristic |
| e.html>`__    | Value.         |
+-------------+----------------+
| `ble\_gattc | Initiates GATT |
| \_write\_fl | procedure:     |
| at <functio | Write          |
| ns/ble_gatt | Characteristic |
| c_write_fla | Value (flat    |
| t.html>`__    | buffer         |
|             | version).      |
+-------------+----------------+
| `ble\_gattc | Initiates GATT |
| \_write\_lo | procedure:     |
| ng <functio | Write Long     |
| ns/ble_gatt | Characteristic |
| c_write_lon | Values.        |
| g.html>`__    |                |
+-------------+----------------+
| `ble\_gattc | Initiates GATT |
| \_write\_no | procedure:     |
| \_rsp <func | Write Without  |
| tions/ble_g | Response.      |
| attc_write_ |                |
| no_rsp.html>` |                |
| __          |                |
+-------------+----------------+
| `ble\_gattc | Initiates GATT |
| \_write\_no | procedure:     |
| \_rsp\_flat | Write Without  |
|  <functions | Response.      |
| /ble_gattc_ |                |
| write_no_rs |                |
| p_flat.html>` |                |
| __          |                |
+-------------+----------------+
| `ble\_gattc | Initiates GATT |
| \_write\_re | procedure:     |
| liable <fun | Reliable       |
| ctions/ble_ | Writes.        |
| gattc_write |                |
| _reliable.m |                |
| d>`__       |                |
+-------------+----------------+
