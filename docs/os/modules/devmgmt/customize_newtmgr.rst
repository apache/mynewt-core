Customizing Newt Manager Usage with mgmt
----------------------------------------

The **mgmt** package enables you to customize Newt Manager (in either
the newtmgr or oicmgr framerwork) to only process the commands that your
application uses. The newtmgr commands are divided into management
groups. A manager package implements the commands for a group. It
implements the handlers that process the commands for the group and
registers the handlers with mgmt. When newtmgr or oicmgr receives a
newtmgr command, it looks up the handler for the command (by management
group id and command id) from mgmt and calls the handler to process the
command.

The system level management groups are listed in following table:

+--------------------------+---------------------------------------------------------+-----------------------+
| Management Group         | newtmgr Commands                                        | Package               |
+==========================+=========================================================+=======================+
| MGMT\_GROUP\_ID\_DEFAULT | ``echo`` ``taskstat`` ``mpstat`` ``datetime`` ``reset`` | mgmt/newtmgr/nmgr\_os |
+--------------------------+---------------------------------------------------------+-----------------------+
| MGMT\_GROUP\_ID\_IMAGE   | ``image``                                               | mgmt/imgmgr           |
+--------------------------+---------------------------------------------------------+-----------------------+
| MGMT\_GROUP\_ID\_STATS   | ``stat``                                                | sys/stats             |
+--------------------------+---------------------------------------------------------+-----------------------+
| MGMT\_GROUP\_ID\_CONFIG  | ``config``                                              | sys/config            |
+--------------------------+---------------------------------------------------------+-----------------------+
| MGMT\_GROUP\_ID\_LOGS    | ``log``                                                 | sys/log               |
+--------------------------+---------------------------------------------------------+-----------------------+
| MGMT\_GROUP\_ID\_CRASH   |  ``crash``                                              | test/crash\_test      |
+--------------------------+---------------------------------------------------------+-----------------------+
| MGMT\_GROUP\_ID\_RUNTEST | ``run``                                                 | test/runtest          |
+--------------------------+---------------------------------------------------------+-----------------------+


Both newtmgr and ocimgr process the MGMT\_GROUP\_ID\_DEFAULT commands by
default. You can also use mgmt to add user defined management group
commands.
