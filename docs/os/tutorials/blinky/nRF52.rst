Blinky, your "Hello World!", on a nRF52 Development Kit
-------------------------------------------------------

This tutorial shows you how to create, build, and run the Blinky
application on a nRF52 Development Kit.

Note that there are several versions of the nRF52 Development Kit in the
market. The boards tested with this tutorial are listed under
"Prerequisites".

Prerequisites
~~~~~~~~~~~~~

-  Meet the prerequisites listed in `Project
   Blinky </os/tutorials/blinky.html>`__.
-  Have a nRF52 Development Kit (one of the following)

   -  Nordic nRF52-DK Development Kit - PCA 10040
   -  Rigado BMD-300 Evaluation Kit - BMD-300-EVAL-ES

-  Install the `Segger JLINK Software and documentation
   pack <https://www.segger.com/jlink-software.html>`__.

This tutorial uses the Nordic nRF52-DK board.

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

Create two targets for the nRF52-DK board - one for the bootloader and
one for the Blinky application.

Run the following ``newt target`` commands, from your project directory,
to create a bootloader target. We name the target ``nrf52_boot``:

 Note: This tutorial uses the Nordic nRF52-DK board. You must specify
the correct bsp for the board you are using.

-  For the Nordic Preview Dev Kit choose
   @apache-mynewt-core/hw/bsp/nrf52pdk (as shown below)
-  For the Nordic Dev Kit choose @apache-mynewt-core/hw/bsp/nrf52dk
   instead (in the highlighted lines)
-  For the Rigado Eval Kit choose @apache-mynewt-core/hw/bsp/bmd300eval
   instead (in the highlighted lines)

.. code:: hl_lines="3"

    $ newt target create nrf52_boot
    $ newt target set nrf52_boot app=@apache-mynewt-core/apps/boot
    $ newt target set nrf52_boot bsp=@apache-mynewt-core/hw/bsp/nrf52pdk
    $ newt target set nrf52_boot build_profile=optimized

 Run the following ``newt target`` commands to create a target for the
Blinky application. We name the target ``nrf52_blinky``.

.. code:: hl_lines="3"

    $ newt target create nrf52_blinky
    $ newt target set nrf52_blinky app=apps/blinky
    $ newt target set nrf52_blinky bsp=@apache-mynewt-core/hw/bsp/nrf52dk
    $ newt target set nrf52_blinky build_profile=debug

 You can run the ``newt target show`` command to verify the target
settings:

.. code-block:: console

    $ newt target show 
    targets/nrf52_blinky
        app=apps/blinky
        bsp=@apache-mynewt-core/hw/bsp/nrf52pdk
        build_profile=debug
    targets/nrf52_boot
        app=@apache-mynewt-core/apps/boot
        bsp=@apache-mynewt-core/hw/bsp/nrf52pdk
        build_profile=optimized

Build the Target Executables
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Run the ``newt build nrf52_boot`` command to build the bootloader:

.. code-block:: console

    $ newt build nrf52_boot
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
    Linking ~/dev/myproj/bin/targets/nrf52_boot/app/apps/boot/boot.elf
    Target successfully built: targets/nrf52_boot

 Run the ``newt build nrf52_blinky`` command to build the Blinky
application:

.. code-block:: console

    $ newt build nrf52_blinky
    Building target targets/nrf52_blinky
    Assembling repos/apache-mynewt-core/hw/bsp/nrf52dk/src/arch/cortex_m4/gcc_startup_nrf52_split.s
    Compiling repos/apache-mynewt-core/hw/bsp/nrf52dk/src/sbrk.c
    Compiling repos/apache-mynewt-core/hw/cmsis-core/src/cmsis_nvic.c
    Compiling repos/apache-mynewt-core/hw/drivers/uart/uart_hal/src/uart_hal.c
    Assembling repos/apache-mynewt-core/hw/bsp/nrf52dk/src/arch/cortex_m4/gcc_startup_nrf52.s
    Compiling apps/blinky/src/main.c

        ...

    Archiving sys_mfg.a
    Archiving sys_sysinit.a
    Archiving util_mem.a
    Linking ~/dev/myproj/bin/targets/nrf52_blinky/app/apps/blinky/blinky.elf
    Target successfully built: targets/nrf52_blinky

Sign and Create the Blinky Application Image
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Run the ``newt create-image nrf52_blinky 1.0.0`` command to create and
sign the application image. You may assign an arbitrary version (e.g.
1.0.0) to the image.

.. code-block:: console

    $ newt create-image nrf52_blinky 1.0.0
    App image succesfully generated: ~/dev/myproj/bin/targets/nrf52_blinky/app/apps/blinky/blinky.img

Connect to the Board
~~~~~~~~~~~~~~~~~~~~

-  Connect a micro-USB cable from your computer to the micro-USB port on
   the nRF52-DK board.
-  Turn the power on the board to ON. You should see the green LED light
   up on the board.

Load the Bootloader and the Blinky Application Image
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Run the ``newt load nrf52_boot`` command to load the bootloader onto the
board:

.. code-block:: console

    $ newt load nrf52_boot
    Loading bootloader
    $

 Run the ``newt load nrf52_blinky`` command to load the Blinky
application image onto the board.

.. code-block:: console

    $ newt load nrf52_blinky
    Loading app image into slot 1

You should see the LED1 on the board blink!

Note: If the LED does not blink, try resetting your board.

If you want to erase the flash and load the image again, you can run
``JLinkExe`` to issue an ``erase`` command.

**Note:** On Windows: Run the ``jlink`` command with the same arguments
from a Windows Command Prompt terminal.

.. code-block:: console

    $ JLinkExe -device nRF52 -speed 4000 -if SWD
    SEGGER J-Link Commander V5.12c (Compiled Apr 21 2016 16:05:51)
    DLL version V5.12c, compiled Apr 21 2016 16:05:45

    Connecting to J-Link via USB...O.K.
    Firmware: J-Link OB-SAM3U128-V2-NordicSemi compiled Mar 15 2016 18:03:17
    Hardware version: V1.00
    S/N: 682863966
    VTref = 3.300V


    Type "connect" to establish a target connection, '?' for help
    J-Link>erase
    Cortex-M4 identified.
    Erasing device (0;?i?)...
    Comparing flash   [100%] Done.
    Erasing flash     [100%] Done.
    Verifying flash   [100%] Done.
    J-Link: Flash download: Total time needed: 0.363s (Prepare: 0.093s, Compare: 0.000s, Erase: 0.262s, Program: 0.000s, Verify: 0.000s, Restore: 0.008s)
    Erasing done.
    J-Link>exit
    $
