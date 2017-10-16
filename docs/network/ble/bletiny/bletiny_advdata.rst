Advertisement Data Fields
-------------------------

This part defines the advertisement data fields used in the ``bletiny``
app. For a complete list of all data types and formats used for Extended
Inquiry Response (EIR), Advertising Data (AD), and OOB data blocks,
refer to the Supplement to the Bluetooth Core Specification, CSSv6,
available for download
`here <https://www.bluetooth.org/DocMan/handlers/DownloadDoc.ashx?doc_id=302735&_ga=1.133090766.1368218946.1444779486>`__.

+-------+------------+------------------+
| **Nam | **Definiti | **Details**      |
| e**   | on**       |                  |
+=======+============+==================+
| uuids | 16-bit     | Indicates the    |
| 16    | Bluetooth  | Service UUID     |
|       | Service    | list is          |
|       | UUIDs      | incomplete i.e.  |
|       |            | more 16-bit      |
|       |            | Service UUIDs    |
|       |            | available. 16    |
|       |            | bit UUIDs shall  |
|       |            | only be used if  |
|       |            | they are         |
|       |            | assigned by the  |
|       |            | Bluetooth SIG.   |
+-------+------------+------------------+
| uuids | 16-bit     | Indicates the    |
| 16\_i | Bluetooth  | Service UUID     |
| s\_co | Service    | list is          |
| mplet | UUIDs      | complete. 16 bit |
| e     |            | UUIDs shall only |
|       |            | be used if they  |
|       |            | are assigned by  |
|       |            | the Bluetooth    |
|       |            | SIG.             |
+-------+------------+------------------+
| uuids | 32-bit     | Indicates the    |
| 32    | Bluetooth  | Service UUID     |
|       | Service    | list is          |
|       | UUIDs      | incomplete i.e.  |
|       |            | more 32-bit      |
|       |            | Service UUIDs    |
|       |            | available. 32    |
|       |            | bit UUIDs shall  |
|       |            | only be used if  |
|       |            | they are         |
|       |            | assigned by the  |
|       |            | Bluetooth SIG.   |
+-------+------------+------------------+
| uuids | 32-bit     | Indicates the    |
| 32\_i | Bluetooth  | Service UUID     |
| s\_co | Service    | list is          |
| mplet | UUIDs      | complete. 32 bit |
| e     |            | UUIDs shall only |
|       |            | be used if they  |
|       |            | are assigned by  |
|       |            | the Bluetooth    |
|       |            | SIG.             |
+-------+------------+------------------+
| uuids | Global     | More 128-bit     |
| 128   | 128-bit    | Service UUIDs    |
|       | Service    | available.       |
|       | UUIDs      |                  |
+-------+------------+------------------+
| uuids | Global     | Complete list of |
| 128\_ | 128-bit    | 128-bit Service  |
| is\_c | Service    | UUIDs            |
| omple | UUIDs      |                  |
| te    |            |                  |
+-------+------------+------------------+
| tx\_p | TX Power   | Indicates the    |
| wr\_l | Level      | transmitted      |
| vl    |            | power level of   |
|       |            | the packet       |
|       |            | containing the   |
|       |            | data type. The   |
|       |            | TX Power Level   |
|       |            | data type may be |
|       |            | used to          |
|       |            | calculate path   |
|       |            | loss on a        |
|       |            | received packet  |
|       |            | using the        |
|       |            | following        |
|       |            | equation:        |
|       |            | pathloss = Tx    |
|       |            | Power Level –    |
|       |            | RSSI where       |
|       |            | “RSSI” is the    |
|       |            | received signal  |
|       |            | strength, in     |
|       |            | dBm, of the      |
|       |            | packet received. |
+-------+------------+------------------+
| devic | Class of   | Size: 3 octets   |
| e\_cl | device     |                  |
| ass   |            |                  |
+-------+------------+------------------+
| slave | Slave      | Contains the     |
| \_itv | Connection | Peripheral’s     |
| l\_ra | Interval   | preferred        |
| nge   | Range      | connection       |
|       |            | interval range,  |
|       |            | for all logical  |
|       |            | connections.     |
|       |            | Size: 4 Octets . |
|       |            | The first 2      |
|       |            | octets defines   |
|       |            | the minimum      |
|       |            | value for the    |
|       |            | connection       |
|       |            | interval in the  |
|       |            | following        |
|       |            | manner:          |
|       |            | connIntervalmin  |
|       |            | =                |
|       |            | Conn\_Interval\_ |
|       |            | Min              |
|       |            | \* 1.25 ms       |
|       |            | Conn\_Interval\_ |
|       |            | Min              |
|       |            | range: 0x0006 to |
|       |            | 0x0C80 Value of  |
|       |            | 0xFFFF indicates |
|       |            | no specific      |
|       |            | minimum. The     |
|       |            | other 2 octets   |
|       |            | defines the      |
|       |            | maximum value    |
|       |            | for the          |
|       |            | connection       |
|       |            | interval in the  |
|       |            | following        |
|       |            | manner:          |
|       |            | connIntervalmax  |
|       |            | =                |
|       |            | Conn\_Interval\_ |
|       |            | Max              |
|       |            | \* 1.25 ms       |
|       |            | Conn\_Interval\_ |
|       |            | Max              |
|       |            | range: 0x0006 to |
|       |            | 0x0C80           |
|       |            | Conn\_Interval\_ |
|       |            | Max              |
|       |            | shall be equal   |
|       |            | to or greater    |
|       |            | than the         |
|       |            | Conn\_Interval\_ |
|       |            | Min.             |
|       |            | Value of 0xFFFF  |
|       |            | indicates no     |
|       |            | specific         |
|       |            | maximum.         |
+-------+------------+------------------+
| svc\_ | Service    | Size: 2 or more  |
| data\ | Data - 16  | octets The first |
| _uuid | bit UUID   | 2 octets contain |
| 16    |            | the 16 bit       |
|       |            | Service UUID     |
|       |            | followed by      |
|       |            | additional       |
|       |            | service data     |
+-------+------------+------------------+
| publi | Public     | Defines the      |
| c\_tg | Target     | address of one   |
| t\_ad | Address    | or more intended |
| dr    |            | recipients of an |
|       |            | advertisement    |
|       |            | when one or more |
|       |            | devices were     |
|       |            | bonded using a   |
|       |            | public address.  |
|       |            | This data type   |
|       |            | shall exist only |
|       |            | once. It may be  |
|       |            | sent in either   |
|       |            | the Advertising  |
|       |            | or Scan Response |
|       |            | data, but not    |
|       |            | both.            |
+-------+------------+------------------+
| appea | Appearance | Defines the      |
| rance |            | external         |
|       |            | appearance of    |
|       |            | the device. The  |
|       |            | Appearance data  |
|       |            | type shall exist |
|       |            | only once. It    |
|       |            | may be sent in   |
|       |            | either the       |
|       |            | Advertising or   |
|       |            | Scan Response    |
|       |            | data, but not    |
|       |            | both.            |
+-------+------------+------------------+
| adv\_ | Advertisin | Contains the     |
| itvl  | g          | advInterval      |
|       | Interval   | value as defined |
|       |            | in the Core      |
|       |            | specification,   |
|       |            | Volume 6, Part   |
|       |            | B, Section       |
|       |            | 4.4.2.2.         |
+-------+------------+------------------+
| le\_a | LE         | Defines the      |
| ddr   | Bluetooth  | device address   |
|       | Device     | of the local     |
|       | Address    | device and the   |
|       |            | address type on  |
|       |            | the LE           |
|       |            | transport.       |
+-------+------------+------------------+
| le\_r | LE Role    | Defines the LE   |
| ole   |            | role             |
|       |            | capabilities of  |
|       |            | the device. 0x00 |
|       |            | Only Peripheral  |
|       |            | Role supported   |
|       |            | 0x01 Only        |
|       |            | Central Role     |
|       |            | supported 0x02   |
|       |            | Peripheral and   |
|       |            | Central Role     |
|       |            | supported,       |
|       |            | Peripheral Role  |
|       |            | preferred for    |
|       |            | connection       |
|       |            | establishment    |
|       |            | 0x03 Peripheral  |
|       |            | and Central Role |
|       |            | supported,       |
|       |            | Central Role     |
|       |            | preferred for    |
|       |            | connection       |
|       |            | establishment    |
|       |            | 0x04 – 0xFF      |
|       |            | Reserved for     |
|       |            | future use       |
+-------+------------+------------------+
| svc\_ | Service    | Size: 4 or more  |
| data\ | Data - 32  | octets The first |
| _uuid | bit UUID   | 4 octets contain |
| 32    |            | the 32 bit       |
|       |            | Service UUID     |
|       |            | followed by      |
|       |            | additional       |
|       |            | service data     |
+-------+------------+------------------+
| svc\_ | Service    | Size: 16 or more |
| data\ | Data - 128 | octets The first |
| _uuid | bit UUID   | 16 octets        |
| 128   |            | contain the 128  |
|       |            | bit Service UUID |
|       |            | followed by      |
|       |            | additional       |
|       |            | service data     |
+-------+------------+------------------+
| uri   | Uniform    | Scheme name      |
|       | Resource   | string and URI   |
|       | Identifier | as a UTF-8       |
|       | (URI)      | string           |
+-------+------------+------------------+
| mfg\_ | Manufactur | Size: 2 or more  |
| data  | er         | octets The first |
|       | Specific   | 2 octets contain |
|       | data       | the Company      |
|       |            | Identifier Code  |
|       |            | followed by      |
|       |            | additional       |
|       |            | manufacturer     |
|       |            | specific data    |
+-------+------------+------------------+


