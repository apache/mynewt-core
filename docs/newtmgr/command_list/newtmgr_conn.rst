newtmgr conn 
-------------

Manage newtmgr connection profiles.

Usage:
^^^^^^

.. code-block:: console

        newtmgr conn [command] [flags] 

Flags:
^^^^^^

Global Flags:
^^^^^^^^^^^^^

.. code-block:: console

      -c, --conn string       connection profile to use
      -l, --loglevel string   log level to use (default "info")
          --name string       name of target BLE device; overrides profile setting
      -t, --timeout float     timeout in seconds (partial seconds allowed) (default 10)
      -r, --tries int         total number of tries in case of timeout (default 1)

Description
^^^^^^^^^^^

The conn command provides subcommands to add, delete, and view
connection profiles. A connection profile specifies information on how
to connect and communicate with a remote device. Newtmgr commands use
the information from a connection profile to send newtmgr requests to
remote devices.

+---------------+---------------+
| Sub-command   | Explanation   |
+===============+===============+
+---------------+---------------+

add \| The newtmgr conn add <conn\_profile> <var-name=value ...> command
creates a connection profile named ``conn_profile``. The command
requires the ``conn_profile`` name and a list of, space separated,
var-name=value pairs. The var-names are: ``type``, and ``connstring``.
The valid values for each var-name parameter are:

.. raw:: html

   <ul>

.. raw:: html

   <li>

``type``: The connection type. Valid values are:

.. raw:: html

   <ul>

.. raw:: html

   <li>

**serial**: Newtmgr protocol over a serial connection.

.. raw:: html

   </li>

.. raw:: html

   <li>

**oic\_serial**: OIC protocol over a serial connection.

.. raw:: html

   </li>

.. raw:: html

   <li>

**udp**:newtmgr protocol over UDP.

.. raw:: html

   </li>

.. raw:: html

   <li>

**oic\_udp**: OIC protocol over UDP.

.. raw:: html

   </li>

.. raw:: html

   <li>

**ble** newtmgr protocol over BLE. This type uses native OS BLE support

.. raw:: html

   </li>

.. raw:: html

   <li>

**oic\_ble**: OIC protocol over BLE. This type uses native OS BLE
support.

.. raw:: html

   </li>

.. raw:: html

   <li>

**bhd**: newtmgr protocol over BLE. This type uses the blehostd
implementation.

.. raw:: html

   </li>

.. raw:: html

   <li>

**oic\_bhd**: OIC protocol over BLE. This type uses the blehostd
implementation.

.. raw:: html

   </li>

.. raw:: html

   </li>

.. raw:: html

   </ul>

\ **Note:** newtmgr does not support BLE on Windows.

.. raw:: html

   <li>

``connstring``: The physical or virtual address for the connection. The
format of the ``connstring`` value depends on the connection ``type``
value as follows:

.. raw:: html

   <ul>

.. raw:: html

   <li>

**serial** and **oic\_serial**: A quoted string with two, comma
separated, ``attribute=value`` pairs. The attribute names and value
format for each attribute are:

.. raw:: html

   <ul>

.. raw:: html

   <li>

``dev``: (Required) The name of the serial port to use. For example:
**/dev/ttyUSB0** on a Linux platform or **COM1** on a Windows platform .

.. raw:: html

   </li>

.. raw:: html

   <li>

``baud``: (Optional) A number that specifies the buad rate for the
connection. Defaults to **115200** if the attribute is not specified.

.. raw:: html

   </li>

.. raw:: html

   </li>

.. raw:: html

   </ul>

Example: connstring="dev=/dev/ttyUSB0, baud=9600" **Note:** The 1.0
format, which only requires a serial port name, is still supported. For
example, ``connstring=/dev/ttyUSB0``.

.. raw:: html

   </li>

.. raw:: html

   <li>

**udp** and **oic\_udp**: The peer ip address and port number that the
newtmgr or oicmgr on the remote device is listening on. It must be of
the form: **[<ip-address>]:<port-number>**.

.. raw:: html

   </li>

.. raw:: html

   <li>

**ble** and **oic\_ble**: The format is a quoted string of, comma
separated, ``attribute=value`` pairs. The attribute names and the value
for each attribute are:

.. raw:: html

   <ul>

.. raw:: html

   <li>

``peer_name``: A string that specifies the name the peer BLE device
advertises.\ **Note**: If this attribute is specified, you do not need
to specify a value for the ``peer_id`` attribute.

.. raw:: html

   </li>

.. raw:: html

   <li>

``peer_id``: The peer BLE device address or UUID. The format depends on
the OS that the newtmgr tool is running on:

.. raw:: html

   <ul>

.. raw:: html

   </li>

**Linux**: 6 byte BLE address. Each byte must be a hexidecimal number
and separated by a colon.

.. raw:: html

   </li>

.. raw:: html

   <li>

**MacOS**: 128 bit UUID.

.. raw:: html

   </li>

.. raw:: html

   </ul>

\ **Note**: This value is only used when a peer name is not specified
for the connection profile or with the ``--name`` flag option.

.. raw:: html

   </li>

.. raw:: html

   <li>

``ctlr_name``: (Optional) Controller name. This value depends on the OS
that the newtmgr tool is running on.

.. raw:: html

   </li>

.. raw:: html

   </ul>

\ **Notes**:

.. raw:: html

   <ul>

.. raw:: html

   <li>

You must specify ``connstring=" "`` if you do not specify any attribute
values.

.. raw:: html

   </li>

.. raw:: html

   <li>

You can use the ``--name`` flag to specify a device name when you issue
a newtmgr command that communicates with a BLE device. You can use this
flag to override or in lieu of specifying a ``peer_name`` or ``peer_id``
attribute in the connection profile.

.. raw:: html

   </li>

.. raw:: html

   </ul>

.. raw:: html

   <li>

**bhd** and **oic\_bhd**: The format is a quoted string of, comma
separated, ``attribute=value`` pairs. The attribute names and the value
format for each attribute are:

.. raw:: html

   <ul>

.. raw:: html

   <li>

``peer_name``: A string that specifies the name the peer BLE device
advertises. \ **Note**: If this attribute is specified, you do not need
to specify values for the ``peer_addr`` and ``peer_addr_type``
attributes.

.. raw:: html

   </li>

.. raw:: html

   <li>

``peer_addr``: A 6 byte peer BLE device address. Each byte must be a
hexidecimal number and separated by a colon. You must also specify a
``peer_addr_type`` value for the device address. \ **Note:** This value
is only used when a peer name is not specified for the connection
profile or with the ``--name`` flag option.

.. raw:: html

   </li>

.. raw:: html

   <li>

``peer_addr_type``: The peer address type. Valid values are:

.. raw:: html

   <ul>

.. raw:: html

   <li>

**public**: Public address assigned by the manufacturer.

.. raw:: html

   </li>

.. raw:: html

   <li>

**random**: Static random address.

.. raw:: html

   </li>

.. raw:: html

   <li>

**rpa\_pub**: Resolvable Private Address with public identity address.

.. raw:: html

   </li>

.. raw:: html

   <li>

**rpa\_rnd**: Resolvable Private Address with static random identity
address.

.. raw:: html

   </li>

.. raw:: html

   </ul>

\ **Note:** This value is only used when a peer name is not specified
for the connection profile or with the ``--name`` flag option.

.. raw:: html

   </li>

.. raw:: html

   </li>

.. raw:: html

   <li>

``own_addr_type``: (Optional) The address type of the BLE controller for
the host that the newtmgr tool is running on. See the ``peer_addr_type``
attribute for valid values. Defaults to **random**.

.. raw:: html

   </li>

.. raw:: html

   <li>

``ctlr_path``: The path of the port that is used to connect the BLE
controller to the host that the newtmgr tool is running on.

.. raw:: html

   </li>

.. raw:: html

   </ul>

 **Note**: You can use the ``--name`` flag to specify a device name when
you issue a newtmgr command that communicates with a BLE device. You can
use this flag to override or in lieu of specifying a ``peer_name`` or
``peer_addr`` attribute in the connection profile.

.. raw:: html

   </li>

.. raw:: html

   </ul>

.. raw:: html

   </li>

.. raw:: html

   </ul>

.. raw:: html

   </li>

.. raw:: html

   </ul>

delete \| The newtmgr conn delete <conn\_profile> command deletes the
``conn_profile`` connection profile. show \| The newtmgr conn show
[conn\_profile] command shows the information for the ``conn_profile``
connection profile. It shows information for all the connection profiles
if ``conn_profile`` is not specified.

Examples
^^^^^^^^

+----------------+--------------------------+--------------------+
| Sub-command    | Usage                    | Explanation        |
+================+==========================+====================+
| add            | newtmgr conn add         | Creates a          |
|                | myserial02               | connection         |
|                | type=oic\_serial         | profile, named     |
|                | connstring=/dev/ttys002  | ``myserial02``, to |
|                |                          | communicate over a |
|                |                          | serial connection  |
|                |                          | at 115200 baud     |
|                |                          | rate with the      |
|                |                          | oicmgr on a device |
|                |                          | that is connected  |
|                |                          | to the host on     |
|                |                          | port /dev/ttys002. |
+----------------+--------------------------+--------------------+
| add            | newtmgr conn add         | Creates a          |
|                | myserial03 type=serial   | connection         |
|                | connstring="dev=/dev/tty | profile, named     |
|                | s003,                    | ``myserial03``, to |
|                | baud=57600"              | communicate over a |
|                |                          | serial connection  |
|                |                          | at 57600 baud rate |
|                |                          | with the newtmgr   |
|                |                          | on a device that   |
|                |                          | is connected to    |
|                |                          | the host on port   |
|                |                          | /dev/ttys003.      |
+----------------+--------------------------+--------------------+
| add            | newtmgr conn add         | Creates a          |
|                | myudp5683                | connection         |
|                | type=oic\_udpconnstring= | profile, named     |
|                | [127.0.0.1]:5683         | ``myudp5683``, to  |
|                |                          | communicate over   |
|                |                          | UDP with the       |
|                |                          | oicmgr on a device |
|                |                          | listening on       |
|                |                          | localhost and port |
|                |                          | 5683.              |
+----------------+--------------------------+--------------------+
| add            | newtmgr conn add         | Creates a          |
|                | mybleprph type=ble       | connection         |
|                | connstring="peer\_name=n | profile, named     |
|                | imble-bleprph"           | ``mybleprph``, to  |
|                |                          | communicate over   |
|                |                          | BLE, using the     |
|                |                          | native OS BLE      |
|                |                          | support, with the  |
|                |                          | newtmgr on a       |
|                |                          | device named       |
|                |                          | ``nimble-bleprph`` |
|                |                          | .                  |
+----------------+--------------------------+--------------------+
| add            | newtmgr conn add         | Creates a          |
|                | mybletype=ble            | connection         |
|                | connstring=" "           | profile, named     |
|                |                          | ``myble``, to      |
|                |                          | communicate over   |
|                |                          | BLE, using the     |
|                |                          | native OS BLE      |
|                |                          | support, with the  |
|                |                          | newtmgr on a       |
|                |                          | device. You must   |
|                |                          | use the ``--name`` |
|                |                          | flag to specify    |
|                |                          | the device name    |
|                |                          | when you issue a   |
|                |                          | newtmgr command    |
|                |                          | that communicates  |
|                |                          | with the device.   |
+----------------+--------------------------+--------------------+
| add            | newtmgr conn add         | Creates a          |
|                | myblehostd type=oic\_bhd | connection         |
|                | connstring="peer\_name=n | profile, named     |
|                | imble-bleprph,ctlr\_path | ``myblehostd``, to |
|                | =/dev/cu.usbmodem14221"  | communicate over   |
|                |                          | BLE, using the     |
|                |                          | blehostd           |
|                |                          | implementation,    |
|                |                          | with the oicmgr on |
|                |                          | a device named     |
|                |                          | ``nimble-bleprph`` |
|                |                          | .                  |
|                |                          | The BLE controller |
|                |                          | is connected to    |
|                |                          | the host on USB    |
|                |                          | port               |
|                |                          | /dev/cu.usbmodem14 |
|                |                          | 211                |
|                |                          | and uses static    |
|                |                          | random address.    |
+----------------+--------------------------+--------------------+
| delete         | newtmgr conn delete      | Deletes the        |
|                | myserial02               | connection profile |
|                |                          | named              |
|                |                          | ``myserial02``     |
+----------------+--------------------------+--------------------+
| delete         | newtmgr conn delete      | Deletes the        |
|                | myserial02               | connection profile |
|                |                          | named              |
|                |                          | ``myserial02``     |
+----------------+--------------------------+--------------------+
| show           | newtmgr conn show        | Displays the       |
|                | myserial01               | information for    |
|                |                          | the ``myserial01`` |
|                |                          | connection         |
|                |                          | profile.           |
+----------------+--------------------------+--------------------+
| show           | newtmgr conn show        | Displays the       |
|                |                          | information for    |
|                |                          | all connection     |
|                |                          | profiles.          |
+----------------+--------------------------+--------------------+
