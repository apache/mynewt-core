Newt Manager
------------

Newt Manager (newtmgr) is the application tool that enables a user to
communicate with and manage remote devices running the Mynewt OS. It
uses a connection profile to establish a connection with a device and
sends command requests to the device. The tool follows the same command
structure as the `newt tool <../newt/newt_ops.html>`__.

Available high-level commands
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The following are the high-level newtmgr commands. Some of these
commands have subcommands. You can use the -h flag to get help for each
command. See the documentation for each command in this guide if you
need more information and examples.

.. code-block:: console

    Available Commands:
      config      Read or write a config value on a device
      conn        Manage newtmgr connection profiles
      crash       Send a crash command to a device
      datetime    Manage datetime on a device
      echo        Send data to a device and display the echoed back data
      fs          Access files on a device
      help        Help about any command
      image       Manage images on a device
      log         Manage logs on a device
      mpstat      Read mempool statistics from a device
      reset       Perform a soft reset of a device
      run         Run test procedures on a device
      stat        Read statistics from a device
      taskstat    Read task statistics from a device

    Flags:
      -c, --conn string       connection profile to use
      -h, --help              help for newtmgr
      -l, --loglevel string   log level to use (default "info")
          --name string       name of target BLE device; overrides profile setting
      -t, --timeout float     timeout in seconds (partial seconds allowed) (default 10)
      -r, --tries int         total number of tries in case of timeout (default 1)
