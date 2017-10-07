Project Slinky
--------------

The goal of the project is to use a sample application called "Slinky"
included in the Mynewt repository to enable remote communications with a
device running the Mynewt OS. The protocol for remote communications is
called newt manager (newtmgr).

If you have an existing project using a target that does not use the
Slinky application and you wish to add newtmgr functionality to it,
check out the tutorial titled `Enable newtmgr in any
app <add_newtmgr.html>`__.

Available Tutorials
~~~~~~~~~~~~~~~~~~~

Tutorials are available for the following boards:

-  `Slinky on a simulated device </os/tutorials/project-sim-slinky>`__.
   This is supported on Mac OS and Linux platforms.
-  `Slinky on a nRF52 </os/tutorials/project-nrf52-slinky>`__.
-  `Slinky on an Olimex </os/tutorials/project-stm32-slinky>`__. ###
   Prerequisites

Ensure that you meet the following prerequisites before continuing with
this tutorial:

-  Have Internet connectivity to fetch remote Mynewt components.
-  Have a computer to build a Mynewt application and connect to the
   board over USB.
-  Have a Micro-USB cable to connect the board and the computer.
-  Have a `serial port setup </os/get_started/serial_access.html>`__.
-  Install the newt tool and the toolchains (See `Basic
   Setup </os/get_started/get_started.html>`__).
-  Install the `newtmgr tool </newtmgr/install_mac.html>`__.
-  Read the Mynewt OS `Concepts </os/get_started/vocabulary.html>`__
   section.
-  Create a project space (directory structure) and populated it with
   the core code repository (apache-mynewt-core) or kn ow how to as
   explained in `Creating Your First
   Project </os/get_started/project_create>`__.

Overview of Steps
~~~~~~~~~~~~~~~~~

-  Install dependencies.
-  Define the bootloader and Slinky application target for the target
   board.
-  Build the bootloader target.
-  Build the Slinky application target and create an application image.
-  Set a up serial connection with the targets.
-  Create a connection profile using the newtmgr tool.
-  Use the newtmgr tool to communicate with the targets.
