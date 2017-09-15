NimBLE Host ATT Client Reference
--------------------------------

Introduction
~~~~~~~~~~~~

The Attribute Protocol (ATT) is a mid-level protocol that all BLE
devices use to exchange data. Data is exchanged when an ATT client reads
or writes an attribute belonging to an ATT server. Any device that needs
to send or receive data must support both the client and server
functionality of the ATT protocol. The only devices which do not support
ATT are the most basic ones: broadcasters and observers (i.e., beaconing
devices and listening devices).

Most ATT functionality is not interesting to an application. Rather than
use ATT directly, an application uses the higher level GATT profile,
which sits directly above ATT in the host. NimBLE exposes the few bits
of ATT functionality which are not encompassed by higher level GATT
functions. This section documents the ATT functionality that the NimBLE
host exposes to the application.

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
| `ble\_att\_ | Retrieves the  |
| mtu <functi | ATT MTU of the |
| ons/ble_att | specified      |
| _mtu.html>`__ | connection.    |
+-------------+----------------+
| `ble\_att\_ | Retrieves the  |
| preferred\_ | preferred ATT  |
| mtu <functi | MTU.           |
| ons/ble_att |                |
| _preferred_ |                |
| mtu.html>`__  |                |
+-------------+----------------+
| `ble\_att\_ | Sets the       |
| set\_prefer | preferred ATT  |
| red\_mtu <f | MTU; the       |
| unctions/bl | device will    |
| e_att_set_p | indicate this  |
| referred_mt | value in all   |
| u.html>`__    | subseqeunt ATT |
|             | MTU exchanges. |
+-------------+----------------+
| `ble\_att\_ | Reads a        |
| svr\_read\_ | locally        |
| local <func | registered     |
| tions/ble_a | attribute.     |
| tt_svr_read |                |
| _local.html>` |                |
| __          |                |
+-------------+----------------+
| `ble\_att\_ | Writes a       |
| svr\_write\ | locally        |
| _local <fun | registered     |
| ctions/ble_ | attribute.     |
| att_svr_wri |                |
| te_local.md |                |
| >`__        |                |
+-------------+----------------+
