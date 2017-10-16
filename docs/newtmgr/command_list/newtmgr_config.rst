newtmgr config 
---------------

Read and write config values on a device.

Usage:
^^^^^^

.. code-block:: console

        newtmgr config <var-name> [var-value] -c <conn_profile> [flags] 

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

Reads and sets the value for the ``var-name`` config variable on a
device. Specify a ``var-value`` to set the value for the ``var-name``
variable. Newtmgr uses the ``conn_profile`` connection profile to
connect to the device.

Examples
^^^^^^^^

+----------------+--------------------------+--------------------+
| Sub-command    | Usage                    | Explanation        |
+================+==========================+====================+
| newtmgr config | Reads the ``myvar``      |
| myvar -c       | config variable value    |
| profile01      | from a device. Newtmgr   |
|                | connects to the device   |
|                | over a connection        |
|                | specified in the         |
|                | ``profile01`` connection |
|                | profile.                 |
+----------------+--------------------------+--------------------+
| newtmgr config | Sets the ``myvar``       |
| myvar 2 -c     | config variable to the   |
| profile01      | value ``2`` on a device. |
|                | Newtmgr connects to the  |
|                | device over a connection |
|                | specified in the         |
|                | ``profile01`` connection |
|                | profile.                 |
+----------------+--------------------------+--------------------+
