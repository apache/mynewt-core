Project Slinky using the Nordic nRF52 Board
-------------------------------------------

This tutorial shows you how to create, build and run the Slinky
application and communicate with newtmgr for a Nordic nRF52 board.

Prerequisites
~~~~~~~~~~~~~

-  Meet the prerequisites listed in `Project
   Slinky </os/tutorials/project-slinky.html>`__.
-  Have a Nordic nRF52-DK board.
-  Install the `Segger JLINK Software and documentation
   pack <https://www.segger.com/jlink-software.html>`__.

Create a New Project
~~~~~~~~~~~~~~~~~~~~

Create a new project if you do not have an existing one. You can skip
this step and proceed to `create the targets <#create_targets>`__ if you
already have a project created or completed the `Sim
Slinky <project-slinky.html>`__ tutorial.

Run the following commands to create a new project. We name the project
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

 Create the Targets
~~~~~~~~~~~~~~~~~~~

Create two targets for the nRF52-DK board - one for the bootloader and
one for the Slinky application.

Run the following ``newt target`` commands, from your project directory,
to create a bootloader target. We name the target ``nrf52_boot``.

.. code-block:: console

    $ newt target create nrf52_boot
    $ newt target set nrf52_boot bsp=@apache-mynewt-core/hw/bsp/nrf52dk
    $ newt target set nrf52_boot build_profile=optimized
    $ newt target set nrf52_boot app=@apache-mynewt-core/apps/boot

 Run the following ``newt target`` commands to create a target for the
Slinky application. We name the target ``nrf52_slinky``.

.. code-block:: console

    $ newt target create nrf52_slinky
    $ newt target set nrf52_slinky bsp=@apache-mynewt-core/hw/bsp/nrf52dk
    $ newt target set nrf52_slinky build_profile=debug
    $ newt target set nrf52_slinky app=@apache-mynewt-core/apps/slinky

Build the Targets
~~~~~~~~~~~~~~~~~

Run the ``newt build nrf52_boot`` command to build the bootloader:

.. code-block:: console

    $ newt build nrf52-boot
    Building target targets/nrf52_boot
    Compiling repos/apache-mynewt-core/boot/bootutil/src/image_ec256.c
    Compiling repos/apache-mynewt-core/boot/bootutil/src/image_ec.c
    Compiling repos/apache-mynewt-core/boot/bootutil/src/image_rsa.c
    Compiling repos/apache-mynewt-core/crypto/mbedtls/src/aes.c
    Compiling repos/apache-mynewt-core/boot/bootutil/src/loader.c
    Compiling repos/apache-mynewt-core/boot/bootutil/src/image_validate.c
    Compiling repos/apache-mynewt-core/boot/bootutil/src/bootutil_misc.c
    Compiling repos/apache-mynewt-core/apps/boot/src/boot.c
        ...

    Archiving sys_mfg.a
    Archiving sys_sysinit.a
    Archiving util_mem.a
    Linking ~/dev/slinky/bin/targets/nrf52_boot/app/apps/boot/boot.elf
    Target successfully built: targets/nrf52_boot

Run the ``newt build nrf52_slinky`` command to build the Slinky
application:

.. code-block:: console

    $newt build nrf52_slinky
    Building target targets/nrf52_slinky
    Compiling repos/apache-mynewt-core/boot/bootutil/src/image_ec256.c
    Compiling repos/apache-mynewt-core/boot/bootutil/src/image_ec.c
    Compiling repos/apache-mynewt-core/boot/bootutil/src/image_rsa.c
    Compiling repos/apache-mynewt-core/boot/split/src/split.c
    Compiling repos/apache-mynewt-core/boot/bootutil/src/loader.c
    Compiling repos/apache-mynewt-core/boot/bootutil/src/bootutil_misc.c
    Compiling repos/apache-mynewt-core/boot/split/src/split_config.c
    Compiling repos/apache-mynewt-core/crypto/mbedtls/src/aesni.c
    Compiling repos/apache-mynewt-core/boot/bootutil/src/image_validate.c
    Compiling repos/apache-mynewt-core/crypto/mbedtls/src/aes.c
    Compiling repos/apache-mynewt-core/apps/slinky/src/main.c

           ...

    Archiving util_mem.a
    Linking ~/dev/slinky/bin/targets/nrf52_slinky/app/apps/slinky/slinky.elf
    Target successfully built: targets/nrf52_slinky

Sign and Create the Slinky Application Image
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Run the ``newt create-image nrf52_slinky 1.0.0`` command to create and
sign the application image. You may assign an arbitrary version (e.g.
1.0.0) to the image.

.. code-block:: console

    $ newt create-image nrf52_slinky 1.0.0
    App image succesfully generated: ~/dev/slinky/bin/targets/nrf52_slinky/app/apps/slinky/slinky.img
    $

Connect to the Board
~~~~~~~~~~~~~~~~~~~~

-  Connect a micro-USB cable from your computer to the micro-USB port on
   the nRF52-DK board.
-  Turn the power on the board to ON. You should see the green LED light
   up on the board.

 ### Load the Bootloader and the Slinky Application Image

Run the ``newt load nrf52_boot`` command to load the bootloader onto the
board:

.. code-block:: console

    $ newt load nrf52_boot
    Loading bootloader
    $

 Run the ``newt load nrf52_slinky`` command to load the Slinky
application image onto the board:

.. code-block:: console

    $ newt load nrf52_slinky
    Loading app image into slot 1
    $

Connect Newtmgr with the Board using a Serial Connection
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set up a serial connection from your computer to the nRF52-DK board (See
`Serial Port Setup </os/get_started/serial_access.html>`__).

Locate the port, in the /dev directory on your computer, that the serial
connection uses. The format of the port name is platform dependent:

-  Mac OS uses the format ``tty.usbserial-<some identifier>``.
-  Linux uses the format ``TTYUSB<N>``, where ``N`` is a number. For
   example, TTYUSB2.
-  MinGW on Windows uses the format ``ttyS<N>``, where ``N`` is a
   number. You must map the port name to a Windows COM port:
   ``/dev/ttyS<N>`` maps to ``COM<N+1>``. For example, ``/dev/ttyS2``
   maps to ``COM3``.

   You can also use the Windows Device Manager to find the COM port
   number.

.. code-block:: console

    $ ls /dev/tty*usbserial*
    /dev/tty.usbserial-1d11
    $

Setup a newtmgr connection profile for the serial port. For our example,
the port is ``/dev/tty.usbserial-1d11``.

Run the ``newtmgr conn add`` command to define a newtmgr connection
profile for the serial port. We name the connection profile
``nrf52serial``.

**Note**:

-  You will need to replace the ``connstring`` with the specific port
   for your serial connection.
-  On Windows, you must specify ``COM<N+1>`` for the connstring if
   ``/dev/ttyS<N>`` is the serial port.

.. code-block:: console

    $ newtmgr conn add nrf52serial type=serial connstring=/dev/tty.usbserial-1d11
    Connection profile nrf52serial successfully added
    $

 You can run the ``newt conn show`` command to see all the newtmgr
connection profiles:

.. code-block:: console

    $ newtmgr conn show
    Connection profiles:
      nrf52serial: type=serial, connstring='/dev/tty.usbserial-1d11'
      sim1: type=serial, connstring='/dev/ttys012'
    $

 ### Use Newtmgr to Query the Board Run some newtmgr commands to query
and receive responses back from the board (See the `Newt Manager
Guide <../../newtmgr/overview>`__ for more information on the newtmgr
commands).

Run the ``newtmgr echo hello -c nrf52serial`` command. This is the
simplest command that requests the board to echo back the text.

.. code-block:: console

    $ newtmgr echo hello -c nrf52serial 
    hello
    $

 Run the ``newtmgr image list -c nrf52serial`` command to list the
images on the board:

.. code-block:: console

    $ newtmgr image list -c nrf52serial 
    Images:
     slot=0
        version: 1.0.0
        bootable: true
        flags: active confirmed
        hash: f411a55d7a5f54eb8880d380bf47521d8c41ed77fd0a7bd5373b0ae87ddabd42
    Split status: N/A
    $

 Run the ``newtmgr taskstat -c nrf52serial`` command to display the task
statistics on the board:

.. code-block:: console

    $ newtmgr taskstat -c nrf52serial
          task pri tid  runtime      csw    stksz   stkuse last_checkin next_checkin
          idle 255   0    43484      539       64       32        0        0
          main 127   1        1       90     1024      353        0        0
         task1   8   2        0      340      192      114        0        0
         task2   9   3        0      340       64       31        0        0
    $
