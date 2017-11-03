GATT feature API for btshell
----------------------------

GATT(GENERIC ATTRIBUTE PROFILE) describes a service framework using the
Attribute Protocol for discovering services, and for reading and writing
characteristic values on a peer device. There are 11 features defined in
the GATT Profile, and each of the features is mapped to procedures and
sub-procedures:

+--------+-------------------+--------------------+-------------------------------+
| **Item | **Feature**       | **Sub-Procedure**  | **nimBLE command**            |
| No.**  |                   |                    |                               |
+========+===================+====================+===============================+
| 1      | Server            | Exchange MTU       | ``gatt-exchange-mtu conn=x``  |
|        | Configuration     |                    |                               |
+--------+-------------------+--------------------+-------------------------------+
| 2      | Primary Service   | Discover All       | ``gatt-discover-service conn= |
|        | Discovery         | Primary Services   | x``                           |
+--------+-------------------+--------------------+-------------------------------+
|        |                   | Discover Primary   | ``gatt-discover-service conn= |
|        |                   | Services By        | x uuid=x``                    |
|        |                   | Service UUID       |                               |
+--------+-------------------+--------------------+-------------------------------+
| 3      | Relationship      | Find Included      | ``gatt-find-included-services |
|        | Discovery         | Services           |  conn=x start=x end=x``       |
+--------+-------------------+--------------------+-------------------------------+
| 4      | Characteristic    | Discover All       | ``gatt-discover-characteristi |
|        | Discovery         | Characteristic of  | c conn=x start=x end=x``      |
|        |                   | a Service          |                               |
+--------+-------------------+--------------------+-------------------------------+
|        |                   | Discover           | ``gatt-discover-characteristi |
|        |                   | Characteristic by  | c conn=x start=x end=x uuid=x |
|        |                   | UUID               | ``                            |
+--------+-------------------+--------------------+-------------------------------+
| 5      | Characteristic    | Discover All       | ``gatt-discover-descriptor co |
|        | Descriptor        | Characteristic     | nn=x start=x end=x``          |
|        | Discovery         | Descriptors        |                               |
+--------+-------------------+--------------------+-------------------------------+
| 6      | Reading a         | Read               | ``gatt-read conn=x attr=x``   |
|        | Characteristic    | Characteristic     |                               |
|        | Value             | Value              |                               |
+--------+-------------------+--------------------+-------------------------------+
|        |                   | Read Using         | ``gatt-read conn=x start=x en |
|        |                   | Characteristic     | d=x uuid=x``                  |
|        |                   | UUID               |                               |
+--------+-------------------+--------------------+-------------------------------+
|        |                   | Read Long          | ``gatt-read conn=x attr=x lon |
|        |                   | Characteristic     | g=1``                         |
|        |                   | Values             |                               |
+--------+-------------------+--------------------+-------------------------------+
|        |                   | Read Multiple      | ``gatt-read conn=x attr=x att |
|        |                   | Characteristic     | r=y attr=z``                  |
|        |                   | Values             |                               |
+--------+-------------------+--------------------+-------------------------------+
| 7      | Writing a         | Write Without      | ``gatt-write conn=x value=0xX |
|        | Characteristic    | Response           | X:0xXX no_rsp=1``             |
|        | Value             |                    |                               |
+--------+-------------------+--------------------+-------------------------------+
|        |                   | Signed Write       | NOT SUPPORTED                 |
|        |                   | Without Response   |                               |
+--------+-------------------+--------------------+-------------------------------+
|        |                   | Write              | ``gatt-write conn=x attr=x va |
|        |                   | Characteristic     | lue=0xXX:0xXX``               |
|        |                   | Value              |                               |
+--------+-------------------+--------------------+-------------------------------+
|        |                   | Write Long         | ``gatt-write conn=x attr=x va |
|        |                   | Characteristic     | lue=0xXX:0xXX long=1``        |
|        |                   | Values             |                               |
+--------+-------------------+--------------------+-------------------------------+
|        |                   | Characteristic     | ``gatt-write conn=x attr=x va |
|        |                   | Value Reliable     | lue=0xXX:0xXX attr=y value=0x |
|        |                   | Writes             | YY:0xYY``                     |
+--------+-------------------+--------------------+-------------------------------+
| 8      | Notification of a | Notifications      | Write *0x01:0x00* to CLIENT   |
|        | Characteristic    |                    | CONFIGURATION characteristic  |
|        | Value             |                    |                               |
+--------+-------------------+--------------------+-------------------------------+
| 9      | Indication of a   | Indications        | Write *0x02:0x00* to CLIENT   |
|        | Characteristic    |                    | CONFIGURATION characteristic  |
|        | Value             |                    |                               |
+--------+-------------------+--------------------+-------------------------------+
| 10     | Reading a         | Read               | ``gatt-read conn=x attr=x``   |
|        | Characteristic    | Characteristic     |                               |
|        | Descriptor        | Descriptors        |                               |
+--------+-------------------+--------------------+-------------------------------+
|        |                   | Read Long          | ``gatt-read conn=x attr=x lon |
|        |                   | Characteristic     | g=1``                         |
|        |                   | Descriptors        |                               |
+--------+-------------------+--------------------+-------------------------------+
| 11     | Writing a         | Write              | ``gatt-write conn=x value=0xX |
|        | Characteristic    | Characteristic     | X:0xXX``                      |
|        | Descriptor        | Descriptors        |                               |
+--------+-------------------+--------------------+-------------------------------+
|        |                   | Write Long         | ``gatt-write conn=x value=0xX |
|        |                   | Characteristic     | X:0xXX long=1``               |
|        |                   | Descriptors        |                               |
+--------+-------------------+--------------------+-------------------------------+

Using NimBLE commands
~~~~~~~~~~~~~~~~~~~~~

Assuming you have discovered and established a BLE connection with at
least one peer device (as explained earlier in `API for btshell
app <btshell_api.html>`__, you can find out what characteristics and
services are available over these connections. Here is a recap.

To show established connections:

::

    gatt-show-conn

To show discovered services, characteristics, and descriptors:

::

    gatt-show

To show local database of services, characteristics, and descriptors:

::

    gatt-show-local

To show connection RSSI:

::

    conn-rssi conn=x
