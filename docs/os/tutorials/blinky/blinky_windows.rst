Project Blinky on a Windows Machine
-----------------------------------

Getting your Windows machine ready for simulated target
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``newt`` tool is the build software used to build Mynewt OS images
or executables for any embedded hardware device/board, including the one
for the current tutorial (STM32-E407 development board from Olimex). You
can run the ``newt`` tool natively on a computer running any of the
three Operating System machines - OSX, Linux, or Windows.

However, Mynewt OS images for a simulated target are built on the
Windows machine by using Linux versions of the build software (newt)in a
virtual machine on your Windows box. The Linux VM is set up by
installing the Docker Toolbox. Your Windows machine will communicate
with the Linux VM via transient ssh connections. You will then download
a Docker image (``newtvm.exe``)that allows you to run the newt commands
in the Linux Docker instance. The Docker image contains:

-  The newt command-line tool
-  Go
-  A multilib-capable native gcc / glibc
-  An arm-none-eabi gcc
-  Native gdb

The sequence of events when using the Docker image is as follows:

1. A new docker environment is created in the Linux VM.
2. The specified command with the newtvm prefix (``newtvm newt``
   command) is sent to the docker environment via ssh.
3. The Linux command runs.
4. The output from the command is sent back to Windows via ssh.
5. The output is displayed in the Windows command prompt.

Install Linux virtual machine
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

-  Download the Docker Toolbox for Windows (version 1.9.0c or later)
   from https://www.docker.com/docker-toolbox. The Docker toolbox
   creates a consistently reproducible and self-contained environment in
   Linux.

-  Run the Docker Toolbox installer. All the default settings are OK.

-  You may need to add
   "C::raw-latex:`\Program `Files:raw-latex:`\Git`:raw-latex:`\usr`:raw-latex:`\bin`"
   to your PATH environment variable. To add to the PATH environment
   variable, right-click on the Start button in the bottom left corner.
   Choose System -> Advanced system settings -> Environment Variables.
   Click on the PATH variable under "System variables" and click Edit to
   check and add it if it is not already there.

Install newtvm tool
^^^^^^^^^^^^^^^^^^^

-  From your base user (home) directory, pull or clone the latest code
   from the newt repository into the ``newt`` directory. It includes the
   executable ``newtvm.exe`` for the newtvm tool in the ``newtvm``
   directory.

   .. code:: c

         C:\Users\admin> git clone https://git-wip-us.apache.org/repos/asf/incubator-mynewt-newt newt

   The newtvm tool is what allows you to run programs in the Linux
   docker instance.

-  Run the Docker Quickstart Terminal application inside the Docker
   folder under Programs. You can find it by clicking Start button ->
   All apps. By default, the Docker Toolbox installer creates a shortcut
   to this program on your desktop. Wait until you see an ASCII art
   whale displayed in the terminal window and the Docker prompt given.

.. code:: c

                              ##         .
                        ## ## ##        ==
                     ## ## ## ## ##    ===
                 /"""""""""""""""""\___/ ===
            ~~~ {~~ ~~~~ ~~~ ~~~~ ~~~ ~ /  ===- ~~~
               \______ o           __/
                 \    \         __/
                  \____\_______/
                  
             docker is configured to use the default machine with IP 192.168.99.100
             For help getting started, check out the docs at https://docs.docker.com
             
             admin@dev1 MINGW64 ~ (master)
             $

The first time you run this, it may take several minutes to complete.
You will need to run the Docker Quickstart Terminal once each time you
restart your computer.

-  Open a command prompt (e.g., Windows-R, "cmd", enter). You execute
   the newt tool commands as though you were running newt in Linux, but
   you prefix each command with "newtvm". For example:

   .. code:: c

           C:\Users\admin\newt\newtvm> newtvm newt help

The newtvm tool will take a long time to run the first time you execute
it. The delay is due to the fact that the tool must download the mynewt
docker instance.

-  You are now ready to proceed to `building the image for the simulated
   target <#building-test-code-on-simulator>`__.

Getting your Windows machine ready for hardware target
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When you want to produce images for actual hardware board on your
Windows machine, go through the following setup procedure and then
proceed to the `blinky project on the Olimex
board <#Using-SRAM-to-make-LED-blink>`__ with this method.

Installing some prerequisites
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

-  You have to install the following if you do not have them already.
   The steps below indicate specific folders where each of these
   programs should be installed. You can choose different locations, but
   the remainder of this tutorial for a Windows machine assumes the
   specified folders.

   -  win-builds-i686
   -  win-builds-x86\_64
   -  MSYS
   -  gcc for ARM
   -  openocd
   -  zadig
   -  git
   -  go

      -  *win-builds (mingw64) 1.5 for i686*

      Download from
      http://win-builds.org/doku.php/download_and_installation_from_windows.
      Install at: "C::raw-latex:`\win`-builds-i686".

      Be sure to click the i686 option (not x86\_64). The defaults for
      all other options are OK. The installer will want to download a
      bunch of additional packages. They are not all necessary, but it
      is simplest to just accept the defaults.

      -  *win-builds (mingw64) 1.5 for x86\_64*

      Download from
      http://win-builds.org/doku.php/download_and_installation_from_windows.
      Install at "C::raw-latex:`\win`-builds-x86\_64"

      Run the installer a second time, but this time click the x86\_64
      option, NOT i686. The defaults for all other options are OK.

      -  *MSYS*

      Start your download from
      http://sourceforge.net/projects/mingw-w64/files/External%20binary%20packages%20%28Win64%20hosted%29/MSYS%20%2832-bit%29/MSYS-20111123.zip

      Unzip to "C::raw-latex:`\msys`"

      -  *gcc for ARM, 4.9.3*

      Download the Windows installer from
      https://launchpad.net/gcc-arm-embedded/+download and install at
      "C::raw-latex:`\Program `Files (x86):raw-latex:`\GNU `Tools ARM
      Embedded:raw-latex:`\4`.9 2015q3".

      -  OpenOCD 0.8.0

      Download OpenOCD 0.8.0 from
      http://www.freddiechopin.info/en/download/category/4-openocd.
      Unzip to "C::raw-latex:`\openocd`".

      -  Zadig 2.1.2

      Download it from http://zadig.akeo.ie and install it at
      "C::raw-latex:`\zadig`".

      -  Git

      Click on https://git-scm.com/download/win to start the download.
      Install at "C::raw-latex:`\Program `Files (x86):raw-latex:`\Git`".
      Specify the "Use Git from the Windows Command Prompt" option. The
      defaults for all other options are OK.

      -  Go

      Download the release for Microsoft Windows from
      https://golang.org/dl/ and install it "C::raw-latex:`\Go`".

Creating local repository
^^^^^^^^^^^^^^^^^^^^^^^^^

-  The directory structure must be first readied for using Go. Go code
   must be kept inside a workspace. A workspace is a directory hierarchy
   with three directories at its root:

   -  src contains Go source files organized into packages (one package
      per directory),

   -  pkg contains package objects, and

   -  bin contains executable commands.

   The GOPATH environment variable specifies the location of your
   workspace. First create a 'dev' directory and then a 'go' directory
   under it. Set the GOPATH environment variable to this directory and
   then proceed to create the directory for cloning the newt tool
   repository.

   .. code:: c

           $ cd c:\
           $ mkdir dev\go
           $ cd dev\go

-  Set the following user environment variables using the steps outlined
   here.

   .. code:: c

       * GOPATH: C:\dev\go
       * PATH: C:\Program Files (x86)\GNU Tools ARM Embedded\4.9 2015q3\bin;%GOPATH%\bin;C:\win-builds-x86_64\bin;C:\win-builds-i686\bin;C:\msys\bin

   Steps:

1. Right-click the start button
2. Click "Control panel"
3. Click "System and Security"
4. Click "System"
5. Click "Advanced system settings" in the left panel
6. Click the "Envoronment Variables..." button
7. There will be two sets of environment variables: user variables in
   the upper half of the screen, and system variables in the lower half.
   Configuring the user variables is recommended and tested (though
   system variables will work as well).

-  Next, install godep. Note that the following command produces no
   output.

   .. code:: c

           $ go get github.com/tools/godep 

-  Set up the repository for the package building tool "newt" on your
   local machine. First create the appropriate directory for it and then
   clone the newt tool repository from the online apache repository (or
   its github.com mirror) into this newly created directory. Check the
   contents of the directory.

   .. code:: c

           $ go get git-wip-us.apache.org/repos/asf/incubator-mynewt-newt.git/newt
           $ dir 
            bin    pkg    src
           $ dir src
           git-wip-us.apache.org   github.com      gopkg.in
           $ dir
           newt
           $ cd newt
           $ dir
           Godeps                  README.md               coding_style.txt        newt.go
           LICENSE                 cli                     design.txt

-  Check that newt is in place.

   .. code:: c

           $ dir $GOPATH\src\git-wip-us.apache.org\repos\asf\incubator-mynewt-newt.git\newt 
           Godeps          README.md       coding_style.txt    newt.go
           LICENSE         cli             design.txt

Building the newt tool
^^^^^^^^^^^^^^^^^^^^^^

-  You will use Go to run the newt.go program to build the newt tool.
   The command used is ``go install`` which compiles and writes the
   resulting executable to an output file named ``newt``. It installs
   the results along with its dependencies in $GOPATH/bin.

   .. code:: c

           $ go install
           $ ls "$GOPATH"/bin/
           godep       incubator-mynewt-newt.git     newt

-  Try running newt using the compiled binary. For example, check for
   the version number by typing 'newt version'. See all the possible
   commands available to a user of newt by typing 'newt -h'.

Note: If you are going to be be modifying the newt tool itself often and
wish to compile the program every time you call it, you may want to
define the newt environment variable that allows you to execute the
command via ``%newt%``. Use
``set newt=go run %GOPATH%\src\github.com\mynewt\newt\newt.go`` or set
it from the GUI. Here, you use ``go run`` which runs the compiled binary
directly without producing an executable.

.. code:: c

            $ newt version
            Newt version:  1.0
            $ newt -h
            Newt allows you to create your own embedded project based on the Mynewt
            operating system. Newt provides both build and package management in a
            single tool, which allows you to compose an embedded workspace, and set
            of projects, and then build the necessary artifacts from those projects.
            For more information on the Mynewt operating system, please visit
            https://www.github.com/mynewt/documentation.

            Please use the newt help command, and specify the name of the command
            you want help for, for help on how to use a specific command

            Usage:
             newt [flags]
             newt [command]

            Examples:
             newt
             newt help [<command-name>]
               For help on <command-name>.  If not specified, print this message.


            Available Commands:
             version     Display the Newt version number.
             target      Set and view target information
             egg         Commands to list and inspect eggs on a nest
             nest        Commands to manage nests & clutches (remote egg repositories)
             help        Help about any command

            Flags:
             -h, --help=false: help for newt
             -l, --loglevel="WARN": Log level, defaults to WARN.
             -q, --quiet=false: Be quiet; only display error output.
             -s, --silent=false: Be silent; don't output anything.
             -v, --verbose=false: Enable verbose output when executing commands.


            Use "newt help [command]" for more information about a command.

-  Without creating a project repository you can't do a whole lot with
   the Newt tool. So you'll have to wait till you have downloaded a nest
   to try out the tool.

Getting the debugger ready
^^^^^^^^^^^^^^^^^^^^^^^^^^

-  Use Zadig to configure the USB driver for your Olimex debugger. If
   your debugger is already set up, you can skip this step.

1. Plug in your Olimex debugger.
2. Start Zadig.
3. Check the Options -> List All Devices checkbox.
4. Select "Olimex OpenOCD JTAG ARM-USB-TINY-H" in the dropdown menu.
5. Select the "WinUSB" driver.
6. Click the "Install Driver" button.

-  Proceed to the section on how to `make an LED
   blink <#using-sram-to-make-led-blink>`__ section.
