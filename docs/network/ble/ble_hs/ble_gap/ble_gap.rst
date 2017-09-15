NimBLE Host GAP Reference
-------------------------

Introduction
~~~~~~~~~~~~

The Generic Access Profile (GAP) is responsible for all connecting,
advertising, scanning, and connection updating operations.

Header
~~~~~~

.. code:: c

    #include "host/ble_hs.h"

Definitions
~~~~~~~~~~~

`BLE host GAP definitions <definitions/ble_gap_defs.html>`__

Functions
~~~~~~~~~

+-------------+----------------+
| Function    | Description    |
+=============+================+
| `ble\_gap\_ | Indicates      |
| adv\_active | whether an     |
|  <functions | advertisement  |
| /ble_gap_ad | procedure is   |
| v_active.md | currently in   |
| >`__        | progress.      |
+-------------+----------------+
| `ble\_gap\_ | Configures the |
| adv\_rsp\_s | data to        |
| et\_data <f | include in     |
| unctions/bl | subsequent     |
| e_gap_adv_r | scan           |
| sp_set_data | responses.     |
| .html>`__     |                |
+-------------+----------------+
| `ble\_gap\_ | Configures the |
| adv\_rsp\_s | fields to      |
| et\_fields  | include in     |
| <functions/ | subsequent     |
| ble_gap_adv | scan           |
| _rsp_set_fi | responses.     |
| elds.html>`__ |                |
+-------------+----------------+
| `ble\_gap\_ | Configures the |
| adv\_set\_d | data to        |
| ata <functi | include in     |
| ons/ble_gap | subsequent     |
| _adv_set_da | advertisements |
| ta.html>`__   | .              |
+-------------+----------------+
| `ble\_gap\_ | Configures the |
| adv\_set\_f | fields to      |
| ields <func | include in     |
| tions/ble_g | subsequent     |
| ap_adv_set_ | advertisements |
| fields.html>` | .              |
| __          |                |
+-------------+----------------+
| `ble\_gap\_ |                |
| adv\_set\_p | [experimental] |
| hys <functi | Configures     |
| ons/ble_gap | primary and    |
| _adv_set_ph | secondary PHYs |
| ys.html>`__   | to use in      |
|             | subsequent     |
|             | extended       |
|             | advertisements |
|             | from Bluetooth |
|             | 5.             |
+-------------+----------------+
| `ble\_gap\_ |                |
| adv\_set\_t | [experimental] |
| x\_power <f | Configures Tx  |
| unctions/bl | Power level to |
| e_gap_adv_s | use in         |
| et_tx_power | subsequent     |
| .html>`__     | extended       |
|             | advertisements |
|             | from Bluetooth |
|             | 5.             |
+-------------+----------------+
| `ble\_gap\_ | Initiates      |
| adv\_start  | advertising.   |
| <functions/ |                |
| ble_gap_adv |                |
| _start.html>` |                |
| __          |                |
+-------------+----------------+
| `ble\_gap\_ | Stops the      |
| adv\_stop < | currently-acti |
| functions/b | ve             |
| le_gap_adv_ | advertising    |
| stop.html>`__ | procedure.     |
+-------------+----------------+
| `ble\_gap\_ | Indicates      |
| conn\_activ | whether a      |
| e <function | connect        |
| s/ble_gap_c | procedure is   |
| onn_active. | currently in   |
| md>`__      | progress.      |
+-------------+----------------+
| `ble\_gap\_ | Aborts a       |
| conn\_cance | connect        |
| l <function | procedure in   |
| s/ble_gap_c | progress.      |
| onn_cancel. |                |
| md>`__      |                |
+-------------+----------------+
| `ble\_gap\_ | Searches for a |
| conn\_find  | connection     |
| <functions/ | with the       |
| ble_gap_con | specified      |
| n_find.html>` | handle.        |
| __          |                |
+-------------+----------------+
| `ble\_gap\_ | Retrieves the  |
| conn\_rssi  | most-recently  |
| <functions/ | measured RSSI  |
| ble_gap_con | for the        |
| n_rssi.html>` | specified      |
| __          | connection.    |
+-------------+----------------+
| `ble\_gap\_ | Initiates a    |
| connect <fu | connect        |
| nctions/ble | procedure.     |
| _gap_connec |                |
| t.html>`__    |                |
+-------------+----------------+
| `ble\_gap\_ |                |
| ext\_connec | [experimental] |
| t <function | Same as above  |
| s/ble_gap_e | but using      |
| xt_connect. | extended       |
| md>`__      | connect from   |
|             | Bluetooth 5.   |
+-------------+----------------+
| `ble\_gap\_ | Performs the   |
| disc <funct | Limited or     |
| ions/ble_ga | General        |
| p_disc.html>` | Discovery      |
| __          | Procedures.    |
+-------------+----------------+
| `ble\_gap\_ |                |
| ext\_disc < | [experimental] |
| functions/b | Same as above  |
| le_gap_ext_ | but using      |
| disc.html>`__ | extended       |
|             | advertising    |
|             | from Bluetooth |
|             | 5.             |
+-------------+----------------+
| `ble\_gap\_ | Indicates      |
| disc\_activ | whether a      |
| e <function | discovery      |
| s/ble_gap_d | procedure is   |
| isc_active. | currently in   |
| md>`__      | progress.      |
+-------------+----------------+
| `ble\_gap\_ | Cancels the    |
| disc\_cance | discovery      |
| l <function | procedure      |
| s/ble_gap_d | currently in   |
| isc_cancel. | progress.      |
| md>`__      |                |
+-------------+----------------+
| `ble\_gap\_ | Initiates the  |
| security\_i | GAP encryption |
| nitiate <fu | procedure.     |
| nctions/ble |                |
| _gap_securi |                |
| ty_initiate |                |
| .html>`__     |                |
+-------------+----------------+
| `ble\_gap\_ | Configures a   |
| set\_event\ | connection to  |
| _cb <functi | use the        |
| ons/ble_gap | specified GAP  |
| _set_event_ | event          |
| cb.html>`__   | callback.      |
+-------------+----------------+
| `ble\_gap\_ | Terminates an  |
| terminate < | established    |
| functions/b | connection.    |
| le_gap_term |                |
| inate.html>`_ |                |
| _           |                |
+-------------+----------------+
| `ble\_gap\_ | Initiates a    |
| update\_par | connection     |
| ams <functi | parameter      |
| ons/ble_gap | update         |
| _update_par | procedure.     |
| ams.html>`__  |                |
+-------------+----------------+
| `ble\_gap\_ | Overwrites the |
| wl\_set <fu | controller's   |
| nctions/ble | white list     |
| _gap_wl_set | with the       |
| .html>`__     | specified      |
|             | contents.      |
+-------------+----------------+
| `ble\_gap\_ | Set privacy    |
| set\_priv\_ | mode for peer  |
| mode <funct | device.        |
| ions/ble_ga |                |
| p_set_priv_ |                |
| mode.html>`__ |                |
+-------------+----------------+
| `ble\_gap\_ | Read PHY on    |
| read\_le\_p | the            |
| hy <functio | connections.   |
| ns/ble_gap_ |                |
| read_le_phy |                |
| .html>`__     |                |
+-------------+----------------+
| `ble\_gap\_ | Set default    |
| set\_prefer | prefered PHY   |
| ed\_default | mode for new   |
| \_le\_phy < | connections.   |
| functions/b |                |
| le_gap_set_ |                |
| prefered_de |                |
| fault_le_ph |                |
| y.html>`__    |                |
+-------------+----------------+
| `ble\_gap\_ | Set prefered   |
| set\_prefer | PHY mode for   |
| ed\_le\_phy | the            |
|  <functions | connections.   |
| /ble_gap_se |                |
| t_prefered_ |                |
| le_phy.html>` |                |
| __          |                |
+-------------+----------------+
