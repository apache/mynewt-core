Advertisement Data Fields
-------------------------

This part defines the advertisement data fields used in the ``btshell``
app. For a complete list of all data types and formats used for Extended
Inquiry Response (EIR), Advertising Data (AD), and OOB data blocks,
refer to the Supplement to the Bluetooth Core Specification, CSSv6,
available for download
`here <https://www.bluetooth.org/DocMan/handlers/DownloadDoc.ashx?doc_id=302735&_ga=1.133090766.1368218946.1444779486>`__.

+-------+------------+------------------+-----+
| **Nam | **Definiti | **Details**      | **b |
| e**   | on**       |                  | tsh |
|       |            |                  | ell |
|       |            |                  | Not |
|       |            |                  | es* |
|       |            |                  | *   |
+=======+============+==================+=====+
| flags | Indicates  | Flags used over  | Nim |
|       | basic      | the LE physical  | BLE |
|       | informatio | channel are: \*  | wil |
|       | n          | Limited          | l   |
|       | about the  | Discoverable     | aut |
|       | advertiser | Mode \* General  | o-c |
|       | .          | Discoverable     | alc |
|       |            | Mode \* BR/EDR   | ula |
|       |            | Not Supported \* | te  |
|       |            | Simultaneous LE  | if  |
|       |            | and BR/EDR to    | set |
|       |            | Same Device      | to  |
|       |            | Capable          | 0.  |
|       |            | (Controller) \*  |     |
|       |            | Simultaneous LE  |     |
|       |            | and BR/EDR to    |     |
|       |            | Same Device      |     |
|       |            | Capable (Host)   |     |
+-------+------------+------------------+-----+
| uuid1 | 16-bit     | Indicates the    | Set |
| 6     | Bluetooth  | Service UUID     | rep |
|       | Service    | list is          | eat |
|       | UUIDs      | incomplete i.e.  | edl |
|       |            | more 16-bit      | y   |
|       |            | Service UUIDs    | for |
|       |            | available. 16    | mul |
|       |            | bit UUIDs shall  | tip |
|       |            | only be used if  | le  |
|       |            | they are         | ser |
|       |            | assigned by the  | vic |
|       |            | Bluetooth SIG.   | e   |
|       |            |                  | UUI |
|       |            |                  | Ds. |
+-------+------------+------------------+-----+
| uuid1 | 16-bit     | Indicates the    |     |
| 6\_is | Bluetooth  | Service UUID     |     |
| \_com | Service    | list is          |     |
| plete | UUIDs      | complete. 16 bit |     |
|       |            | UUIDs shall only |     |
|       |            | be used if they  |     |
|       |            | are assigned by  |     |
|       |            | the Bluetooth    |     |
|       |            | SIG.             |     |
+-------+------------+------------------+-----+
| uuid3 | 32-bit     | Indicates the    | Set |
| 2     | Bluetooth  | Service UUID     | rep |
|       | Service    | list is          | eat |
|       | UUIDs      | incomplete i.e.  | edl |
|       |            | more 32-bit      | y   |
|       |            | Service UUIDs    | for |
|       |            | available. 32    | mul |
|       |            | bit UUIDs shall  | tip |
|       |            | only be used if  | le  |
|       |            | they are         | ser |
|       |            | assigned by the  | vic |
|       |            | Bluetooth SIG.   | e   |
|       |            |                  | UUI |
|       |            |                  | Ds. |
+-------+------------+------------------+-----+
| uuid3 | 32-bit     | Indicates the    |     |
| 2\_is | Bluetooth  | Service UUID     |     |
| \_com | Service    | list is          |     |
| plete | UUIDs      | complete. 32 bit |     |
|       |            | UUIDs shall only |     |
|       |            | be used if they  |     |
|       |            | are assigned by  |     |
|       |            | the Bluetooth    |     |
|       |            | SIG.             |     |
+-------+------------+------------------+-----+
| uuid1 | Global     | More 128-bit     | Set |
| 28    | 128-bit    | Service UUIDs    | rep |
|       | Service    | available.       | eat |
|       | UUIDs      |                  | edl |
|       |            |                  | y   |
|       |            |                  | for |
|       |            |                  | mul |
|       |            |                  | tip |
|       |            |                  | le  |
|       |            |                  | ser |
|       |            |                  | vic |
|       |            |                  | e   |
|       |            |                  | UUI |
|       |            |                  | Ds. |
+-------+------------+------------------+-----+
| uuid1 | Global     | Complete list of |     |
| 28\_i | 128-bit    | 128-bit Service  |     |
| s\_co | Service    | UUIDs            |     |
| mplet | UUIDs      |                  |     |
| e     |            |                  |     |
+-------+------------+------------------+-----+
| tx\_p | TX Power   | Indicates the    | Nim |
| ower\ | Level      | transmitted      | BLE |
| _leve |            | power level of   | wil |
| l     |            | the packet       | l   |
|       |            | containing the   | aut |
|       |            | data type. The   | o-c |
|       |            | TX Power Level   | alc |
|       |            | data type may be | ula |
|       |            | used to          | te  |
|       |            | calculate path   | if  |
|       |            | loss on a        | set |
|       |            | received packet  | to  |
|       |            | using the        | -12 |
|       |            | following        | 8.  |
|       |            | equation:        |     |
|       |            | pathloss = Tx    |     |
|       |            | Power Level –    |     |
|       |            | RSSI where       |     |
|       |            | “RSSI” is the    |     |
|       |            | received signal  |     |
|       |            | strength, in     |     |
|       |            | dBm, of the      |     |
|       |            | packet received. |     |
+-------+------------+------------------+-----+
| slave | Slave      | Contains the     |     |
| \_int | Connection | Peripheral’s     |     |
| erval | Interval   | preferred        |     |
| \_ran | Range      | connection       |     |
| ge    |            | interval range,  |     |
|       |            | for all logical  |     |
|       |            | connections.     |     |
|       |            | Size: 4 Octets . |     |
|       |            | The first 2      |     |
|       |            | octets defines   |     |
|       |            | the minimum      |     |
|       |            | value for the    |     |
|       |            | connection       |     |
|       |            | interval in the  |     |
|       |            | following        |     |
|       |            | manner:          |     |
|       |            | connIntervalmin  |     |
|       |            | =                |     |
|       |            | Conn\_Interval\_ |     |
|       |            | Min              |     |
|       |            | \* 1.25 ms       |     |
|       |            | Conn\_Interval\_ |     |
|       |            | Min              |     |
|       |            | range: 0x0006 to |     |
|       |            | 0x0C80 Value of  |     |
|       |            | 0xFFFF indicates |     |
|       |            | no specific      |     |
|       |            | minimum. The     |     |
|       |            | other 2 octets   |     |
|       |            | defines the      |     |
|       |            | maximum value    |     |
|       |            | for the          |     |
|       |            | connection       |     |
|       |            | interval in the  |     |
|       |            | following        |     |
|       |            | manner:          |     |
|       |            | connIntervalmax  |     |
|       |            | =                |     |
|       |            | Conn\_Interval\_ |     |
|       |            | Max              |     |
|       |            | \* 1.25 ms       |     |
|       |            | Conn\_Interval\_ |     |
|       |            | Max              |     |
|       |            | range: 0x0006 to |     |
|       |            | 0x0C80           |     |
|       |            | Conn\_Interval\_ |     |
|       |            | Max              |     |
|       |            | shall be equal   |     |
|       |            | to or greater    |     |
|       |            | than the         |     |
|       |            | Conn\_Interval\_ |     |
|       |            | Min.             |     |
|       |            | Value of 0xFFFF  |     |
|       |            | indicates no     |     |
|       |            | specific         |     |
|       |            | maximum.         |     |
+-------+------------+------------------+-----+
| servi | Service    | Size: 2 or more  |     |
| ce\_d | Data - 16  | octets The first |     |
| ata\_ | bit UUID   | 2 octets contain |     |
| uuid1 |            | the 16 bit       |     |
| 6     |            | Service UUID     |     |
|       |            | followed by      |     |
|       |            | additional       |     |
|       |            | service data     |     |
+-------+------------+------------------+-----+
| publi | Public     | Defines the      |     |
| c\_ta | Target     | address of one   |     |
| rget\ | Address    | or more intended |     |
| _addr |            | recipients of an |     |
| ess   |            | advertisement    |     |
|       |            | when one or more |     |
|       |            | devices were     |     |
|       |            | bonded using a   |     |
|       |            | public address.  |     |
|       |            | This data type   |     |
|       |            | shall exist only |     |
|       |            | once. It may be  |     |
|       |            | sent in either   |     |
|       |            | the Advertising  |     |
|       |            | or Scan Response |     |
|       |            | data, but not    |     |
|       |            | both.            |     |
+-------+------------+------------------+-----+
| appea | Appearance | Defines the      |     |
| rance |            | external         |     |
|       |            | appearance of    |     |
|       |            | the device. The  |     |
|       |            | Appearance data  |     |
|       |            | type shall exist |     |
|       |            | only once. It    |     |
|       |            | may be sent in   |     |
|       |            | either the       |     |
|       |            | Advertising or   |     |
|       |            | Scan Response    |     |
|       |            | data, but not    |     |
|       |            | both.            |     |
+-------+------------+------------------+-----+
| adver | Advertisin | Contains the     |     |
| tisin | g          | advInterval      |     |
| g\_in | Interval   | value as defined |     |
| terva |            | in the Core      |     |
| l     |            | specification,   |     |
|       |            | Volume 6, Part   |     |
|       |            | B, Section       |     |
|       |            | 4.4.2.2.         |     |
+-------+------------+------------------+-----+
| servi | Service    | Size: 4 or more  |     |
| ce\_d | Data - 32  | octets The first |     |
| ata\_ | bit UUID   | 4 octets contain |     |
| uuid3 |            | the 32 bit       |     |
| 2     |            | Service UUID     |     |
|       |            | followed by      |     |
|       |            | additional       |     |
|       |            | service data     |     |
+-------+------------+------------------+-----+
| servi | Service    | Size: 16 or more |     |
| ce\_d | Data - 128 | octets The first |     |
| ata\_ | bit UUID   | 16 octets        |     |
| uuid1 |            | contain the 128  |     |
| 28    |            | bit Service UUID |     |
|       |            | followed by      |     |
|       |            | additional       |     |
|       |            | service data     |     |
+-------+------------+------------------+-----+
| uri   | Uniform    | Scheme name      |     |
|       | Resource   | string and URI   |     |
|       | Identifier | as a UTF-8       |     |
|       | (URI)      | string           |     |
+-------+------------+------------------+-----+
| mfg\_ | Manufactur | Size: 2 or more  |     |
| data  | er         | octets The first |     |
|       | Specific   | 2 octets contain |     |
|       | data       | the Company      |     |
|       |            | Identifier Code  |     |
|       |            | followed by      |     |
|       |            | additional       |     |
|       |            | manufacturer     |     |
|       |            | specific data    |     |
+-------+------------+------------------+-----+
| eddys |            |                  |     |
| tone\ |            |                  |     |
| _url  |            |                  |     |
+-------+------------+------------------+-----+


