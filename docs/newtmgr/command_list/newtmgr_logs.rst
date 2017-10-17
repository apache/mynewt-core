newtmgr log 
------------

Manage logs on a device.

Usage:
^^^^^^

.. code-block:: console

        newtmgr log [command] -c <conn_profile> [flags] 

Global Flags:
^^^^^^^^^^^^^

.. code-block:: console

      -c, --conn string       connection profile to use
      -h, --help              help for newtmgr
      -l, --loglevel string   log level to use (default "info")
          --name string       name of target BLE device; overrides profile setting
      -t, --timeout float     timeout in seconds (partial seconds allowed) (default 10)
      -r, --tries int         total number of tries in case of timeout (default 1)

Description
^^^^^^^^^^^

The log command provides subcommands to manage logs on a device. Newtmgr
uses the ``conn_profile`` connection profile to connect to the device.

+----------------+---------------------------+
| Sub-command    | Explanation               |
+================+===========================+
| clear          | The newtmgr log clear     |
|                | command clears the logs   |
|                | on a device.              |
+----------------+---------------------------+
| level\_list    | The newtmgr level\_list   |
|                | command shows the log     |
|                | levels on a device.       |
+----------------+---------------------------+
| list           | The newtmgr log list      |
|                | command shows the log     |
|                | names on a device.        |
+----------------+---------------------------+
| module\_list   | The newtmgr log           |
|                | module\_list command      |
|                | shows the log module      |
|                | names on a device.        |
+----------------+---------------------------+

show \| The newtmgr log show command displays logs on a device. The
command format is: newtmgr log show [log\_name [min-index
[min-timestamp]]] -c <conn\_profile>The following optional parameters
can be used to filter the logs to display:

.. raw:: html

   <ul>

.. raw:: html

   <li>

**log\_name**: Name of log to display. If log\_name is not specified,
all logs are displayed.

.. raw:: html

   </li>

.. raw:: html

   <li>

**min-index**: Minimum index of the log entries to display. This value
is only valid when a log\_name is specified. The value can be ``last``
or a number. If the value is ``last``, only the last log entry is
displayed. If the value is a number, log entries with an index equal to
or higher than min-index are displayed.

.. raw:: html

   </li>

.. raw:: html

   <li>

**min-timestamp**: Minimum timestamp of log entries to display. The
value is only valid if min-index is specified and is not the value
``last``. Only log entries with a timestamp equal to or later than
min-timestamp are displayed. Log entries with a timestamp equal to
min-timestamp are only displayed if the log entry index is equal to or
higher than min-index.

.. raw:: html

   </li>

.. raw:: html

   </ul>

Examples
^^^^^^^^

+----------------+--------------------------+--------------------+
| Sub-command    | Usage                    | Explanation        |
+================+==========================+====================+
| clear          | newtmgr log clear-c      | Clears the logs on |
|                | profile01                | a device. Newtmgr  |
|                |                          | connects to the    |
|                |                          | device over a      |
|                |                          | connection         |
|                |                          | specified in the   |
|                |                          | ``profile01``      |
|                |                          | connection         |
|                |                          | profile.           |
+----------------+--------------------------+--------------------+
| level\_list    | newtmgr log level\_list  | Shows the log      |
|                | -c profile01             | levels on a        |
|                |                          | device. Newtmgr    |
|                |                          | connects to the    |
|                |                          | device over a      |
|                |                          | connection         |
|                |                          | specified in the   |
|                |                          | ``profile01``      |
|                |                          | connection         |
|                |                          | profile.           |
+----------------+--------------------------+--------------------+
| list           | newtmgr log list-c       | Shows the log      |
|                | profile01                | names on a device. |
|                |                          | Newtmgr connects   |
|                |                          | to the device over |
|                |                          | a connection       |
|                |                          | specified in the   |
|                |                          | ``profile01``      |
|                |                          | connection         |
|                |                          | profile.           |
+----------------+--------------------------+--------------------+
| module\_list   | newtmgr log              | Shows the log      |
|                | module\_list-c profile01 | module names on a  |
|                |                          | device. Newtmgr    |
|                |                          | connects to the    |
|                |                          | device over a      |
|                |                          | connection         |
|                |                          | specified in the   |
|                |                          | ``profile01``      |
|                |                          | connection         |
|                |                          | profile.           |
+----------------+--------------------------+--------------------+
| show           | newtmgr log show -c      | Displays all logs  |
|                | profile01                | on a device.       |
|                |                          | Newtmgr connects   |
|                |                          | to the device over |
|                |                          | a connection       |
|                |                          | specified in the   |
|                |                          | ``profile01``      |
|                |                          | connection         |
|                |                          | profile.           |
+----------------+--------------------------+--------------------+
| show           | newtmgr log show         | Displays all log   |
|                | reboot\_log -c profile01 | entries for the    |
|                |                          | reboot\_log on a   |
|                |                          | device. Newtmgr    |
|                |                          | connects to the    |
|                |                          | device over a      |
|                |                          | connection         |
|                |                          | specified in the   |
|                |                          | ``profile01``      |
|                |                          | connection         |
|                |                          | profile.           |
+----------------+--------------------------+--------------------+
| show           | newtmgr log show         | Displays the last  |
|                | reboot\_log last -c      | entry from the     |
|                | profile01                | reboot\_log on a   |
|                |                          | device. Newtmgr    |
|                |                          | connects to the    |
|                |                          | device over a      |
|                |                          | connection         |
|                |                          | specified in the   |
|                |                          | ``profile01``      |
|                |                          | connection         |
|                |                          | profile.           |
+----------------+--------------------------+--------------------+
| show           | newtmgr log show         | Displays the       |
|                | reboot\_log 2 -c         | reboot\_log log    |
|                | profile01                | entries with an    |
|                |                          | index 2 and higher |
|                |                          | on a device.       |
|                |                          | Newtmgr connects   |
|                |                          | to the device over |
|                |                          | a connection       |
|                |                          | specified in the   |
|                |                          | ``profile01``      |
|                |                          | connection         |
|                |                          | profile.           |
+----------------+--------------------------+--------------------+
| show           | newtmgr log show         | Displays the       |
|                | reboot\_log 5 123456 -c  | reboot\_log log    |
|                | profile01                | entries with a     |
|                |                          | timestamp higher   |
|                |                          | than 123456 and    |
|                |                          | log entries with a |
|                |                          | timestamp equal to |
|                |                          | 123456 and an      |
|                |                          | index equal to or  |
|                |                          | higher than 5.     |
|                |                          | Newtmgr connects   |
|                |                          | to the device over |
|                |                          | a connection       |
|                |                          | specified in the   |
|                |                          | ``profile01``      |
|                |                          | connection         |
|                |                          | profile.           |
+----------------+--------------------------+--------------------+
