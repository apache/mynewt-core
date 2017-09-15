GATT feature API for bletiny
----------------------------

GATT(GENERIC ATTRIBUTE PROFILE) describes a service framework using the
Attribute Protocol for discovering services, and for reading and writing
characteristic values on a peer device. There are 11 features defined in
the GATT Profile, and each of the features is mapped to procedures and
sub-procedures:

+-------+------------+------------------+---------+
| **Ite | **Feature* | **Sub-Procedure* | **nimBL |
| m     | *          | *                | E       |
| No.** |            |                  | command |
|       |            |                  | **      |
+=======+============+==================+=========+
| 1     | Server     | Exchange MTU     | ``b mtu |
|       | Configurat |                  | ``      |
|       | ion        |                  |         |
+-------+------------+------------------+---------+
| 2     | Primary    | Discover All     | ``b dis |
|       | Service    | Primary Services | c svc c |
|       | Discovery  |                  | onn=x`` |
+-------+------------+------------------+---------+
|       |            | Discover Primary | ``b dis |
|       |            | Services By      | c svc c |
|       |            | Service UUID     | onn=x u |
|       |            |                  | uid=x`` |
+-------+------------+------------------+---------+
| 3     | Relationsh | Find Included    | ``b fin |
|       | ip         | Services         | d inc_s |
|       | Discovery  |                  | vcs con |
|       |            |                  | n=x sta |
|       |            |                  | rt=x en |
|       |            |                  | d=x``   |
+-------+------------+------------------+---------+
| 4     | Characteri | Discover All     | ``b dis |
|       | stic       | Characteristic   | c chr c |
|       | Discovery  | of a Service     | onn=x s |
|       |            |                  | tart=x  |
|       |            |                  | end=x`` |
+-------+------------+------------------+---------+
|       |            | Discover         | ``b dis |
|       |            | Characteristic   | c chr c |
|       |            | by UUID          | onn=x s |
|       |            |                  | tart=x  |
|       |            |                  | end=x u |
|       |            |                  | uid=x`` |
+-------+------------+------------------+---------+
| 5     | Characteri | Discover All     | ``b dis |
|       | stic       | Characteristic   | c dsc c |
|       | Descriptor | Descriptors      | onn=x s |
|       | Discovery  |                  | tart=x  |
|       |            |                  | end=x`` |
+-------+------------+------------------+---------+
| 6     | Reading a  | Read             | ``b rea |
|       | Characteri | Characteristic   | d conn= |
|       | stic       | Value            | x attr= |
|       | Value      |                  | x``     |
+-------+------------+------------------+---------+
|       |            | Read Using       | ``b rea |
|       |            | Characteristic   | d conn= |
|       |            | UUID             | x start |
|       |            |                  | =x end= |
|       |            |                  | x uuid= |
|       |            |                  | x``     |
+-------+------------+------------------+---------+
|       |            | Read Long        | ``b rea |
|       |            | Characteristic   | d conn= |
|       |            | Values           | x attr= |
|       |            |                  | x long= |
|       |            |                  | 1``     |
+-------+------------+------------------+---------+
|       |            | Read Multiple    | ``b rea |
|       |            | Characteristic   | d conn= |
|       |            | Values           | x attr= |
|       |            |                  | x attr= |
|       |            |                  | y attr= |
|       |            |                  | z``     |
+-------+------------+------------------+---------+
| 7     | Writing a  | Write Without    | ``b wri |
|       | Characteri | Response         | te conn |
|       | stic       |                  | =x valu |
|       | Value      |                  | e=0xXX: |
|       |            |                  | 0xXX no |
|       |            |                  | _rsp=1` |
|       |            |                  | `       |
+-------+------------+------------------+---------+
|       |            | Signed Write     | NOT     |
|       |            | Without Response | SUPPORT |
|       |            |                  | ED      |
+-------+------------+------------------+---------+
|       |            | Write            | ``b wri |
|       |            | Characteristic   | te conn |
|       |            | Value            | =x attr |
|       |            |                  | =x valu |
|       |            |                  | e=0xXX: |
|       |            |                  | 0xXX``  |
+-------+------------+------------------+---------+
|       |            | Write Long       | ``b wri |
|       |            | Characteristic   | te conn |
|       |            | Values           | =x attr |
|       |            |                  | =x valu |
|       |            |                  | e=0xXX: |
|       |            |                  | 0xXX lo |
|       |            |                  | ng=1``  |
+-------+------------+------------------+---------+
|       |            | Characteristic   | ``b wri |
|       |            | Value Reliable   | te conn |
|       |            | Writes           | =x attr |
|       |            |                  | =x valu |
|       |            |                  | e=0xXX: |
|       |            |                  | 0xXX at |
|       |            |                  | tr=y va |
|       |            |                  | lue=0xY |
|       |            |                  | Y:0xYY` |
|       |            |                  | `       |
+-------+------------+------------------+---------+
| 8     | Notificati | Notifications    | Write   |
|       | on         |                  | CLIENT  |
|       | of a       |                  | CONFIGU |
|       | Characteri |                  | RATION  |
|       | stic       |                  | charact |
|       | Value      |                  | eristic |
+-------+------------+------------------+---------+
| 9     | Indication | Indications      | Write   |
|       | of a       |                  | CLIENT  |
|       | Characteri |                  | CONFIGU |
|       | stic       |                  | RATION  |
|       | Value      |                  | charact |
|       |            |                  | eristic |
+-------+------------+------------------+---------+
| 10    | Reading a  | Read             | ``b rea |
|       | Characteri | Characteristic   | d conn= |
|       | stic       | Descriptors      | x attr= |
|       | Descriptor |                  | x``     |
+-------+------------+------------------+---------+
|       |            | Read Long        | ``b rea |
|       |            | Characteristic   | d conn= |
|       |            | Descriptors      | x attr= |
|       |            |                  | x long= |
|       |            |                  | 1``     |
+-------+------------+------------------+---------+
| 11    | Writing a  | Write            | ``b wri |
|       | Characteri | Characteristic   | te conn |
|       | stic       | Descriptors      | =x valu |
|       | Descriptor |                  | e=0xXX: |
|       |            |                  | 0xXX``  |
+-------+------------+------------------+---------+
|       |            | Write Long       | ``b wri |
|       |            | Characteristic   | te conn |
|       |            | Descriptors      | =x valu |
|       |            |                  | e=0xXX: |
|       |            |                  | 0xXX lo |
|       |            |                  | ng=1``  |
+-------+------------+------------------+---------+

 ### Using nimBLE commands Assuming you have discovered and established
a BLE connection with at least one peer device (as explained earlier in
`API for bletiny app <bletiny_api.html>`__, you can find out what
characteristics and services are available over these connections. Here
is a recap.

::

    b show conn

To show discovered services

::

    b show svc

To show discovered characteristics

::

    b show chr

To show connection RSSI

::

    b show rssi conn=x
