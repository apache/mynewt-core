Blinky, your "Hello World!", on Arduino Zero (Pandoc Conversion)
----------------------------------------------------------------

Learn how to use packages from a default application repository of
Mynewt to build your first *Hello World* application (Blinky) on a
target board. Once built using the *newt* tool, this application will
blink the LED lights on the target board.

This tutorial describes how to run Mynewt OS on Arduino Zero. Follow
these simple steps and your board will be blinking in no time!

Prerequisites
~~~~~~~~~~~~~

Before tackling this tutorial, it's best to read about Mynewt in the
:ref:`Introduction <introduction>` section of this
documentation.

Equipment
~~~~~~~~~

You will need the following equipment

-  An Arduino Zero board. NOTE: There are many flavors of Arduino.
   Ensure that you have an Arduino Zero. See below for the versions of
   Arduino Zero that are compatible with this tutorial
-  A computer that can connect to the Arduino Zero over USB
-  A USB cable (Type A to micro B) that can connect the computer to the
   Arduino
-  The Mynewt Release

This tutorial has been tested on the following three Arduino Zero boards
- Zero, M0 Pro, and Zero-Pro.

Mynewt has not been tested on Arduino M0 which has no internal debugger
support.

Install Mynewt and Newt
~~~~~~~~~~~~~~~~~~~~~~~

-  If you have not already done so, install Newt as shown in the `Newt
   install tutorial <../../newt/install/newt_mac.html>`__
-  If you have not already done so, create a project as shown in the
   Quick Start guide on how to `Create Your First
   Project <../get_started/project_create.html>`__. Skip the testing and
   building the project steps in that tutorial since you will be
   defining a target for your Arduino board in this tutorial.

Fetch External Packages
~~~~~~~~~~~~~~~~~~~~~~~

Mynewt uses source code provided directly from the chip manufacturer for
low level operations. Sometimes this code is licensed only for the
specific manufacturer of the chipset and cannot live in the Apache
Mynewt repository. That happens to be the case for the Arduino Zero
board which uses Atmel SAMD21. Runtime's github repository hosts such
external third-party packages and the Newt tool can fetch them.

To fetch the package with MCU support for Atmel SAMD21 for Arduino Zero
from the Runtime git repository, you need to add the repository to the
``project.yml`` file in your base project directory.

Here is an example ``project.yml`` file with the Arduino Zero repository
added. The sections with ``mynewt_arduino_zero`` that need to be added
to your project file are highlighted.

\`\`\`hl\_lines="6 14 15 16 17 18" $ more project.yml project.name:
"my\_project"

project.repositories: - apache-mynewt-core - mynewt\_arduino\_zero

repository.apache-mynewt-core: type: github vers: 0-latest user: apache
repo: incubator-mynewt-core

repository.mynewt\_arduino\_zero: type: github vers: 0-latest user:
runtimeinc repo: mynewt\_arduino\_zero $ \`\`\`

Once you've edited your ``project.yml`` file, the next step is to
install the project dependencies, this can be done with the
``newt install`` command (to see more output, provide the ``-v`` verbose
option.):

.. code-block:: console

    $ newt install
    apache-mynewt-core
    mynewt_arduino_zero
    $

**NOTE:** If there has been a new release of a repo used in your project
since you last installed it, the ``0-latest`` version for the repo in
the ``project.yml`` file will refer to the new release and will not
match the installed files. In that case you will get an error message
saying so and you will need to run ``newt upgrade`` to overwrite the
existing files with the latest codebase.

Create your bootloader target
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Next, you need to tell Newt what to build. For the Arduino Zero, we are
going to generate both a bootloader, and an image target.

To generate the bootloader target, you need to specify the following
options. The output of the commands (indicating success) have been
suppressed for easier readability.

.. code-block:: console

    $ newt target create arduino_boot
    $ newt target set arduino_boot bsp=@mynewt_arduino_zero/hw/bsp/arduino_zero
    $ newt target set arduino_boot app=@apache-mynewt-core/apps/boot
    $ newt target set arduino_boot build_profile=optimized

If you have an Arduino Zero Pro or M0 Pro, you have to set the following
next:

::

    $ newt target set arduino_boot features=arduino_zero_pro

If you have an Arduino Zero, you have to set the following instead:

::

    $ newt target set arduino_boot features=arduino_zero

These commands do a few things:

-  Create a target named ``arduino_boot``, in order to build the Arduino
   Zero Bootloader.
-  Set the application for the ``arduino_boot`` target to the default
   Apache Mynewt bootloader (``@apache-mynewt-core/apps/boot``)
-  Set the board support package for the target to
   ``@mynewt_arduino_zero/hw/bsp/arduino_zero``. This is a reference to
   the downloaded Arduino Zero support from Github.
-  Use the "optimized" build profile for the ``arduino_boot`` target.
   This instructs Newt to generate smaller and more efficient code for
   this target. This setting is necessary due to the bootloader's strict
   size constraints.
-  Tells the Board Support Package to enable support for the Arduino
   Zero Pro or the Arduino Zero. Set it to ``arduino_zero`` or
   ``arduino_zero_pro`` depending on the board you have.

Build your bootloader
~~~~~~~~~~~~~~~~~~~~~

Once you've configured the bootloader target, the next step is to build
the bootloader for your Arduino. You can do this by using the
``newt build`` command:

.. code-block:: console

    $ newt build arduino_boot
    Compiling asprintf.c
    Compiling atoi.c
    Compiling atol.c
    Compiling atoll.c
    Compiling bsearch.c
    Compiling bzero.c
    Compiling calloc.c
    Compiling fgets.c
    Compiling inline.c
    <snip>
    App successfully built: myproject/bin/arduino_boot/apps/boot/boot.elf

If this command finishes successfully, you have successfully built the
Arduino bootloader, and the next step is to build your application for
the Arduino board.

Build your blinky app
~~~~~~~~~~~~~~~~~~~~~

To create and download your application, you create another target, this
one pointing to the application you want to download to the Arduino
board. In this tutorial, we will use the default application that comes
with your project, ``apps/blinky``:

**Note**: Remember to set features to ``arduino_zero`` if your board is
Arduino Zero and not a Pro!

.. code:: hl_lines="9"

    $ newt target create arduino_blinky
    Target targets/arduino_blinky successfully created
    $ newt target set arduino_blinky app=apps/blinky
    Target targets/arduino_blinky successfully set target.app to apps/blinky
    $ newt target set arduino_blinky bsp=@mynewt_arduino_zero/hw/bsp/arduino_zero
    Target targets/arduino_blinky successfully set target.bsp to @mynewt_arduino_zero/hw/bsp/arduino_zero
    $ newt target set arduino_blinky build_profile=debug
    Target targets/arduino_blinky successfully set target.build_profile to debug
    $ newt target set arduino_blinky features=arduino_zero_pro
    Target targets/arduino_blinky successfully set pkg.features to arduino_zero_pro
    $

You can now build the target, with ``newt build``:

.. code-block:: console

    $ newt build arduino_blinky
    Compiling main.c
    Archiving blinky.a
    Compiling cons_fmt.c
    Compiling cons_tty.c
    Archiving full.a
    Compiling case.c
    Compiling suite.c
    Compiling testutil.c
    Archiving testutil.a
    <snip>
    App successfully built: myproject/bin/arduino_blinky/apps/blinky/blinky.elf

 Congratulations! You have successfully built your application. Now it's
time to load both the bootloader and application onto the target.

Connect the Target
~~~~~~~~~~~~~~~~~~

Connect your computer to the Arduino Zero (from now on we'll call this
the target) with the Micro-USB cable through the Programming Port as
shown below. Mynewt will download and debug the target through this
port. You should see a little green LED come on. That means the board
has power.

No external debugger is required. The Arduino Zero comes with an
internal debugger that can be accessed by Mynewt.

A image below shows the Arduino Zero Programming Port.

Download the Bootloader
~~~~~~~~~~~~~~~~~~~~~~~

Execute the command to download the bootloader.

.. code:: c

        $ newt load arduino_boot

If the newt tool finishes without error, that means the bootloader has
been successfully loaded onto the target.

 Reminder if you are using Docker: When working with actual hardware,
remember that each board has an ID. If you swap boards and do not
refresh the USB Device Filter on the VirtualBox UI, the ID might be
stale and the Docker instance may not be able to see the board
correctly. For example, you may see an error message like
``Error: unable to find CMSIS-DAP device`` when you try to load or run
an image on the board. In that case, you need to click on the USB link
in VirtualBox UI, remove the existing USB Device Filter (e.g. "Atmel
Corp. EDBG CMSIS-DAP[0101]") by clicking on the "Removes selected USB
filter" button, and add a new filter by clicking on the "Adds new USB
filter" button.

Run the Image
~~~~~~~~~~~~~

Now that the bootloader is downloaded to the target, the next step is to
load your image onto the Arduino Zero. The easiest way to do this, is to
use the ``newt run`` command. ``newt run`` will automatically rebuild
your program (if necessary), create an image, and load it onto the
target device.

Here, we will load our ``arduino_blinky`` target onto the device, and we
should see it run:

.. code-block:: console

    $ newt run arduino_blinky 0.0.0
    Debugging myproject/bin/arduino_blinky/apps/blinky/blinky.elf
    Open On-Chip Debugger 0.9.0 (2015-09-23-21:46)
    Licensed under GNU GPL v2
    For bug reports, read
        http://openocd.org/doc/doxygen/bugs.html
    Info : only one transport option; autoselect 'swd'
    adapter speed: 500 kHz
    adapter_nsrst_delay: 100
    cortex_m reset_config sysresetreq
    Info : CMSIS-DAP: SWD  Supported
    Info : CMSIS-DAP: JTAG Supported
    Info : CMSIS-DAP: Interface Initialised (SWD)
    Info : CMSIS-DAP: FW Version = 01.1F.0118
    Info : SWCLK/TCK = 1 SWDIO/TMS = 1 TDI = 1 TDO = 1 nTRST = 0 nRESET = 1
    Info : CMSIS-DAP: Interface ready
    Info : clock speed 500 kHz
    Info : SWD IDCODE 0x0bc11477
    Info : at91samd21g18.cpu: hardware has 4 breakpoints, 2 watchpoints
    GNU gdb (GNU Tools for ARM Embedded Processors) 7.8.0.20150604-cvs
    Copyright (C) 2014 Free Software Foundation, Inc.
    License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
    This is free software: you are free to change and redistribute it.
    There is NO WARRANTY, to the extent permitted by law.  Type "show copying"
    and "show warranty" for details.
    This GDB was configured as "--host=x86_64-apple-darwin10 --target=arm-none-eabi".
    Type "show configuration" for configuration details.
    For bug reporting instructions, please see:
    <http://www.gnu.org/software/gdb/bugs/>.
    Find the GDB manual and other documentation resources online at:
    <http://www.gnu.org/software/gdb/documentation/>.
    For help, type "help".
    Type "apropos word" to search for commands related to "word"...
    Reading symbols from myproject/bin/arduino_blinky/apps/blinky/blinky.elf...done.
    target state: halted
    target halted due to debug-request, current mode: Thread
    xPSR: 0x21000000 pc: 0x0000030e msp: 0x20008000
    Info : accepting 'gdb' connection on tcp/3333
    Info : SAMD MCU: SAMD21G18A (256KB Flash, 32KB RAM)
    0x0000030e in ?? ()
    (gdb) r
    The "remote" target does not support "run".  Try "help target" or "continue".
    (gdb) c
    Continuing.

**NOTE:** The 0.0.0 specified after the target name to ``newt run`` is
the version of the image to load. If you are not providing remote
upgrade, and are just developing locally, you can provide 0.0.0 for
every image version.

If you want the image to run without the debugger connected, simply quit
the debugger and restart the board. The image you programmed will come
and run on the Arduino on next boot!

Watch the LED blink
~~~~~~~~~~~~~~~~~~~

Congratulations! You have created a Mynewt operating system running on
the Arduino Zero. The LED right next to the power LED should be
blinking. It is toggled by one task running on the Mynewt OS.

We have more fun tutorials for you to get your hands dirty. Be bold and
try other Blinky-like `tutorials <../tutorials/nRF52.html>`__ or try
enabling additional functionality such as `remote
comms <project-target-slinky.html>`__ on the current board.

If you see anything missing or want to send us feedback, please do so by
signing up for appropriate mailing lists on our `Community
Page <../../community.html>`__.

Keep on hacking and blinking!
