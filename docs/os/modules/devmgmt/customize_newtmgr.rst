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

.. raw:: html

   <table style="width:90%" align="center">

.. raw:: html

   <td>

Management Group

.. raw:: html

   </td>

.. raw:: html

   <td>

newtmgr Commands

.. raw:: html

   </td>

.. raw:: html

   <td>

Package

.. raw:: html

   </td>

.. raw:: html

   <tr>

.. raw:: html

   <td>

MGMT\_GROUP\_ID\_DEFAULT

.. raw:: html

   </td>

.. raw:: html

   <td>

``echo`` ``taskstat`` ``mpstat`` ``datetime`` ``reset``

.. raw:: html

   </td>

.. raw:: html

   <td>

mgmt/newtmgr/nmgr\_os

.. raw:: html

   </td>

.. raw:: html

   </tr>

.. raw:: html

   <tr>

.. raw:: html

   <td>

MGMT\_GROUP\_ID\_IMAGE

.. raw:: html

   </td>

.. raw:: html

   <td>

``image``

.. raw:: html

   </td>

.. raw:: html

   <td>

mgmt/imgmgr

.. raw:: html

   </td>

.. raw:: html

   </tr>

.. raw:: html

   <tr>

.. raw:: html

   <td>

MGMT\_GROUP\_ID\_STATS

.. raw:: html

   </td>

.. raw:: html

   <td>

``stat``

.. raw:: html

   </td>

.. raw:: html

   <td>

sys/stats

.. raw:: html

   </td>

.. raw:: html

   </tr>

.. raw:: html

   <tr>

.. raw:: html

   <td>

MGMT\_GROUP\_ID\_CONFIG

.. raw:: html

   </td>

.. raw:: html

   <td>

``config``

.. raw:: html

   </td>

.. raw:: html

   <td>

sys/config

.. raw:: html

   </td>

.. raw:: html

   </tr>

.. raw:: html

   <tr>

.. raw:: html

   <td>

MGMT\_GROUP\_ID\_LOGS

.. raw:: html

   </td>

.. raw:: html

   <td>

``log``

.. raw:: html

   </td>

.. raw:: html

   <td>

sys/log

.. raw:: html

   </td>

.. raw:: html

   </tr>

.. raw:: html

   <tr>

.. raw:: html

   <td>

MGMT\_GROUP\_ID\_CRASH

.. raw:: html

   </td>

.. raw:: html

   <td>

``crash``

.. raw:: html

   </td>

.. raw:: html

   <td>

test/crash\_test

.. raw:: html

   </td>

.. raw:: html

   </tr>

.. raw:: html

   <tr>

.. raw:: html

   <td>

MGMT\_GROUP\_ID\_RUNTEST

.. raw:: html

   </td>

.. raw:: html

   <td>

``run``

.. raw:: html

   </td>

.. raw:: html

   <td>

test/runtest

.. raw:: html

   </td>

.. raw:: html

   </tr>

.. raw:: html

   </table>

Both newtmgr and ocimgr process the MGMT\_GROUP\_ID\_DEFAULT commands by
default. You can also use mgmt to add user defined management group
commands.
