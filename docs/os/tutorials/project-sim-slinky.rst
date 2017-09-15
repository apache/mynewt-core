Project Sim Slinky
------------------

This tutorial shows you how to create, build and run the Slinky
application and communicate with newtmgr for a simulated device. This is
supported on Mac OS and Linux platforms.

 ### Prerequisites

Meet the prerequisites listed in `Project
Slinky </os/tutorials/project-slinky.html>`__.

Creating a new project
~~~~~~~~~~~~~~~~~~~~~~

Instructions for creating a project are located in the `Basic
Setup <../get_started/project_create/>`__ section of the `Mynewt
Documentation <../introduction/>`__

We will list only the steps here for brevity. We will name the project
``slinky``.

.. code-block:: console

    $ newt new slinky
    Downloading project skeleton from apache/mynewt-blinky...
    ...
    Installing skeleton in slink...
    Project slinky successfully created
    $ cd slinky
    $newt install
    apache-mynewt-core

Setting up your target build
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Create a target for ``slinky`` using the native bsp. We will list only
the steps and suppress the tool output here for brevity.

.. code-block:: console

        $ newt target create sim_slinky
        $ newt target set sim_slinky bsp=@apache-mynewt-core/hw/bsp/native
        $ newt target set sim_slinky build_profile=debug
        $ newt target set sim_slinky app=@apache-mynewt-core/apps/slinky

Building Your target
~~~~~~~~~~~~~~~~~~~~

To build your target, use ``newt build``. When complete, an executable
file is created.

.. code-block:: console

        $ newt build sim_slinky 
        Building target targets/sim_slinky
        Compiling repos/apache-mynewt-core/boot/bootutil/src/image_ec256.c
        Compiling repos/apache-mynewt-core/boot/bootutil/src/image_rsa.c
        Compiling repos/apache-mynewt-core/boot/bootutil/src/image_ec.c
        Compiling repos/apache-mynewt-core/boot/split/src/split.c
        Compiling repos/apache-mynewt-core/boot/bootutil/src/image_validate.c
        Compiling repos/apache-mynewt-core/boot/bootutil/src/loader.c
        Compiling repos/apache-mynewt-core/boot/bootutil/src/bootutil_misc.c
        Compiling repos/apache-mynewt-core/crypto/mbedtls/src/aesni.c
        Compiling repos/apache-mynewt-core/crypto/mbedtls/src/aes.c
        Compiling repos/apache-mynewt-core/boot/split/src/split_config.c
        Compiling repos/apache-mynewt-core/apps/slinky/src/main.c

                  ...

        Archiving util_crc.a
        Archiving util_mem.a
        Linking ~/dev/slinky/bin/targets/sim_slinky/app/apps/slinky/slinky.elf
        Target successfully built: targets/sim_slinky

Run the target
~~~~~~~~~~~~~~

Run the executable you have build for the simulated environment. The
serial port name on which the simulated target is connected is shown in
the output when mynewt slinky starts.

.. code-block:: console

        $ ~/dev/slinky/bin/targets/sim_slinky/app/apps/slinky/slinky.elf
        uart0 at /dev/ttys005

In this example, the slinky app opened up a com port ``/dev/ttys005``
for communications with newtmgr.

**NOTE:** This application will block. You will need to open a new
console (or execute this in another console) to continue the tutorial.\*

Setting up a connection profile
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

You will now set up a connection profile using ``newtmgr`` for the
serial port connection and start communicating with the simulated remote
device.

.. code-block:: console

        $ newtmgr conn add sim1 type=serial connstring=/dev/ttys005
        Connection profile sim1 successfully added
        $ newtmgr conn show
        Connection profiles: 
          sim1: type=serial, connstring='/dev/ttys005'

Executing newtmgr commands with the target
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

You can now use connection profile ``sim1`` to talk to the running
sim\_slinky. As an example, we will query the running mynewt OS for the
usage of its memory pools.

.. code-block:: console

        $ newtmgr -c sim1 mpstat
        Return Code = 0
                                name blksz  cnt free  min
                              msys_1   292   12   10   10

As a test command, you can send an arbitrary string to the target and it
will echo that string back in a response to newtmgr.

.. code-block:: console

        $ newtmgr -c sim1 echo "Hello Mynewt"
        Hello Mynewt

In addition to these, you can also examine running tasks, statistics,
logs, image status (not on sim), and configuration.
