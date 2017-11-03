newtmgr taskstat 
-----------------

Read task statistics from a device.

Usage:
^^^^^^

.. code-block:: console

        newtmgr taskstat -c <conn_profile> [flags] 

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

Reads and displays the task statistics from a device. Newtmgr uses the
``conn_profile`` connection profile to connect to the device. It lists
the following statistics for each task:

-  **task**: Task name
-  **pri**: Task priority
-  **runtime**: The time (ms) that the task has been running for
-  **csw**: Number of times the task has switched context
-  **stksz**: Stack size allocated for the task
-  **stkuse**: Actual stack size the task uses
-  **last\_checkin**: Last sanity checkin with the `Sanity
   Task </os/core_os/sanity/sanity.html>`__
-  **next\_checkin**: Next sanity checkin

Examples
^^^^^^^^

+----------------+--------------------------+--------------------+
| Sub-command    | Usage                    | Explanation        |
+================+==========================+====================+
| newtmgr        | Reads and displays the   |
| taskstat-c     | task statistics from a   |
| profile0       | device. Newtmgr connects |
|                | to the device over a     |
|                | connection specified in  |
|                | the ``profile01``        |
|                | connection profile.      |
+----------------+--------------------------+--------------------+

Here is an example output for the ``myble`` application from the
`Enabling Newt Manager in any app </os/tutorials/add_newtmgr.html>`__
tutiorial:

.. code-block:: console

    $ newtmgr taskstat -c myserial 
          task pri tid  runtime      csw    stksz   stkuse last_checkin next_checkin
        ble_ll   0   2        0       12       80       58        0        0
          idle 255   0    16713       95       64       31        0        0
          main 127   1        2       81      512      275        0        0
