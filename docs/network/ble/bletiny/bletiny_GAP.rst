GAP API for bletiny
-------------------

Generic Access Profile (GAP) defines the generic procedures related to
discovery of Bluetooth devices (idle mode procedures) and link
management aspects of connecting to Bluetooth devices (connecting mode
procedures). It also defines procedures related to use of different
security levels.

Several different modes and procedures may be performed simultaneously
over an LE physical transport. The following modes and procedures are
defined for use over an LE physical transport:

1. **Broadcast mode and observation procedure**

   -  These allow two devices to communicate in a unidirectional
      connectionless manner using the advertising events.

2. **Discovery modes and procedures**

   -  All devices shall be in either non-discoverable mode or one of the
      discoverable modes.
   -  A device in the discoverable mode shall be in either the general
      discoverable mode or the limited discoverable mode.
   -  A device in non-discoverable mode will not be discovered by any
      device that is performing either the general discovery procedure
      or the limited discovery procedure.

3. **Connection modes and procedures**

   -  allow a device to establish a connection to another device.
   -  allow updating of parameters of the connection
   -  allow termination of the connection

4. **Bonding modes and procedures**

   -  Bonding allows two connected devices to exchange and store
      security and identity information to create a trusted
      relationship.
   -  Bonding can occur only between two devices in bondable mode.

Usage API
~~~~~~~~~

+-------+------------+------------------+
| **Ite | **Modes    | **nimBLE         |
| m     | and        | command**        |
| No.** | Procedures |                  |
|       | **         |                  |
+=======+============+==================+
| 1     | Broadcast  | ``b adv conn=non |
|       | Mode       |  disc=x``        |
+-------+------------+------------------+
|       | Observatio | ``b scan dur=x d |
|       | n          | isc=x type=x fil |
|       | Procedure  | t=x``            |
+-------+------------+------------------+
| 2     | Non-Discov | ``b adv conn=x d |
|       | erable     | isc=non``        |
|       | mode       |                  |
+-------+------------+------------------+
|       | Limited    | ``b adv conn=x d |
|       | Discoverab | isc=ltd``        |
|       | le         |                  |
|       | mode       |                  |
+-------+------------+------------------+
|       | General    | ``b adv conn=x d |
|       | Discoverab | isc=gen``        |
|       | le         |                  |
|       | mode       |                  |
+-------+------------+------------------+
|       | Limited    | ``b scan dur=x d |
|       | Discovery  | isc=ltd type=act |
|       | procedure  | ive filt=no_wl`` |
+-------+------------+------------------+
|       | General    | ``b scan dur=x d |
|       | Discovery  | isc=gen type=act |
|       | procedure  | ive filt=no_wl`` |
+-------+------------+------------------+
|       | Name       | ``b scan dur=x`` |
|       | Discovery  | ``b scan cancel` |
|       | procedure  | `                |
|       |            | ``b conn peer_ad |
|       |            | dr_type=x peer_a |
|       |            | ddr=x``          |
|       |            | ``b read conn=x  |
|       |            | uuid=0x2a00``    |
+-------+------------+------------------+
| 3     | Non-connec | ``b adv conn=non |
|       | table      |  disc=x``        |
|       | mode       |                  |
+-------+------------+------------------+
|       | Directed   | ``b adv conn=dir |
|       | connectabl |  [own_addr_type= |
|       | e          | x] [disc=x] [dur |
|       | mode       | =x]``            |
+-------+------------+------------------+
|       | Undirected | ``b adv conn=und |
|       | connectabl |  [own_addr_type= |
|       | e          | x] [disc=x] [dur |
|       | mode       | =x]``            |
+-------+------------+------------------+
|       | Auto       | ``b wl addr_type |
|       | connection | =x addr=x [addr_ |
|       | establishm | type=y addr=y] [ |
|       | ent        | ...]``           |
|       | procedure  | ``b conn addr_ty |
|       |            | pe=wl``          |
+-------+------------+------------------+
|       | General    | ``b scan dur=x`` |
|       | connection | ``b scan cancel` |
|       | establishm | `                |
|       | ent        | ``b conn peer_ad |
|       | procedure  | dr_type=x peer_a |
|       |            | ddr=x``          |
+-------+------------+------------------+
|       | Selective  | ``b wl addr_type |
|       | connection | =x addr=x [addr_ |
|       | establishm | type=y addr=y] [ |
|       | ent        | ...]``           |
|       | procedure  | ``b scan filt=us |
|       |            | e_wl dur=x``     |
|       |            | ``b scan cancel` |
|       |            | `                |
|       |            | ``b conn peer_ad |
|       |            | dr_type=x peer_a |
|       |            | ddr=x [own_addr_ |
|       |            | type=x]``        |
+-------+------------+------------------+
|       | Direct     | ``b conn addr_ty |
|       | connection | pe=x addr=x [par |
|       | establishm | ams]``           |
|       | ent        |                  |
|       | procedure  |                  |
+-------+------------+------------------+
|       | Connection | ``b update conn= |
|       | parameter  | x <params>``     |
|       | update     |                  |
|       | procedure  |                  |
+-------+------------+------------------+
|       | Terminate  | ``b term conn=x` |
|       | connection | `                |
|       | procedure  |                  |
+-------+------------+------------------+
| 4     | Non-Bondab | ``b set sm_data  |
|       | le         | bonding=0``      |
|       | mode       | [\*]             |
+-------+------------+------------------+
|       | Bondable   | ``b set sm_data  |
|       | mode       | bonding=1``      |
|       |            | [\*]             |
+-------+------------+------------------+
|       | Bonding    | ``b sec start co |
|       | procedure  | nn=x``           |
|       |            | [\*]             |
+-------+------------+------------------+

**[\*]** Security is disabled by default in bletiny. To use the bonding
modes and procedures, add ``BLE_SM_LEGACY: 1`` or ``BLE_SM_SC: 1`` to
your syscfg.yml file depending on your needs.

Address Types
~~~~~~~~~~~~~

+--------------------+-------------------------------------------------------+
| *bletiny string*   | *Description*                                         |
+====================+=======================================================+
| public             | Public address.                                       |
+--------------------+-------------------------------------------------------+
| random             | Random static address.                                |
+--------------------+-------------------------------------------------------+
| rpa\_pub           | Resolvable private address, public identity.          |
+--------------------+-------------------------------------------------------+
| rpa\_rnd           | Resolvable private address, random static identity.   |
+--------------------+-------------------------------------------------------+
| wl                 | Use white list; ignore peer\_addr parameter.          |
+--------------------+-------------------------------------------------------+

Connection Types
~~~~~~~~~~~~~~~~

Bluetooth Specification Version 5.0 allows for different types of
connections:

+--------------------------------------------------+---------------------------------+
| *Description*                                    | *bletiny ext parameter value*   |
+==================================================+=================================+
| Legacy connection                                | none                            |
+--------------------------------------------------+---------------------------------+
| Extended connection with 1M PHY                  | 1M                              |
+--------------------------------------------------+---------------------------------+
| Extended connection with coded PHY               | coded                           |
+--------------------------------------------------+---------------------------------+
| Extended connection with both 1M and coded PHY   | both                            |
+--------------------------------------------------+---------------------------------+
| Extended connection with 1M, 2M and coded PHYs   | all                             |
+--------------------------------------------------+---------------------------------+

Connection Parameters
~~~~~~~~~~~~~~~~~~~~~

Connection parameter definitions can be found in Section 7.8.12 of the
BLUETOOTH SPECIFICATION Version 5.0 [Vol 2, Part E].

Connection parameters for all types of connections:

+-------------+---------------------------------------------------+------------+
| *Name*      | *Description*                                     | *bletiny   |
|             |                                                   | string*    |
+=============+===================================================+============+
| Connection  | Parameter indicating the type of connection       | ext        |
| Type        |                                                   |            |
+-------------+---------------------------------------------------+------------+
| Peer\_Addre | Whether the peer is using a public or random      | peer\_addr |
| ss\_Type    | address (see Address types table).                | \_type     |
+-------------+---------------------------------------------------+------------+
| Peer\_Addre | The 6-byte device address of the peer; ignored if | peer\_addr |
| ss          | white list is used                                |            |
+-------------+---------------------------------------------------+------------+
| Own\_Addres | The type of address to use when initiating the    | own\_addr\ |
| s\_Type     | connection (see Address types table)              | _type      |
+-------------+---------------------------------------------------+------------+
| Duration    | Number of milliseconds before aborting the        | dur        |
|             | connect attempt                                   |            |
+-------------+---------------------------------------------------+------------+

Connection parameters for legacy and 1M PHY extended connection:

+------------+-----------------------------------------------------+-----------+
| *Name*     | *Description*                                       | *bletiny  |
|            |                                                     | string*   |
+============+=====================================================+===========+
| LE\_Scan\_ | Recommendation from the Host on how long the        | scan\_itv |
| Interval   | Controller should scan                              | l         |
+------------+-----------------------------------------------------+-----------+
| LE\_Scan\_ | Recommendation from the Host on how frequently the  | scan\_win |
| Window     | Controller should scan                              | dow       |
+------------+-----------------------------------------------------+-----------+
| Conn\_Inte | Defines minimum allowed connection interval         | itvl\_min |
| rval\_Min  |                                                     |           |
+------------+-----------------------------------------------------+-----------+
| Conn\_Inte | Defines maximum allowed connection interval         | itvl\_max |
| rval\_Max  |                                                     |           |
+------------+-----------------------------------------------------+-----------+
| Conn\_Late | Defines the maximum allowed connection latency      | latency   |
| ncy        |                                                     |           |
+------------+-----------------------------------------------------+-----------+
| Supervisio | Link supervision timeout for the connection.        | timeout   |
| n\_Timeout |                                                     |           |
+------------+-----------------------------------------------------+-----------+
| Minimum\_C | Informative parameter providing the Controller with | min\_ce\_ |
| E\_Length  | the expected minimum length of the connection event | len       |
+------------+-----------------------------------------------------+-----------+
| Maximum\_C | Informative parameter providing the Controller with | max\_ce\_ |
| E\_Length  | the expected maximum length of the connection event | len       |
+------------+-----------------------------------------------------+-----------+

Extended Connection parameters for coded PHY connection:

+------------+-----------------------------------------------------+-----------+
| *Name*     | *Description*                                       | *bletiny  |
|            |                                                     | string*   |
+============+=====================================================+===========+
| LE\_Scan\_ | Recommendation from the Host on how long the        | coded\_sc |
| Interval   | Controller should scan                              | an\_itvl  |
+------------+-----------------------------------------------------+-----------+
| LE\_Scan\_ | Recommendation from the Host on how frequently the  | coded\_sc |
| Window     | Controller should scan                              | an\_windo |
|            |                                                     | w         |
+------------+-----------------------------------------------------+-----------+
| Conn\_Inte | Defines minimum allowed connection interval         | coded\_it |
| rval\_Min  |                                                     | vl\_min   |
+------------+-----------------------------------------------------+-----------+
| Conn\_Inte | Defines maximum allowed connection interval         | coded\_it |
| rval\_Max  |                                                     | vl\_max   |
+------------+-----------------------------------------------------+-----------+
| Conn\_Late | Defines the maximum allowed connection latency      | coded\_la |
| ncy        |                                                     | tency     |
+------------+-----------------------------------------------------+-----------+
| Supervisio | Link supervision timeout for the connection.        | coded\_ti |
| n\_Timeout |                                                     | meout     |
+------------+-----------------------------------------------------+-----------+
| Minimum\_C | Informative parameter providing the Controller with | coded\_mi |
| E\_Length  | the expected minimum length of the connection event | n\_ce\_le |
|            |                                                     | n         |
+------------+-----------------------------------------------------+-----------+
| Maximum\_C | Informative parameter providing the Controller with | coded\_ma |
| E\_Length  | the expected maximum length of the connection event | x\_ce\_le |
|            |                                                     | n         |
+------------+-----------------------------------------------------+-----------+

Extended Connection parameters for 2M PHY connection:

+------------+-----------------------------------------------------+-----------+
| *Name*     | *Description*                                       | *bletiny  |
|            |                                                     | string*   |
+============+=====================================================+===========+
| Conn\_Inte | Defines minimum allowed connection interval         | 2M\_itvl\ |
| rval\_Min  |                                                     | _min      |
+------------+-----------------------------------------------------+-----------+
| Conn\_Inte | Defines maximum allowed connection interval         | 2M\_itvl\ |
| rval\_Max  |                                                     | _max      |
+------------+-----------------------------------------------------+-----------+
| Conn\_Late | Defines the maximum allowed connection latency      | 2M\_laten |
| ncy        |                                                     | cy        |
+------------+-----------------------------------------------------+-----------+
| Supervisio | Link supervision timeout for the connection.        | 2M\_timeo |
| n\_Timeout |                                                     | ut        |
+------------+-----------------------------------------------------+-----------+
| Minimum\_C | Informative parameter providing the Controller with | 2M\_min\_ |
| E\_Length  | the expected minimum length of the connection event | ce\_len   |
+------------+-----------------------------------------------------+-----------+
| Maximum\_C | Informative parameter providing the Controller with | 2M\_max\_ |
| E\_Length  | the expected maximum length of the connection event | ce\_len   |
+------------+-----------------------------------------------------+-----------+

Scan Types
~~~~~~~~~~

Bluetooth Specification Version 5.0 allows for different types of scan:

+--------------------------------------------+---------------------------------+
| *Description*                              | *bletiny ext parameter value*   |
+============================================+=================================+
| Legacy scan                                | 0                               |
+--------------------------------------------+---------------------------------+
| Extended scan with 1M PHY                  | 1M                              |
+--------------------------------------------+---------------------------------+
| Extended scan with coded PHY               | coded                           |
+--------------------------------------------+---------------------------------+
| Extended scan with both 1M and coded PHY   | both                            |
+--------------------------------------------+---------------------------------+

Scan Parameters
~~~~~~~~~~~~~~~

Scan parameter definitions can be found in Section 7.8.10 of the
BLUETOOTH SPECIFICATION Version 5.0 [Vol 2, Part E].

+-----------------+-----------------------------------------------+-------------+
| *Name*          | *Description*                                 | *bletiny    |
|                 |                                               | string*     |
+=================+===============================================+=============+
| Scan Type       | Parameter indicating the type of scan         | ext         |
+-----------------+-----------------------------------------------+-------------+
| LE\_Scan\_Type  | Controls the type of scan to perform (passive | passive     |
|                 | or active)                                    |             |
+-----------------+-----------------------------------------------+-------------+
| LE\_Scan\_Inter | Recommendation from the Host on how long the  | itvl        |
| val             | Controller should scan                        |             |
+-----------------+-----------------------------------------------+-------------+
| LE\_Scan\_Windo | Recommendation from the Host on how           | window      |
| w               | frequently the Controller should scan         |             |
+-----------------+-----------------------------------------------+-------------+
| Scanning\_Filte | Policy about which advertising packets to     | filter      |
| r\_Policy       | accept                                        |             |
+-----------------+-----------------------------------------------+-------------+
| Duration        | Number of milliseconds before canceling scan  | dur         |
|                 | procedure                                     |             |
+-----------------+-----------------------------------------------+-------------+
| Limited         | Limited scan procedure                        | ltd         |
+-----------------+-----------------------------------------------+-------------+
| No duplicates   | Filter out duplicates in shell output         | nodups      |
+-----------------+-----------------------------------------------+-------------+
| Own\_Address\_T | The type of address to use when scanning (see | own\_addr\_ |
| ype             | Address types table)                          | type        |
+-----------------+-----------------------------------------------+-------------+

Extended Scan parameters:

+--------------+--------------------------------------------------+--------------+
| *Name*       | *Description*                                    | *bletiny     |
|              |                                                  | string*      |
+==============+==================================================+==============+
| Duration     | Number of milliseconds before canceling scan     | duration     |
|              | procedure                                        |              |
+--------------+--------------------------------------------------+--------------+
| Period       | Period in which scan should be enabled for       | period       |
|              | specified duration                               |              |
+--------------+--------------------------------------------------+--------------+
| LE\_Scan\_Ty | Controls the type of scan to perform (passive or | lr\_passive  |
| pe           | active)                                          |              |
+--------------+--------------------------------------------------+--------------+
| LE\_Scan\_In | Recommendation from the Host on how long the     | lr\_itvl     |
| terval       | Controller should scan                           |              |
+--------------+--------------------------------------------------+--------------+
| LE\_Scan\_Wi | Recommendation from the Host on how frequently   | lr\_window   |
| ndow         | the Controller should scan                       |              |
+--------------+--------------------------------------------------+--------------+

Advertisment Parameters
~~~~~~~~~~~~~~~~~~~~~~~

+-----------+---------------------+--------------------------------+---------------+
| *bletiny  | *Description*       | *Notes*                        | *Default*     |
| string*   |                     |                                |               |
+===========+=====================+================================+===============+
| conn      | Connectable mode    | See Connectable Modes table.   | und           |
+-----------+---------------------+--------------------------------+---------------+
| disc      | Discoverable mode   | See Discoverable Modes table.  | gen           |
+-----------+---------------------+--------------------------------+---------------+
| own\_addr | The type of address | See Address Types table.       | public        |
| \_type    | to advertise with   |                                |               |
+-----------+---------------------+--------------------------------+---------------+
| peer\_add | The peer's address  | Only used for directed         | public        |
| r\_type   | type                | advertising; see Address Types |               |
|           |                     | table.                         |               |
+-----------+---------------------+--------------------------------+---------------+
| peer\_add | The peer's address  | Only used for directed         | N/A           |
| r         |                     | advertising                    |               |
+-----------+---------------------+--------------------------------+---------------+
| chan\_map |                     |                                | 0             |
+-----------+---------------------+--------------------------------+---------------+
| filt      | The filter policy   | See Advertisement Filter       | none          |
|           |                     | Policies table.                |               |
+-----------+---------------------+--------------------------------+---------------+
| itvl\_min |                     | units=0.625ms                  | non: 100ms;   |
|           |                     |                                | und/dir: 30ms |
+-----------+---------------------+--------------------------------+---------------+
| itvl\_max |                     | units=0.625ms                  | non: 150ms;   |
|           |                     |                                | und/dir: 60ms |
+-----------+---------------------+--------------------------------+---------------+
| hd        | Whether to use      | 0/1                            | 0             |
|           | high-duty-cycle     |                                |               |
+-----------+---------------------+--------------------------------+---------------+
| dur       |                     | Milliseconds                   | Forever       |
+-----------+---------------------+--------------------------------+---------------+

Extended Advertising parameters:

+----------+-------------------------------------------+---------+----------------+
| *bletiny | *Description*                             | *Notes* | *Default*      |
| string*  |                                           |         |                |
+==========+===========================================+=========+================+
| tx\_powe | Maximum power level at which the          | -127 -  | 127 (Host has  |
| r        | advertising packets are to be transmitted | 127 dBm | no preference) |
+----------+-------------------------------------------+---------+----------------+
| primary\ | PHY on which the advertising packets are  |         | none           |
| _phy     | transmitted on the primary advertising    |         |                |
|          | channel                                   |         |                |
+----------+-------------------------------------------+---------+----------------+
| secondar | PHY on which the advertising packets are  |         | primary\_phy   |
| y\_phy   | transmitted on the secondary advertising  |         |                |
|          | channel                                   |         |                |
+----------+-------------------------------------------+---------+----------------+

Advertising PHY Types
~~~~~~~~~~~~~~~~~~~~~

+---------------------------------------+-----------------------------+
| *Description*                         | *bletiny parameter value*   |
+=======================================+=============================+
| Legacy advertising                    | none                        |
+---------------------------------------+-----------------------------+
| Extended advertising with 1M PHY      | 1M                          |
+---------------------------------------+-----------------------------+
| Extended advertising with 2M PHY      | 2M                          |
+---------------------------------------+-----------------------------+
| Extended advertising with coded PHY   | coded                       |
+---------------------------------------+-----------------------------+

Advertisement Filter Policies
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

+--------------------+------------------+------------+
| *btshell string*   | *Description*    | *Notes*    |
+====================+==================+============+
| none               | No filtering. No | Default    |
|                    | whitelist used.  |            |
+--------------------+------------------+------------+
| scan               | Process all      |            |
|                    | connection       |            |
|                    | requests but     |            |
|                    | only scans from  |            |
|                    | white list.      |            |
+--------------------+------------------+------------+
| conn               | Process all scan |            |
|                    | request but only |            |
|                    | connection       |            |
|                    | requests from    |            |
|                    | white list.      |            |
+--------------------+------------------+------------+
| both               | Ignore all scan  |            |
|                    | and connection   |            |
|                    | requests unless  |            |
|                    | in white list.   |            |
+--------------------+------------------+------------+
