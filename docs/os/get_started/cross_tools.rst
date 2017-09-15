Installing the Cross Tools for ARM
==================================

This page shows you how to install the tools to build, run, and debug
Mynewt OS applications that run on supported ARM target boards. It shows
you how to install the following tools on Mac OS, Linux and Windows:

-  ARM cross toolchain to compile and build Mynewt applications for the
   target boards.
-  Debuggers to load and debug applications on the target boards.

Installing the ARM Cross Toolchain
----------------------------------

ARM maintains a pre-built GNU toolchain with gcc and gdb targeted at
Embedded ARM Processors, namely Cortex-R/Cortex-M processor families.
Mynewt OS has been tested with version 4.9 of the toolchain and we
recommend you install this version to get started. Mynewt OS will
eventually work with multiple versions available, including the latest
releases.

Installing the ARM Toolchain For Mac OS X
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Add the **PX4/homebrew-px4** homebrew tap and install version 4.9 of the
toolchain. After installing, check that the symbolic link that homebrew
created points to the correct version of the debugger.

.. code-block:: console

    $ brew tap PX4/homebrew-px4
    $ brew update
    $ brew install gcc-arm-none-eabi-49
    $ arm-none-eabi-gcc --version  
    arm-none-eabi-gcc (GNU Tools for ARM Embedded Processors) 4.9.3 20150529 (release) [ARM/embedded-4_9-branch revision 224288]
    Copyright (C) 2014 Free Software Foundation, Inc.
    This is free software; see the source for copying conditions.  There is NO
    warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
    $ ls -al /usr/local/bin/arm-none-eabi-gdb
    lrwxr-xr-x  1 aditihilbert  admin  69 Sep 22 17:16 /usr/local/bin/arm-none-eabi-gdb -> /usr/local/Cellar/gcc-arm-none-eabi-49/20150609/bin/arm-none-eabi-gdb

**Note:** If no version is specified, brew will install the latest
version available.

 ### Installing the ARM Toolchain For Linux

On a Debian-based Linux distribution, gcc 4.9.3 for ARM can be installed
with apt-get as documented below. The steps are explained in depth at
https://launchpad.net/~team-gcc-arm-embedded/+archive/ubuntu/ppa.

.. code-block:: console

    $ sudo apt-get remove binutils-arm-none-eabi gcc-arm-none-eabi 
    $ sudo add-apt-repository ppa:team-gcc-arm-embedded/ppa
    $ sudo apt-get update 
    $ sudo apt-get install gcc-arm-none-eabi
    $ sudo apt-get install gdb-arm-none-eabi

 ### Installing the ARM Toolchain for Windows Step 1: Download and run
the
`installer <https://launchpad.net/gcc-arm-embedded/4.9/4.9-2015-q2-update/+download/gcc-arm-none-eabi-4_9-2015q2-20150609-win32.exe>`__
to install arm-none-eabi-gcc and arm-none-eabi-gdb. Select the default
destination folder: **C::raw-latex:`\Program `Files
(x86):raw-latex:`\GNU `Tools ARM Embedded:raw-latex:`\4`.9 2015q2**.

**Note:** You may select a different folder but the installation
instructions use the default values.

Step 2: Add the path:\*\* C::raw-latex:`\Program `Files
(x86):raw-latex:`\GNU `Tools ARM Embedded:raw-latex:`\4`.9
2015q2:raw-latex:`\bin*`\* to your Windows **Path** environment
variable. Note: You must add **bin** to the path.

Step 3: Check that you are using the installed versions
arm-none-eabi-gcc and arm-none-eabi-gdb. Open a MinGW terminal and run
the ``which`` commands.

**Note:** You must start a new MinGW terminal to inherit the new
**Path** values.

.. code-block:: console

    $ which arm-none-eabi-gcc
    /c/Program Files (x86)/GNU Tools ARM Embedded/4.9 2015q2/bin/arm-none-eabi-gcc
    $which arm-none-eabi-gdb
    /c/Program Files (x86)/GNU Tools ARM Embedded/4.9 2015q2/bin/arm-none-eabi-gdb

Installing the Debuggers
------------------------

Mynewt uses, depending on the board, either the OpenOCD or SEGGER J-Link
debuggers. ### Installing the OpenOCD Debugger OpenOCD (Open On-Chip
Debugger) is open-source software that allows your computer to interface
with the JTAG debug connector on a variety of boards. A JTAG connection
lets you debug and test embedded target devices. For more on OpenOCD go
to http://openocd.org.

OpenOCD version 0.10.0 with nrf52 support is required. A binary for this
version is available to download for Mac OS, Linux, and Windows.

 #### Installing OpenOCD on Mac OS Step 1: Download the `binary tarball
for Mac
OS <https://github.com/runtimeco/openocd-binaries/raw/master/openocd-bin-0.10.0-MacOS.tgz>`__.

Step 2: Change to the root directory:

.. code-block:: console

    $cd / 

 Step 3: Untar the tarball and install into \*\* /usr/local/bin**. You
will need to replace ** ~/Downloads \*\* with the directory that the
tarball is downloaded to.

.. code-block:: console

    sudo tar -xf ~/Downloads/openocd-bin-0.10.0-MacOS.tgz ` 

 Step 4: Check the OpenOCD version you are using.

.. code-block:: console

    $which openocd
    /usr/local/bin/openocd
    $openocd -v
    Open On-Chip Debugger 0.10.0
    Licensed under GNU GPL v2
    For bug reports, read
    http://openocd.org/doc/doxygen/bugs.html

You should see version: **0.10.0**.

If you see one of these errors:

-  Library not loaded: /usr/local/lib/libusb-0.1.4.dylib - Run
   ``brew install libusb-compat``.
-  Library not loaded: /usr/local/opt/libftdi/lib/libftdi1.2.dylib - Run
   ``brew install libftdi``.
-  Library not loaded: /usr/local/lib/libhidapi.0.dylib - Run
   ``brew install hidapi``.

 #### Installing OpenOCD on Linux Step 1: Download the `binary tarball
for
Linux <https://github.com/runtimeco/openocd-binaries/raw/master/openocd-bin-0.10.0-Linux.tgz>`__

Step 2: Change to the root directory:

::

    $cd / 

 Step 3: Untar the tarball and install into \*\* /usr/local/bin**. You
will need to replace ** ~/Downloads \*\* with the directory that the
tarball is downloaded to.

\*\* Note:\*\* You must specify the -p option for the tar command.

.. code-block:: console

    $sudo tar -xpf ~/Downloads/openocd-bin-0.10.0-Linux.tgz

 Step 4: Check the OpenOCD version you are using:

.. code-block:: console

    $which openocd
    /usr/local/bin/openocd
    $openocd -v
    Open On-Chip Debugger 0.10.0
    Licensed under GNU GPL v2
    For bug reports, read
    http://openocd.org/doc/doxygen/bugs.html

You should see version: **0.10.0**.

If you see any of these error messages:

-  openocd: error while loading shared libraries: libhidapi-hidraw.so.0:
   cannot open shared object file: No such file or directory

-  openocd: error while loading shared libraries: libusb-1.0.so.0:
   cannot open shared object file: No such file or directory

run the following command to install the libraries:

.. code-block:: console

    $sudo apt-get install libhidapi-dev:i386

 #### Installing OpenOCD on Windows Step 1: Download the `binary zip
file for
Windows <https://github.com/runtimeco/openocd-binaries/raw/master/openocd-0.10.0.zip>`__.

Step 2: Extract into the **C::raw-latex:`\openocd`-0.10.0** folder.

Step 3: Add the path: \*\*
C::raw-latex:`\openocd`-0.10.0:raw-latex:`\bin*`\* to your Windows User
**Path** environment variable. Note: You must add **bin** to the path.

Step 4: Check the OpenOCD version you are using. Open a new MinGW
terminal and run the following commands:

**Note:** You must start a new MinGW terminal to inherit the new
**Path** values.

.. code-block:: console

    $which openocd
    /c/openocd-0.10.0/bin/openocd
    $openocd -v
    Open On-Chip Debugger 0.10.0
    Licensed under GNU GPL v2
    For bug reports, read
            http://openocd.org/doc/doxygen/bugs.html

You should see version: **0.10.0**.

 ###Installing SEGGER J-Link You can download and install Segger J-LINK
Software and documentation pack from
`SEGGER <https://www.segger.com/jlink-software.html>`__.

**Note:** On Windows, perform the following after the installation:

-  Add the installation destination folder path to your Windows user
   **Path** environment variable. You do not need to add **bin** to the
   path.
-  Open a new MinGW terminal to inherit the new **Path** values.
