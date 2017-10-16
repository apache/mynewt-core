newtmgr stat
------------

Read statistics from a device.

Usage:
^^^^^^

.. code-block:: console

        newtmgr stat <stats_name> -c <conn_profile> [flags] 
        newtmgr stat [command] -c <conn_profile> [flags] 

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

Displays statistic for the Stats named ``stats_name`` from a device. You
can use the ``list`` subcommand to get a list of the Stats names from
the device. Newtmgr uses the ``conn_profile`` connection profile to
connect to the device.

+----------------+--------------------------+
| Sub-command    | Explanation              |
+================+==========================+
| The newtmgr    |
| stat command   |
| displays the   |
| statistics for |
| the            |
| ``stats_name`` |
| Stats from a   |
| device.        |
+----------------+--------------------------+
| list           | The newtmgr stat list    |
|                | command displays the     |
|                | list of Stats names from |
|                | a device.                |
+----------------+--------------------------+

Examples
^^^^^^^^

+----------------+--------------------------+--------------------+
| Sub-command    | Usage                    | Explanation        |
+================+==========================+====================+
| newtmgr stat   | Displays the ``ble_att`` |
| ble\_att -c    | statistics on a device.  |
| profile01      | Newtmgr connects to the  |
|                | device over a connection |
|                | specified in the         |
|                | ``profile01`` connection |
|                | profile.                 |
+----------------+--------------------------+--------------------+
| list           | newtmgr stat list -c     | Displays the list  |
|                | profile01                | of Stats names     |
|                |                          | from a device.     |
|                |                          | Newtmgr connects   |
|                |                          | to the device over |
|                |                          | a connection       |
|                |                          | specified in the   |
|                |                          | ``profile01``      |
|                |                          | connection         |
|                |                          | profile.           |
+----------------+--------------------------+--------------------+

Here are some example outputs for the ``myble`` application from the
`Enabling Newt Manager in any app </os/tutorials/add_newtmgr.html>`__
tutiorial:

The statistics for the ble\_att Stats:

.. code-block:: console


    $ newtmgr stat ble_att -c myserial
    stat group: ble_att
             0 error_rsp_rx
             0 error_rsp_tx
             0 exec_write_req_rx
             0 exec_write_req_tx
             0 exec_write_rsp_rx
             0 exec_write_rsp_tx
             0 find_info_req_rx
             0 find_info_req_tx
             0 find_info_rsp_rx
             0 find_info_rsp_tx
             0 find_type_value_req_rx
             0 find_type_value_req_tx
             0 find_type_value_rsp_rx
             0 find_type_value_rsp_tx

                   ...

             0 read_rsp_rx
             0 read_rsp_tx
             0 read_type_req_rx
             0 read_type_req_tx
             0 read_type_rsp_rx
             0 read_type_rsp_tx
             0 write_cmd_rx
             0 write_cmd_tx
             0 write_req_rx
             0 write_req_tx
             0 write_rsp_rx
             0 write_rsp_tx

 The list of Stats names using the list subcommand:

.. code-block:: console


    $ newtmgr stat list -c myserial
    stat groups:
        ble_att
        ble_gap
        ble_gattc
        ble_gatts
        ble_hs
        ble_l2cap
        ble_ll
        ble_ll_conn
        ble_phy
        stat
