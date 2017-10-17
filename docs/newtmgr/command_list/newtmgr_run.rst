newtmgr run 
------------

Run test procedures on a device.

Usage:
^^^^^^

.. code-block:: console

        newtmgr run [command] -c <conn_profile> [flags] 

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

The run command provides subcommands to run test procedures on a device.
Newtmgr uses the ``conn_profile`` connection profile to connect to the
device.

+----------------+---------------------------+
| Sub-command    | Explanation               |
+================+===========================+
| list           | The newtmgr run list      |
|                | command lists the         |
|                | registered tests on a     |
|                | device.                   |
+----------------+---------------------------+
| test           | The newtmgr run test      |
|                | [all\|testname]           |
|                | [token-value] command     |
|                | runs the ``testname``     |
|                | test or all tests on a    |
|                | device. All tests are run |
|                | if ``all`` or no          |
|                | ``testname`` is           |
|                | specified. If a           |
|                | ``token-value`` is        |
|                | specified, the token      |
|                | value is output with the  |
|                | log messages.             |
+----------------+---------------------------+

Examples
^^^^^^^^

+----------------+--------------------------+--------------------+
| Sub-command    | Usage                    | Explanation        |
+================+==========================+====================+
| list           | newtmgr runlist -c       | Lists all the      |
|                | profile01                | registered tests   |
|                |                          | on a device.       |
|                |                          | Newtmgr connects   |
|                |                          | to the device over |
|                |                          | a connection       |
|                |                          | specified in the   |
|                |                          | ``profile01``      |
|                |                          | connection         |
|                |                          | profile.           |
+----------------+--------------------------+--------------------+
| test           | newtmgr run test         | Runs all the tests |
|                | all201612161220-c        | on a device.       |
|                | profile01                | Outputs the        |
|                |                          | ``201612161220``   |
|                |                          | token with the log |
|                |                          | messages. Newtmgr  |
|                |                          | connects to the    |
|                |                          | device over a      |
|                |                          | connection         |
|                |                          | specified in the   |
|                |                          | ``profile01``      |
|                |                          | connection         |
|                |                          | profile.           |
+----------------+--------------------------+--------------------+
| test           | newtmgr run test         | Runs the           |
|                | mynewtsanity-c profile01 | ``mynewtsanity``   |
|                |                          | test on a device.  |
|                |                          | Newtmgr connects   |
|                |                          | to the device over |
|                |                          | a connection       |
|                |                          | specified in the   |
|                |                          | ``profile01``      |
|                |                          | connection         |
|                |                          | profile.           |
+----------------+--------------------------+--------------------+
