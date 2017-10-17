Creating Your First Mynewt Project
----------------------------------

This page shows you how to create a Mynewt project using the ``newt``
command-line tool. The project is a blinky application that toggles a
pin. The application uses the Mynewt's simulated hardware and runs as a
native application on Mac OS and Linux.

**Note:** The Mynewt simulator is not yet supported on Windows. If you
are using the native install option (not the Docker option), you will
need to create the blinky application for a target board. We recommend
that you read the section on creating a new project and fetching the
source repository to understand the Mynewt repository structure, create
a new project, and fetch the source dependencies before proceeding to
one of the `Blinky Tutorials </os/tutorials/blinky.html>`__.

This guide shows you how to:

1. Create a new project and fetch the source repository and
   dependencies.
2. Test the project packages. (Not supported on Windows.)
3. Build and run the simulated blinky application. (Not supported on
   Windows.)

Prerequisites
~~~~~~~~~~~~~

-  Have Internet connectivity to fetch remote Mynewt components.
-  Install the newt tool:

   -  If you have taken the native install option, see the installation
      instructions for `Mac OS <../../newt/install/newt_mac.html>`__,
      `Linux <../../newt/install/newt_linux.html>`__, or
      `Windows <../../newt/install/newt_windows.html>`__.
   -  If you have taken the Docker option, you have already installed
      Newt.

-  Install the `native toolchain <native_tools.html>`__ to compile and
   build a Mynewt native application.

Creating a New Project and Fetching the Source Repository
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This section describes how to use the newt tool to create a new project
and fetch the core mynewt source repository.

 ####Creating a New Project

Choose a name for your project. We name the project ``myproj``.

 Run the ``newt new myproj`` command, from your **dev** directory, to
create a new project:

**Note:** This tutorial assumes you created a **dev** directory under
your home directory.

.. code-block:: console

    $cd ~/dev
    $ newt new myproj
    Downloading project skeleton from apache/mynewt-blinky...
    Installing skeleton in myproj...
    Project myproj successfully created.

The newt tool creates a project base directory name **myproj**. All newt
tool commands are run from the project base directory. The newt tool
populates this new project with a base skeleton of a new Apache Mynewt
project in the project base directory. It has the following structure:

**Note**: If you do not have ``tree``, run ``brew install tree`` to
install on Mac OS, ``sudo apt-get install tree`` to install on Linux,
and ``pacman -Su tree`` from a MinGW terminal to install on Windows.

.. code-block:: console

    $ cd myproj
    $ tree 
    .
    ├── DISCLAIMER
    ├── LICENSE
    ├── NOTICE
    ├── README.md
    ├── apps
    │   └── blinky
    │       ├── pkg.yml
    │       └── src
    │           └── main.c
    ├── project.yml
    └── targets
        ├── my_blinky_sim
        │   ├── pkg.yml
        │   └── target.yml
        └── unittest
            ├── pkg.yml
            └── target.yml

    6 directories, 11 files

The newt tool installs the following files for a project in the project
base directory:

1. The file ``project.yml`` contains the repository list that the
   project uses to fetch its packages. Your project is a collection of
   repositories. In this case, the project only comprises the core
   mynewt repository. Later, you will add more repositories to include
   other mynewt components.
2. The file ``apps/blinky/pkg.yml`` contains the description of your
   application and its package dependencies.
3. A ``target`` directory that contains the ``my_blinky_sim`` directory.
   The ``my_blinky_sim`` directory a target information to build a
   version of myproj. Use ``newt target show`` to see available build
   targets.
4. A non-buildable target called ``unittest``. This is used internally
   by ``newt`` and is not a formal build target.

**Note:** The actual code and package files are not installed (except
the template for ``main.c``). See the next step to install the packages.

 #### Fetching the Mynewt Source Repository and Dependencies

By default, Mynewt projects rely on a single repository:
**apache-mynewt-core** and uses the source in the master branch. If you
need to use a different branch, you need to change the ``vers`` value in
the project.yml file:

.. code-block:: console

    repository.apache-mynewt-core:
        type: github
        vers: 1-latest
        user: apache
        repo: mynewt-core

Changing vers to 0-dev will put you on the latest master branch. **This
branch may not be stable and you may encounter bugs or other problems.**

**Note:** On Windows platforms, you will need to change vers to 0-dev
and use the latest master branch. Release 1.0.0 is not supported on
Windows.

 Run the ``newt install`` command, from your project base directory
(myproj), to fetch the source repository and dependencies.

**Note:** It may take a while to download the apache-mynewt-core
reposistory. Use the *-v* (verbose) option to see the installation
progress.

.. code-block:: console

    $ newt install
    apache-mynewt-core

 **Note:** If you get the following error:

::

    ReadDesc: No matching branch for apache-mynewt-core repo
    Error: No matching branch for apache-mynewt-core repop

You must edit the ``project.yml`` file and change the line
``repo: incubator-mynewt-core`` as shown in the following example to
``repo: mynewt-core``:

.. code:: hl_lines="5"

    repository.apache-mynewt-core:
        type: github
        vers: 1-latest
        user: apache
        repo: incubator-mynewt-core

View the core of the Apache Mynewt OS that is downloaded into your local
directory.

(The actual output will depend on what is in the latest 'master' branch)

.. code-block:: console

    $ tree -L 2 repos/apache-mynewt-core/

    repos/apache-mynewt-core/
    ├── CODING_STANDARDS.md
    ├── DISCLAIMER
    ├── LICENSE
    ├── NOTICE
    ├── README.md
    ├── RELEASE_NOTES.md
    ├── apps
    │   ├── blecent
    │   ├── blehci
    │   ├── bleprph
    │   ├── bleprph_oic
    │   ├── blesplit
    │   ├── bletest
    │   ├── bletiny
    │   ├── bleuart
    │   ├── boot
    │   ├── fat2native
    │   ├── ffs2native
    │   ├── ocf_sample
    │   ├── slinky
    │   ├── slinky_oic
    │   ├── spitest
    │   ├── splitty
    │   ├── test
    │   ├── testbench
    │   └── timtest
    ├── boot
    │   ├── boot_serial
    │   ├── bootutil
    │   ├── split
    │   └── split_app
    ├── compiler
    │   ├── arm-none-eabi-m0
    │   ├── arm-none-eabi-m4
    │   ├── gdbmacros
    │   ├── mips
    │   ├── sim
    │   └── sim-mips
    ├── crypto
    │   ├── mbedtls
    │   └── tinycrypt
    ├── docs
    │   └── doxygen.xml
    ├── encoding
    │   ├── base64
    │   ├── cborattr
    │   ├── json
    │   └── tinycbor
    ├── fs
    │   ├── disk
    │   ├── fatfs
    │   ├── fcb
    │   ├── fs
    │   └── nffs
    ├── hw
    │   ├── bsp
    │   ├── cmsis-core
    │   ├── drivers
    │   ├── hal
    │   ├── mcu
    │   └── scripts
    ├── kernel
    │   └── os
    ├── libc
    │   └── baselibc
    ├── mgmt
    │   ├── imgmgr
    │   ├── mgmt
    │   ├── newtmgr
    │   └── oicmgr
    ├── net
    │   ├── ip
    │   ├── nimble
    │   ├── oic
    │   └── wifi
    ├── project.yml
    ├── repository.yml
    ├── sys
    │   ├── config
    │   ├── console
    │   ├── coredump
    │   ├── defs
    │   ├── flash_map
    │   ├── id
    │   ├── log
    │   ├── mfg
    │   ├── reboot
    │   ├── shell
    │   ├── stats
    │   └── sysinit
    ├── targets
    │   └── unittest
    ├── test
    │   ├── crash_test
    │   ├── flash_test
    │   ├── runtest
    │   └── testutil
    ├── time
    │   └── datetime
    └── util
        ├── cbmem
        ├── crc
        └── mem

    94 directories, 9 files

Testing the Project Packages
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Note**: This is not yet supported on Windows.

You can use the newt tool to execute the unit tests in a package. For
example, run the following command to test the ``sys/config`` package in
the ``apache-mynewt-core`` repo:

.. code-block:: console

    $ newt test @apache-mynewt-core/sys/config
    Testing package @apache-mynewt-core/sys/config/test-fcb
    Compiling bootutil_misc.c
    Compiling image_ec.c
    Compiling image_rsa.c
    Compiling image_validate.c

        ...

    Linking ~/dev/myproj/bin/targets/unittest/sys_config_test-fcb/app/sys/config/test-fcb/sys_config_test-fcb.elf
    Executing test: ~/dev/myproj/bin/targets/unittest/sys_config_test-fcb/app/sys/config/test-fcb/sys_config_test-fcb.elf
    Testing package @apache-mynewt-core/sys/config/test-nffs
    Compiling repos/apache-mynewt-core/encoding/base64/src/hex.c
    Compiling repos/apache-mynewt-core/fs/fs/src/fs_cli.c
    Compiling repos/apache-mynewt-core/fs/fs/src/fs_dirent.c
    Compiling repos/apache-mynewt-core/fs/fs/src/fs_mkdir.c
    Compiling repos/apache-mynewt-core/fs/fs/src/fs_mount.c
    Compiling repos/apache-mynewt-core/encoding/base64/src/base64.c
    Compiling repos/apache-mynewt-core/fs/fs/src/fs_file.c
    Compiling repos/apache-mynewt-core/fs/disk/src/disk.c
    Compiling repos/apache-mynewt-core/fs/fs/src/fs_nmgr.c
    Compiling repos/apache-mynewt-core/fs/fs/src/fsutil.c
    Compiling repos/apache-mynewt-core/fs/nffs/src/nffs.c

         ...

    Linking ~/dev/myproj/bin/targets/unittest/sys_config_test-nffs/app/sys/config/test-nffs/sys_config_test-nffs.elf
    Executing test: ~/dev/myproj/bin/targets/unittest/sys_config_test-nffs/app/sys/config/test-nffs/sys_config_test-nffs.elf
    Passed tests: [sys/config/test-fcb sys/config/test-nffs]
    All tests passed

**Note:** If you installed the latest gcc using homebrew on your Mac,
you are probably running gcc-6. Make sure you change the compiler.yml
configuration to specify that you are using gcc-6 (See `Native Install
Option <native_tools.html>`__). You can also downgrade your installation
to gcc-5 and use the default gcc compiler configuration for MyNewt:

.. code-block:: console

    $ brew uninstall gcc-6
    $ brew link gcc-5

**Note:** If you are running the standard gcc for 64-bit machines, it
does not support 32-bit. In that case you will see compilation errors.
You need to install multilib gcc (e.g. gcc-multilib if you running on a
64-bit Ubuntu).

To test all the packages in a project, specify ``all`` instead of the
package name.

.. code-block:: console

    $ newt test all
    Testing package @apache-mynewt-core/boot/boot_serial/test
    Compiling repos/apache-mynewt-core/boot/boot_serial/test/src/boot_test.c
    Compiling repos/apache-mynewt-core/boot/boot_serial/test/src/testcases/boot_serial_setup.c

         ...

    Linking ~/dev/myproj/bin/targets/unittest/boot_boot_serial_test/app/boot/boot_serial/test/boot_boot_serial_test.elf

    ...lots of compiling and testing...

    Linking ~/dev/myproj/bin/targets/unittest/util_cbmem_test/app/util/cbmem/test/util_cbmem_test.elf
    Executing test: ~/dev/myproj/bin/targets/unittest/util_cbmem_test/app/util/cbmem/test/util_cbmem_test.elf
    Passed tests: [boot/boot_serial/test boot/bootutil/test crypto/mbedtls/test encoding/base64/test encoding/cborattr/test encoding/json/test fs/fcb/test fs/nffs/test kernel/os/test net/ip/mn_socket/test net/nimble/host/test net/oic/test sys/config/test-fcb sys/config/test-nffs sys/flash_map/test sys/log/full/test util/cbmem/test]
    All tests passed

Building and Running the Simulated Blinky Application
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The section shows you how to build and run the blinky application to run
on Mynewt's simulated hardware.

**Note**: This is not yet supported on Windows. Refer to the `Blinky
Tutorials </os/tutorials/blinky.html>`__ to create a blinky application
for a target board.

 ####Building the Application To build the simulated blinky application,
run ``newt build my_blinky_sim``:

.. code-block:: console

    $ newt build my_blinky_sim 
    Building target targets/my_blinky_sim
    Compiling repos/apache-mynewt-core/hw/hal/src/hal_common.c
    Compiling repos/apache-mynewt-core/hw/drivers/uart/src/uart.c
    Compiling repos/apache-mynewt-core/hw/hal/src/hal_flash.c
    Compiling repos/apache-mynewt-core/hw/bsp/native/src/hal_bsp.c
    Compiling repos/apache-mynewt-core/hw/drivers/uart/uart_hal/src/uart_hal.c
    Compiling apps/blinky/src/main.c

        ...


    Archiving sys_mfg.a
    Archiving sys_sysinit.a
    Archiving util_mem.a
    Linking ~/dev/myproj/bin/targets/my_blinky_sim/app/apps/blinky/blinky.elf
    Target successfully built: targets/my_blinky_sim

Running the Blinky Application
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

You can run the simulated version of your project and see the simulated
LED blink.

If you natively install the toolchain execute the binary directly:

.. code-block:: console

    $ ./bin/targets/my_blinky_sim/app/apps/blinky/blinky.elf
    hal_gpio set pin  1 to 0

 If you are using newt docker, use ``newt run`` to run the simulated
binary.

.. code-block:: console

    $ newt run my_blinky_sim
    Loading app image into slot 1
        ...
    Debugging ~/dev/myproj/bin/targets/my_blinky_sim/app/apps/blinky/blinky.elf
        ...
    Reading symbols from /bin/targets/my_blinky_sim/app/apps/blinky/blinky.elf...done.
    (gdb)

Type ``r`` at the ``(gdb)`` prompt to run the project. You will see an
output indicating that the hal\_gpio pin is toggling between 1 and 0 in
a simulated blink.

Type ``r`` at the ``(gdb)`` prompt to run the project. You will see an
output indicating that the ``hal_gpio`` pin is toggling between 1 and 0
in a simulated blink.

Exploring other Mynewt OS Features
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Congratulations, you have created your first project! The blinky
application is not terribly exciting when it is run in the simulator, as
there is no LED to blink. Apache Mynewt has a lot more functionality
than just running simulated applications. It provides all the features
you'll need to cross-compile your application, run it on real hardware
and develop a full featured application.

If you're interested in learning more, a good next step is to dig in to
one of the `tutorials <../tutorials/tutorials>`__ and get a Mynewt
project running on real hardware.

Happy Hacking!
