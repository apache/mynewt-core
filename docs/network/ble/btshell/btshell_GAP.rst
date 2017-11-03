GAP API for btshell
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
| 1     | Broadcast  | ``advertise conn |
|       | Mode       | =non discov=x``  |
+-------+------------+------------------+
|       | Observatio | ``scan duration= |
|       | n          | x passive=x filt |
|       | Procedure  | er=x``           |
+-------+------------+------------------+
| 2     | Non-Discov | ``advertise conn |
|       | erable     | =x discov=non``  |
|       | mode       |                  |
+-------+------------+------------------+
|       | Limited    | ``advertise conn |
|       | Discoverab | =x discov=ltd``  |
|       | le         |                  |
|       | mode       |                  |
+-------+------------+------------------+
|       | General    | ``advertise conn |
|       | Discoverab | =x discov=gen``  |
|       | le         |                  |
|       | mode       |                  |
+-------+------------+------------------+
|       | Limited    | ``scan duration= |
|       | Discovery  | x discov=ltd pas |
|       | procedure  | sive=0 filter=no |
|       |            | _wl``            |
+-------+------------+------------------+
|       | General    | ``scan duration= |
|       | Discovery  | x discov=gen pas |
|       | procedure  | sive=0 filter=no |
|       |            | _wl``            |
+-------+------------+------------------+
|       | Name       | ``scan duration= |
|       | Discovery  | x``              |
|       | procedure  | ``scan cancel``  |
|       |            | ``connect peer_a |
|       |            | ddr_type=x peer_ |
|       |            | addr=x``         |
|       |            | ``gatt-read conn |
|       |            | =x uuid=0x2a00`` |
+-------+------------+------------------+
| 3     | Non-connec | ``advertise conn |
|       | table      | =non discov=x``  |
|       | mode       |                  |
+-------+------------+------------------+
|       | Directed   | ``advertise conn |
|       | connectabl | =dir [own_addr_t |
|       | e          | ype=x] [discov=x |
|       | mode       | ] [duration=x]`` |
+-------+------------+------------------+
|       | Undirected | ``advertise conn |
|       | connectabl | =und [own_addr_t |
|       | e          | ype=x] [discov=x |
|       | mode       | ] [duration=x]`` |
+-------+------------+------------------+
|       | Auto       | ``white-list add |
|       | connection | r_type=x addr=x  |
|       | establishm | [addr_type=y add |
|       | ent        | r=y] [...]``     |
|       | procedure  | ``connect addr_t |
|       |            | ype=wl``         |
+-------+------------+------------------+
|       | General    | ``scan duration= |
|       | connection | x``              |
|       | establishm | ``scan cancel``  |
|       | ent        | ``connect peer_a |
|       | procedure  | ddr_type=x peer_ |
|       |            | addr=x``         |
+-------+------------+------------------+
|       | Selective  | ``white-list add |
|       | connection | r_type=x addr=x  |
|       | establishm | [addr_type=y add |
|       | ent        | r=y] [...]``     |
|       | procedure  | ``scan filter=us |
|       |            | e_wl duration=x` |
|       |            | `                |
|       |            | ``scan cancel``  |
|       |            | ``connect peer_a |
|       |            | ddr_type=x peer_ |
|       |            | addr=x [own_addr |
|       |            | _type=x]``       |
+-------+------------+------------------+
|       | Direct     | ``connect addr_t |
|       | connection | ype=x addr=x [pa |
|       | establishm | rams]``          |
|       | ent        |                  |
|       | procedure  |                  |
+-------+------------+------------------+
|       | Connection | ``conn-update-pa |
|       | parameter  | rams conn=x <par |
|       | update     | ams>``           |
|       | procedure  |                  |
+-------+------------+------------------+
|       | Terminate  | ``disconnect con |
|       | connection | n=x``            |
|       | procedure  |                  |
+-------+------------+------------------+
| 4     | Non-Bondab | ``security-set-d |
|       | le         | ata bonding=0``  |
|       | mode       | [\*]             |
+-------+------------+------------------+
|       | Bondable   | ``security-set-d |
|       | mode       | ata bonding=1``  |
|       |            | [\*]             |
+-------+------------+------------------+
|       | Bonding    | ``security-start |
|       | procedure  |  conn=x``        |
|       |            | [\*]             |
+-------+------------+------------------+

**[\*]** Security is disabled by default in btshell. To use the bonding
modes and procedures, add ``BLE_SM_LEGACY: 1`` or ``BLE_SM_SC: 1`` to
your syscfg.yml file depending on your needs.

Address Types
~~~~~~~~~~~~~

+--------------+------------------------------------+---------------------------+
| *btshell     | *Description*                      | *Notes*                   |
| string*      |                                    |                           |
+==============+====================================+===========================+
| public       | Public address.                    |                           |
+--------------+------------------------------------+---------------------------+
| random       | Random static address.             |                           |
+--------------+------------------------------------+---------------------------+
| rpa\_pub     | Resolvable private address, public | Not available for all     |
|              | identity.                          | commands.                 |
+--------------+------------------------------------+---------------------------+
| rpa\_rnd     | Resolvable private address, random | Not available for all     |
|              | static identity.                   | commands.                 |
+--------------+------------------------------------+---------------------------+
| wl           | Use white list; ignore peer\_addr  | Only availble for         |
|              | parameter.                         | "connect" command.        |
+--------------+------------------------------------+---------------------------+

Connection Types
~~~~~~~~~~~~~~~~

Bluetooth Specification Version 5.0 allows for different types of
connections:

+--------------------------------------------------+---------------------------------+
| *Description*                                    | *btshell ext parameter value*   |
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
| *Name*      | *Description*                                     | *btshell   |
|             |                                                   | string*    |
+=============+===================================================+============+
| Connection  | Parameter indicating the type of connection       | extended   |
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
| Duration    | Number of milliseconds before aborting the        | duration   |
|             | connect attempt                                   |            |
+-------------+---------------------------------------------------+------------+

Connection parameters for legacy and 1M PHY extended connection:

+------------+-----------------------------------------------------+------------+
| *Name*     | *Description*                                       | *btshell   |
|            |                                                     | string*    |
+============+=====================================================+============+
| LE\_Scan\_ | Recommendation from the Host on how long the        | scan\_inte |
| Interval   | Controller should scan                              | rval       |
+------------+-----------------------------------------------------+------------+
| LE\_Scan\_ | Recommendation from the Host on how frequently the  | scan\_wind |
| Window     | Controller should scan                              | ow         |
+------------+-----------------------------------------------------+------------+
| Conn\_Inte | Defines minimum allowed connection interval         | interval\_ |
| rval\_Min  |                                                     | min        |
+------------+-----------------------------------------------------+------------+
| Conn\_Inte | Defines maximum allowed connection interval         | interval\_ |
| rval\_Max  |                                                     | max        |
+------------+-----------------------------------------------------+------------+
| Conn\_Late | Defines the maximum allowed connection latency      | latency    |
| ncy        |                                                     |            |
+------------+-----------------------------------------------------+------------+
| Supervisio | Link supervision timeout for the connection.        | timeout    |
| n\_Timeout |                                                     |            |
+------------+-----------------------------------------------------+------------+
| Minimum\_C | Informative parameter providing the Controller with | min\_conn\ |
| E\_Length  | the expected minimum length of the connection event | _event\_le |
|            |                                                     | n          |
+------------+-----------------------------------------------------+------------+
| Maximum\_C | Informative parameter providing the Controller with | max\_conn\ |
| E\_Length  | the expected maximum length of the connection event | _event\_le |
|            |                                                     | n          |
+------------+-----------------------------------------------------+------------+

Extended Connection parameters for coded PHY connection:

+------------+---------------------------------------------------+--------------+
| *Name*     | *Description*                                     | *btshell     |
|            |                                                   | string*      |
+============+===================================================+==============+
| LE\_Scan\_ | Recommendation from the Host on how long the      | coded\_scan\ |
| Interval   | Controller should scan                            | _interval    |
+------------+---------------------------------------------------+--------------+
| LE\_Scan\_ | Recommendation from the Host on how frequently    | coded\_scan\ |
| Window     | the Controller should scan                        | _window      |
+------------+---------------------------------------------------+--------------+
| Conn\_Inte | Defines minimum allowed connection interval       | coded\_inter |
| rval\_Min  |                                                   | val\_min     |
+------------+---------------------------------------------------+--------------+
| Conn\_Inte | Defines maximum allowed connection interval       | coded\_inter |
| rval\_Max  |                                                   | val\_max     |
+------------+---------------------------------------------------+--------------+
| Conn\_Late | Defines the maximum allowed connection latency    | coded\_laten |
| ncy        |                                                   | cy           |
+------------+---------------------------------------------------+--------------+
| Supervisio | Link supervision timeout for the connection.      | coded\_timeo |
| n\_Timeout |                                                   | ut           |
+------------+---------------------------------------------------+--------------+
| Minimum\_C | Informative parameter providing the Controller    | coded\_min\_ |
| E\_Length  | with the expected minimum length of the           | conn\_event\ |
|            | connection event                                  | _len         |
+------------+---------------------------------------------------+--------------+
| Maximum\_C | Informative parameter providing the Controller    | coded\_max\_ |
| E\_Length  | with the expected maximum length of the           | conn\_event\ |
|            | connection event                                  | _len         |
+------------+---------------------------------------------------+--------------+

Extended Connection parameters for 2M PHY connection:

+------------+----------------------------------------------------+-------------+
| *Name*     | *Description*                                      | *btshell    |
|            |                                                    | string*     |
+============+====================================================+=============+
| Conn\_Inte | Defines minimum allowed connection interval        | 2M\_interva |
| rval\_Min  |                                                    | l\_min      |
+------------+----------------------------------------------------+-------------+
| Conn\_Inte | Defines maximum allowed connection interval        | 2M\_interva |
| rval\_Max  |                                                    | l\_max      |
+------------+----------------------------------------------------+-------------+
| Conn\_Late | Defines the maximum allowed connection latency     | 2M\_latency |
| ncy        |                                                    |             |
+------------+----------------------------------------------------+-------------+
| Supervisio | Link supervision timeout for the connection.       | 2M\_timeout |
| n\_Timeout |                                                    |             |
+------------+----------------------------------------------------+-------------+
| Minimum\_C | Informative parameter providing the Controller     | 2M\_min\_co |
| E\_Length  | with the expected minimum length of the connection | nn\_event\_ |
|            | event                                              | len         |
+------------+----------------------------------------------------+-------------+
| Maximum\_C | Informative parameter providing the Controller     | 2M\_max\_co |
| E\_Length  | with the expected maximum length of the connection | nn\_event\_ |
|            | event                                              | len         |
+------------+----------------------------------------------------+-------------+

Scan Types
~~~~~~~~~~

Bluetooth Specification Version 5.0 allows for different types of scan:

+--------------------------------------------+---------------------------------+
| *Description*                              | *btshell ext parameter value*   |
+============================================+=================================+
| Legacy scan                                | none                            |
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
| *Name*          | *Description*                                 | *btshell    |
|                 |                                               | string*     |
+=================+===============================================+=============+
| Scan Type       | Parameter indicating the type of scan         | extended    |
+-----------------+-----------------------------------------------+-------------+
| LE\_Scan\_Type  | Controls the type of scan to perform (passive | passive     |
|                 | or active)                                    |             |
+-----------------+-----------------------------------------------+-------------+
| LE\_Scan\_Inter | Recommendation from the Host on how long the  | interval    |
| val             | Controller should scan                        |             |
+-----------------+-----------------------------------------------+-------------+
| LE\_Scan\_Windo | Recommendation from the Host on how           | window      |
| w               | frequently the Controller should scan         |             |
+-----------------+-----------------------------------------------+-------------+
| Scanning\_Filte | Policy about which advertising packets to     | filter      |
| r\_Policy       | accept                                        |             |
+-----------------+-----------------------------------------------+-------------+
| Duration        | Number of milliseconds before canceling scan  | duration    |
|                 | procedure                                     |             |
+-----------------+-----------------------------------------------+-------------+
| Limited         | Limited scan procedure                        | limited     |
+-----------------+-----------------------------------------------+-------------+
| No duplicates   | Filter out duplicates in shell output         | nodups      |
+-----------------+-----------------------------------------------+-------------+
| Own\_Address\_T | The type of address to use when scanning (see | own\_addr\_ |
| ype             | Address types table)                          | type        |
+-----------------+-----------------------------------------------+-------------+

Extended Scan parameters:

+-------------+-------------------------------------------------+---------------+
| *Name*      | *Description*                                   | *btshell      |
|             |                                                 | string*       |
+=============+=================================================+===============+
| Duration    | Number of milliseconds before canceling scan    | extended\_dur |
|             | procedure                                       | ation         |
+-------------+-------------------------------------------------+---------------+
| Period      | Period in which scan should be enabled for      | extended\_per |
|             | specified duration                              | iod           |
+-------------+-------------------------------------------------+---------------+
| LE\_Scan\_T | Controls the type of scan to perform (passive   | longrange\_pa |
| ype         | or active)                                      | ssive         |
+-------------+-------------------------------------------------+---------------+
| LE\_Scan\_I | Recommendation from the Host on how long the    | longrange\_in |
| nterval     | Controller should scan                          | terval        |
+-------------+-------------------------------------------------+---------------+
| LE\_Scan\_W | Recommendation from the Host on how frequently  | longrange\_wi |
| indow       | the Controller should scan                      | ndow          |
+-------------+-------------------------------------------------+---------------+

Advertisment Parameters
~~~~~~~~~~~~~~~~~~~~~~~

+-----------+---------------------+--------------------------------+---------------+
| *btshell  | *Description*       | *Notes*                        | *Default*     |
| string*   |                     |                                |               |
+===========+=====================+================================+===============+
| conn      | Connectable mode    | See Connectable Modes table.   | und           |
+-----------+---------------------+--------------------------------+---------------+
| discov    | Discoverable mode   | See Discoverable Modes table.  | gen           |
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
| channel\_ |                     |                                | 0             |
| map       |                     |                                |               |
+-----------+---------------------+--------------------------------+---------------+
| filter    | The filter policy   | See Advertisement Filter       | none          |
|           |                     | Policies table.                |               |
+-----------+---------------------+--------------------------------+---------------+
| interval\ |                     | units=0.625ms                  | non: 100ms;   |
| _min      |                     |                                | und/dir: 30ms |
+-----------+---------------------+--------------------------------+---------------+
| interval\ |                     | units=0.625ms                  | non: 150ms;   |
| _max      |                     |                                | und/dir: 60ms |
+-----------+---------------------+--------------------------------+---------------+
| high\_dut | Whether to use      | 0/1                            | 0             |
| y         | high-duty-cycle     |                                |               |
+-----------+---------------------+--------------------------------+---------------+
| duration  |                     | Milliseconds                   | Forever       |
+-----------+---------------------+--------------------------------+---------------+

Extended Advertising parameters:

+----------+-------------------------------------------+---------+----------------+
| *btshell | *Description*                             | *Notes* | *Default*      |
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
| *Description*                         | *btshell parameter value*   |
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
