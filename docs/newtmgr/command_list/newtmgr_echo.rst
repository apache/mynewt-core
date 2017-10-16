newtmgr echo 
-------------

Send data to a device and display the echoed back data.

Usage:
^^^^^^

.. code-block:: console

        newtmgr echo <text> -c <conn_profile> [flags] 

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

Sends the ``text`` to a device and outputs the text response from the
device. Newtmgr uses the ``conn_profile`` connection profile to connect
to the device.

Examples
^^^^^^^^

+----------------+--------------------------+--------------------+
| Sub-command    | Usage                    | Explanation        |
+================+==========================+====================+
| newtmgr echo   | Sends the text 'hello'   |
| hello-c        | to a device and displays |
| profile01      | the echoed back data.    |
|                | Newtmgr connects to the  |
|                | device over a connection |
|                | specified in the         |
|                | ``profile01`` profile.   |
+----------------+--------------------------+--------------------+
