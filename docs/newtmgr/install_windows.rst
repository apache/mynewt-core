Installing Newtmgr on Windows
-----------------------------

This guide shows you how to install the latest release of newtmgr from
binary or from source. The tool is written in Go (golang).

It assumes that you have already installed the `newt tool on
Windows </newt/install/newt_windows/>`__ and have the Windows
development environment set up.

This guide shows you how to perform the following:

1. Install latest release of newtmgr from binary.
2. Install latest release of newtmgr from source.

See `Installing Previous Releases of Newtmgr </newtmgr/prev_releases>`__
to install an earlier version of newtgmr.

**Note:** If you would like to contribute to the newtmgr tool, see
`Setting Up Go Environment to Contribute to Newt and Newtmgr
Tools </faq/go_env.html>`__.

Installing the Latest Release of Newtmgr Tool from Binary
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

You can install the latest release of newtmgr from binary. It has been
tested on Windows 10 64 bit platform.

 1. Start a MinGW terminal.

 2. Download the newtmgr binary tar file:

.. code-block:: console


    $ wget -P /tmp https://raw.githubusercontent.com/runtimeco/binary-releases/master/mynewt-newt-tools_1.3.0/newtmgr_1_3_0_windows_amd64.tar.gz

 3. Extract the file:

-  If you previously built newtmgr from the master branch, you can
   extract the file into your $GOPATH/bin directory. Note: This
   overwrites the current newtmgr.exe in the directory and assumes that
   you are using $GOPATH/bin for your Go applications.

   ::

        tar -xzf /tmp/newtmgr_1_3_0_windows_amd64.tar.gz -C $GOPATH/bin

-  If you are installing newtmgr for the first time and do not have Go
   setup, you can extract into /usr/bin directory:

   ::

        tar -xzf /tmp/newtmgr_1_3_0_windows_amd64.tar.gz -C /usr/bin

 4. Verify the installed version of newtmgr. See `Checking the Installed
Version <#check_newtmgr>`__.

 ### Installing the Latest Release of Newtmgr from Source

If you have an older version of Windows or a 32 bit platform, you can
build and install the latest release version of newtmgr from source.

 1. Download and install the latest version of
`Go <https://golang.org/dl/>`__. Newtmgr requires Go version 1.7.6 or
higher.

 2. Start MinGW terminal.

 3. Create a Go workspace in the /tmp directory:

.. code-block:: console


    $ cd /tmp
    $ mkdir go
    $ cd go
    $ export GOPATH=/tmp/go

 4. Run ``go get`` to download the newtmgr source. Note that ``go get``
pulls down the HEAD from the master branch in git, builds, and installs
newtmgr.

.. code-block:: console


    $ go get mynewt.apache.org/newtmgr/newtmgr

**Note** If you get the following error, you may ignore it as we will
rebuild newtmgr from the latest release version of newtmgr in the next
step:

.. code-block:: console


    # github.com/currantlabs/ble/examples/lib/dev
    ..\..\..\github.com\currantlabs\ble\examples\lib\dev\dev.go:7: undefined: DefaultDevice

 5. Check out the source from the latest release version:

.. code-block:: console


    $ cd src/mynewt.apache.org/newtmgr
    $ git checkout mynewt_1_3_0_tag
    Note: checking out 'mynewt_1_3_0_tag'.

 6. Build newtmgr from the latest release version:

.. code-block:: console


    $ cd newtmgr
    $ go install
    $ ls /tmp/go/bin/newtmgr.exe
    -rwxr-xr-x 1 user None 15457280 Sep 12 00:30 /tmp/go/bin/newtmgr.exe

 7. If you have a Go workspace, remember to reset your GOPATH to your Go
workspace.

 7. Copy the newtmgr executable to a bin directory in your path. You can
put it in the /usr/bin or the $GOPATH/bin directory.

 ### Checking the Installed Version

 1. Run ``which newtmgr`` to verify that you are using the installed
version of newtmgr.

 2. Get information about the newtmgr tool:

.. code-block:: console


    $newtmgr
    Newtmgr helps you manage remote devices running the Mynewt OS

    Usage:
      newtmgr [flags]
      newtmgr [command]

    Available Commands:
      config      Read or write a config value on a device
      conn        Manage newtmgr connection profiles
      crash       Send a crash command to a device
      datetime    Manage datetime on a device
      echo        Send data to a device and display the echoed back data
      fs          Access files on a device
      help        Help about any command
      image       Manage images on a device
      log         Manage logs on a device
      mpstat      Read mempool statistics from a device
      reset       Perform a soft reset of a device
      run         Run test procedures on a device
      stat        Read statistics from a device
      taskstat    Read task statistics from a device

    Flags:
      -c, --conn string       connection profile to use
      -h, --help              help for newtmgr
      -l, --loglevel string   log level to use (default "info")
          --name string       name of target BLE device; overrides profile setting
      -t, --timeout float     timeout in seconds (partial seconds allowed) (default 10)
      -r, --tries int         total number of tries in case of timeout (default 1)

    Use "newtmgr [command] --help" for more information about a command.

