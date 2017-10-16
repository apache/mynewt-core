newtmgr fs
----------

Access files on a device.

Usage:
^^^^^^

.. code-block:: console

        newtmgr fs [command] -c <conn_profile> [flags] 

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

The fs command provides the subcommands to download a file from and
upload a file to a device. Newtmgr uses the ``conn_profile`` connection
profile to connect to the device.

+----------------+---------------------------+
| Sub-command    | Explanation               |
+================+===========================+
| download       | The newtmgr download      |
|                | <src-filename>            |
|                | <dst-filename> command    |
|                | downloads the file named  |
|                | <src-filename> from a     |
|                | device and names it       |
|                | <dst-filename> on your    |
|                | host.                     |
+----------------+---------------------------+
| upload         | The newtmgr upload        |
|                | <src-filename>            |
|                | <dst-filename> command    |
|                | uploads the file named    |
|                | <src-filename> to a       |
|                | device and names the file |
|                | <dst-filename> on the     |
|                | device.                   |
+----------------+---------------------------+

Examples
^^^^^^^^

+----------------+--------------------------+--------------------+
| Sub-command    | Usage                    | Explanation        |
+================+==========================+====================+
| download       | newtmgr fs download      | Downloads the file |
|                | /cfg/mfg mfg.txt -c      | name ``/cfg/mfg``  |
|                | profile01                | from a device and  |
|                |                          | names the file     |
|                |                          | ``mfg.txt`` on     |
|                |                          | your host. Newtmgr |
|                |                          | connects to the    |
|                |                          | device over a      |
|                |                          | connection         |
|                |                          | specified in the   |
|                |                          | ``profile01``      |
|                |                          | connection         |
|                |                          | profile.           |
+----------------+--------------------------+--------------------+
| upload         | newtmgr fs upload        | Uploads the file   |
|                | mymfg.txt /cfg/mfg -c    | name ``mymfg.txt`` |
|                | profile01                | to a device and    |
|                |                          | names the file     |
|                |                          | ``cfg/mfg`` on the |
|                |                          | device. Newtmgr    |
|                |                          | connects to the    |
|                |                          | device over a      |
|                |                          | connection         |
|                |                          | specified in the   |
|                |                          | ``profile01``      |
|                |                          | connection         |
|                |                          | profile.           |
+----------------+--------------------------+--------------------+
