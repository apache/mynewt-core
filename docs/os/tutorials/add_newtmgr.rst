Enabling Newt Manager in Your Application
-----------------------------------------

In order for your application to communicate with the newtmgr tool and
process Newt Manager commands, you must enable Newt Manager device
management and the support to process Newt Manager commands in your
application. This tutorial explains how to add the support to your
application.

This tutorial assumes that you have read the `Device Management with
Newt Manager </os/modules/devmgmt/newtmgr/>`__ guide and are familiar
with the ``newtmgr`` and ``oicmgr`` frameworks and all the options that
are available to customize your application.

This tutorial shows you how to configure your application to:

-  Use the newtmgr framework.
-  Use serial transport to communicate with the newtmgr tool.
-  Support all Newt Manager commands.

See `Other Configuration Options <#other-configuration-options>`__ on
how to customize your application.

Prerequisites
~~~~~~~~~~~~~

Ensure that you have met the following prerequisites before continuing
with this tutorial:

-  Have Internet connectivity to fetch remote Mynewt components.
-  Have a cable to establish a serial USB connection between the board
   and the laptop.
-  Install the newt tool and toolchains (See `Basic
   Setup </os/get_started/get_started.html>`__).
-  Install the `newtmgr tool <../../newtmgr/install_mac.html>`__.

Use an Existing Project
~~~~~~~~~~~~~~~~~~~~~~~

We assume that you have worked through at least some of the other
tutorials and have an existing project. In this example, we modify the
``btshell`` app to enable Newt Manager support. We call our target
``myble``. You can create the target using any name you choose.

Modify Package Dependencies and Configurations
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Add the following packages to the ``pkg.deps`` parameter in your target
or application ``pkg.yml`` file:

.. code-block:: console


    pkg.deps:
        - mgmt/newtmgr
        - mgmt/newtmgr/transport/nmgr_shell
        - mgmt/imgmgr
        - sys/log/full
        - sys/stats/full
        - sys/config
        - test/crash_test
        - test/runtest

Each package provides the following Newt Manager functionality:

-  ``mgmt/newtmgr``: Supports the newtmgr framework and the Newt Manager
   ``echo``, ``taskstat`` ``mpstat``, ``datetime``, and ``reset``
   commands.
-  ``mgmt/newtmgr/transport/nmgr_shell``: Supports serial transport.
-  ``mgmt/imgmgr``: Supports the ``newtmgr image`` command
-  ``sys/log/full`` : Supports the ``newtmgr log`` command.
-  ``sys/stats/full``: Supports the ``newtmgr stat`` command.
-  ``sys/config``: Supports the ``newtmgr config`` command.
-  ``test/crash_test``: Supports the ``newtmgr crash`` command.
-  ``test/runtest``: Supports the ``newt run`` command.

Add the following configuration setting values to the ``syscfg.vals``
parameter in the target or application ``syscfg.yml`` file:

.. code-block:: console


    syscfg.vals:
        LOG_NEWTMGR: 1
        STATS_NEWTMGR: 1
        CONFIG_NEWTMGR: 1
        CRASH_TEST_NEWTMGR: 1
        RUNTEST_NEWTMGR: 1
        SHELL_TASK: 1
        SHELL_NEWTMGR: 1

The first five configuration settings enable support for the Newt
Manager ``log``, ``stat``, ``config``, ``crash``, and ``run`` commands.
The ``SHELL_TASK`` setting enables the shell for serial transport. The
``SHELL_NEWTMGR`` setting enables newtmgr support in the shell.

Note that you may need to override additional configuration settings
that are specific to each package to customize the package
functionality.

Modify the Source
~~~~~~~~~~~~~~~~~

By default, the ``mgmt`` package uses the Mynewt default event queue to
receive request events from the newtmgr tool. These events are processed
in the context of the application main task.

You can specify a different event queue for the package to use. If you
choose to use a dedicated event queue, you must create a task to process
events from this event queue. The ``mgmt`` package executes and handles
newtmgr request events in the context of this task. The ``mgmt`` package
exports the ``mgmt_evq_set()`` function that allows you to specify an
event queue.

This example uses the Mynewt default event queue and you do not need to
modify your application source.

If you choose to use a different event queue, see `Events and Event
Queues <event_queue.html>`__ for details on how to initialize an event
queue and create a task to process the events. You will also need to
modify your ``main.c`` to add the call to the ``mgmt_evq_set()``
function as follows:

Add the ``mgmt/mgmt.h`` header file:

.. code-block:: console


    #include <mgmt/mgmt.h>

Add the call to specify the event queue. In the ``main()`` function,
scroll down to the ``while (1)`` loop and add the following statement
above the loop:

.. code-block:: console


    mgmt_evq_set(&my_eventq)

where ``my_eventq`` is an event queue that you have initialized.

Build the Targets
~~~~~~~~~~~~~~~~~

Build the two targets as follows:

::

    $ newt build nrf52_boot
    <snip>
    App successfully built: ./bin/nrf52_boot/apps/boot/boot.elf
    $ newt build myble
    Compiling hci_common.c
    Compiling util.c
    Archiving nimble.a
    Compiling os.c
    <snip>

Create the Application Image
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Generate an application image for the ``myble`` target. You can use any
version number you choose.

::

    $ newt create-image myble 1.0.0
    App image successfully generated: ./bin/makerbeacon/apps/btshell/btshell.img
    Build manifest: ./bin/makerbeacon/apps/btshell/manifest.json

Load the Image
~~~~~~~~~~~~~~

Ensure the USB connector is in place and the power LED on the board is
lit. Turn the power switch on your board off, then back on to reset the
board after loading the image.

::

    $ newt load nrf52_boot
    $ newt load myble

Set Up a Connection Profile
~~~~~~~~~~~~~~~~~~~~~~~~~~~

The newtmgr tool requires a connection profile in order to connect to
your board. If you have not done so, follow the
`instructions <../../newtmgr/command_list/newtmgr_conn.html>`__ for
setting up your connection profile.

Communicate with Your Application
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Once you have a connection profile set up, you can connect to your
device with ``newtmgr -c myconn <command>`` to run commands in your
application.

Issue the ``echo`` command to ensure that your application is
communicating with the newtmgr tool:

.. code-block:: console


    # newtmgr -c myconn echo hello
    hello

Test your application to ensure that it can process a Newt Manager
command that is supported by a different package. Issue the ``stat``
command to see the BLE stats.

.. code-block:: console


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

                   ...

             0 read_type_req_tx
             0 read_type_rsp_rx
             0 read_type_rsp_tx
             0 write_cmd_rx
             0 write_cmd_tx
             0 write_req_rx
             0 write_req_tx
             0 write_rsp_rx
             0 write_rsp_tx

Your application is now able to communicate with the newtmgr tool.

Other Configuration Options
~~~~~~~~~~~~~~~


This section explains how to customize your application to use other
Newt Manager protocol options.

Newtmgr Framework Transport Protocol Options
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The newtmgr framework currently supports BLE and serial transport
protocols. To configure the transport protocols that are supported,
modify the ``pkg.yml`` and ``syscfg.yml`` files as follows:

-  Add the ``mgmt/newtmgr/transport/ble`` package to the ``pkg.deps``
   parameter to enable BLE transport.
-  Add the ``mgmt/newtmgr/transport/nmgr_shell`` package to the
   ``pkg.deps`` parameter, and add ``SHELL_TASK: 1`` and
   ``SHELL_NEWTMGR`` to the ``syscfg.vals`` parameter to enable serial
   transport when your application also uses the
   `Shell </os/modules/shell/shell.html>`__.
-  Add the ``mgmt/newtmgr/transport/nmgr_uart`` package to the
   ``pkg.deps`` parameter to enable serial transport over a UART port.
   You can use this package instead of the ``nmgr_shell`` package when
   your application does not use the
   `Shell </os/modules/shell/shell.html>`__ or you want to use a dedicated
   UART port to communicate with newtmgr. You can change the
   ``NMGR_UART`` and ``NMGR_URART_SPEED`` sysconfig values to specify a
   different port.

Oicmgr Framework Options
^^^^^^^^^^^^^^^^^^^^^^^^

To use the oicmgr framework instead of the newtmgr framework, modify the
``pkg.yml`` and ``syscfg.yml`` files as follows:

-  Add the ``mgmt/oicmgr`` package (instead of the ``mgmt/newtmgr`` and
   ``mgmt/newtmgr/transport`` packages as described previously) to the
   ``pkg.deps`` parameter.
-  Add ``OC_SERVER: 1`` to the ``syscfg.vals`` parameter.

Oicmgr supports the IP, serial, and BLE transport protocols. To
configure the transport protocols that are supported, set the
configuration setting values in the ``syscfg.vals`` parameter as
follows:

-  Add ``OC_TRANSPORT_IP: 1`` to enable IP transport.
-  Add ``OC_TRANSPORT_GATT: 1`` to enable BLE transport.
-  Add ``OC_TRANSPORT_SERIAL: 1``, ``SHELL_TASK: 1``,
   ``SHELL_NEWTMGR:1`` to enable serial transport.

Customize the Newt Manager Commands that Your Application Supports
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

We recommend that you only enable support for the Newt Manager commands
that your application uses to reduce your application code size. To
configure the commands that are supported, set the configuration setting
values in the ``syscfg.vals`` parameter as follows:

-  Add ``LOG_NEWTMGR: 1`` to enable support for the ``newtmgr log``
   command.
-  Add ``STATS_NEWTMGR: 1`` to enable support for the ``newtmgr stat``
   command.
-  Add ``CONFIG_NEWTMGR: 1`` to enable support for the
   ``newtmgr config`` command.
-  Add ``CRASH_TEST_NEWTMGR: 1`` to enable support for the
   ``newtmgr crash`` command.
-  Add ``RUNTEST_NEWTMGR: 1`` to enable support for the
   ``newtmgr crash`` command.

Notes:

-  When you enable Newt Manager support, using either the newtmgr or
   oicmgr framework, your application automatically supports the Newt
   Manager ``echo``, ``taskstat``, ``mpstat``, ``datetime``, and
   ``reset`` commands. These commands cannot be configured individually.
-  The ``mgmt/imgmgr`` package does not provide a configuration setting
   to enable or disable support for the ``newtmgr image`` command. Do
   not specify the package in the ``pkg.deps`` parameter if your device
   has limited flash memory and cannot support Over-The-Air (OTA)
   firmware upgrades.
