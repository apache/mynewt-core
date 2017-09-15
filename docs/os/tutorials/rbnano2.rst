Blinky, your "Hello World!", on RedBear Nano 2
----------------------------------------------

This tutorial shows you how to create, build and run the Blinky
application on a RedBear Nano 2 board.

Prerequisites
~~~~~~~~~~~~~

-  Meet the prerequisites listed in `Project
   Blinky </os/tutorials/blinky.html>`__.
-  Have a RedBear Nano 2 board.
-  Install a patched version of OpenOCD 0.10.0 described in `Install
   OpenOCD </os/get_started/cross_tools/>`__.

Create a Project
~~~~~~~~~~~~~~~~

Create a new project if you do not have an existing one. You can skip
this step and proceed to `create the targets <#create_targets>`__ if you
already have a project created.

Run the following commands to create a new project:

.. code-block:: console

        $ mkdir ~/dev
        $ cd ~/dev
        $ newt new myproj
        Downloading project skeleton from apache/mynewt-blinky...
        Installing skeleton in myproj...
        Project myproj successfully created.
        $ cd myproj
        $ newt install
        apache-mynewt-core
        $

Create the Targets
~~~~~~~~~~~~~~~~~~

Create two targets for the RedBear Nano 2 board - one for the bootloader
and one for the Blinky application.

Run the following ``newt target`` commands, from your project directory,
to create a bootloader target. We name the target ``rbnano2_boot``:

.. code-block:: console

    $ newt target create rbnano2_boot
    $ newt target set rbnano2_boot app=@apache-mynewt-core/apps/boot
    $ newt target set rbnano2_boot bsp=@apache-mynewt-core/hw/bsp/rb-nano2
    $ newt target set rbnano2_boot build_profile=optimized

 Run the following ``newt target`` commands to create a target for the
Blinky application. We name the target ``nrf52_blinky``.

.. code-block:: console

    $ newt target create rbnano2_blinky
    $ newt target set rbnano2_blinky app=apps/blinky
    $ newt target set rbnano2_blinky bsp=@apache-mynewt-core/hw/bsp/rb-nano2
    $ newt target set rbnano2_blinky build_profile=debug

 You can run the ``newt target show`` command to verify the target
settings:

.. code-block:: console

    $ newt target show 
    targets/rbnano2_blinky
        app=apps/blinky
        bsp=@apache-mynewt-core/hw/bsp/rb-nano2
        build_profile=debug
    targets/rbnano2_boot
        app=@apache-mynewt-core/apps/boot
        bsp=@apache-mynewt-core/hw/bsp/rb-nano2
        build_profile=optimized

Build the Target Executables
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Run the ``newt build rbnano2_boot`` command to build the bootloader:

.. code-block:: console

    $newt build rbnano2_boot
    Building target targets/rbnano2_boot
    Compiling repos/apache-mynewt-core/boot/bootutil/src/image_rsa.c
    Compiling repos/apache-mynewt-core/boot/bootutil/src/image_ec256.c
    Compiling repos/apache-mynewt-core/boot/bootutil/src/loader.c
    Compiling repos/apache-mynewt-core/crypto/mbedtls/src/aes.c
    Compiling repos/apache-mynewt-core/boot/bootutil/src/image_validate.c
    Compiling repos/apache-mynewt-core/boot/bootutil/src/image_ec.c
    Compiling repos/apache-mynewt-core/boot/bootutil/src/bootutil_misc.c
    Compiling repos/apache-mynewt-core/apps/boot/src/boot.c

          ...

    Archiving sys_sysinit.a
    Archiving util_mem.a
    Linking ~/dev/myproj/bin/targets/rbnano2_boot/app/apps/boot/boot.elf
    Target successfully built: targets/rbnano2_boot

 Run the ``newt build rbnano2_blinky`` command to build the Blinky
application:

.. code-block:: console

    $newt build rbnano2_blinky
    Building target targets/rbnano2_blinky
    Assembling repos/apache-mynewt-core/hw/bsp/rb-nano2/src/arch/cortex_m4/gcc_startup_nrf52_split.s
    Compiling repos/apache-mynewt-core/hw/drivers/uart/src/uart.c
    Compiling repos/apache-mynewt-core/hw/cmsis-core/src/cmsis_nvic.c
    Compiling repos/apache-mynewt-core/hw/bsp/rb-nano2/src/sbrk.c
    Compiling apps/blinky/src/main.c

         ...

    Archiving sys_sysinit.a
    Archiving util_mem.a
    Linking ~/dev/myproj/bin/targets/rbnano2_blinky/app/apps/blinky/blinky.elf
    Target successfully built: targets/rbnano2_blinky

Sign and Create the Blinky Application Image
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Run the ``newt create-image rbnano2_blinky 1.0.0`` command to create and
sign the application image. You may assign an arbitrary version (e.g.
1.0.0) to the image.

.. code-block:: console

    $newt create-image rbnano2_blinky 1.0.0
    App image succesfully generated: ~/dev/myproj/bin/targets/rbnano2_blinky/app/apps/blinky/blinky.img

Connect to the Board
~~~~~~~~~~~~~~~~~~~~

Connect the RedBear Nano 2 USB to a USB port on your computer. You
should see an orange LED light up on the board.

Load the Bootloader
~~~~~~~~~~~~~~~~~~~

Run the ``newt load rbnano2_boot`` command to load the bootloader onto
the board:

.. code-block:: console

    $ newt load rbnano2_boot
    Loading bootloader
    $

**Note:** On Windows platforms, if you get an
``unable to find CMSIS-DAP device`` error, you will need to download and
install the mbed Windows serial port driver from
https://developer.mbed.org/handbook/Windows-serial-configuration. Follow
the instructions from the site to install the driver. Here are some
additional notes about the installation:

1. The instructions indicate that the mbed Windows serial port driver is
   not required for Windows 10. If you are using Windows 10 and get the
   ``unable to find CMSIS-DAP device`` error, we recommend that you
   install the driver.
2. If the driver installation fails, we recommend that you unplug the
   board, plug it back in, and retry the installation.

Run the ``newt load rbnano2_boot`` command again.

 ####Clear the Write Protection on the Flash Memory The flash memory on
the RedBear Nano 2 comes write protected from the factory. If you get an
error loading the bootloader and you are using a brand new chip, you
need to clear the write protection from the debugger and then load the
bootloader again. Run the ``newt debug rbnano2_blinky`` command and
issue the following commands at the highlighted (gdb) prompts.

**Note:** The output of the debug session below is for Mac OS and Linux
platforms. On Windows, openocd and gdb are started in separate Windows
Command Prompt terminals, and the terminals are automatically closed
when you quit gdb. In addition, the output of openocd is logged to the
openocd.log file in your project's base directory instead of the
terminal.

\`\`\`hl\_lines="8 9 11 14" $newt debug rbnano2\_blinky
[~/dev/myproj/repos/apache-mynewt-core/hw/bsp/rb-nano2/rb-nano2\_debug.sh
~/dev/myproj/repos/apache-mynewt-core/hw/bsp/rb-nano2
~/dev/myproj/bin/targets/rbnano2\_blinky/app/apps/blinky/blinky] Open
On-Chip Debugger 0.10.0-dev-snapshot (2017-03-28-11:24) Licensed under
GNU GPL v2

::

     ...

(gdb) set {unsigned long}0x4001e504=2 (gdb) x/1wx 0x4001e504
0x4001e504:0x00000002 (gdb) set {unsigned long}0x4001e50c=1 Info : SWD
DPIDR 0x2ba01477 Error: Failed to read memory at 0x00009ef4 (gdb) x/32wx
0x00 0x0:0xffffffff0xffffffff0xffffffff0xffffffff
0x10:0xffffffff0xffffffff0xffffffff0xffffffff
0x20:0xffffffff0xffffffff0xffffffff0xffffffff
0x30:0xffffffff0xffffffff0xffffffff0xffffffff
0x40:0xffffffff0xffffffff0xffffffff0xffffffff
0x50:0xffffffff0xffffffff0xffffffff0xffffffff
0x60:0xffffffff0xffffffff0xffffffff0xffffffff
0x70:0xffffffff0xffffffff0xffffffff0xffffffff (gdb)
``<br> ### Load the Blinky Application Image <br> Run the `newt load rbnano2_blinky` command to load the Blinky application image onto the board:``\ no-highlight
$ newt load rbnano2\_blinky Loading app image into slot 1 \`\`\`

You should see a blue LED on the board blink!

Note: If the LED does not blink, try resetting your board.
