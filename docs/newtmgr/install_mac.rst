Installing Newtmgr on Mac OS
----------------------------

Newtmgr is supported on Mac OS X 64 bit platforms and has been tested on
Mac OS 10.11 and higher.

This page shows you how to install the following versions of newtmgr:

-  Upgrade to or install the latest release version (1.3.0).
-  Install the latest from the master branch (unstable).

See `Installing Previous Releases of Newtmgr </newtmgr/prev_releases>`__
to install an earlier version of newtmgr.

**Note:** If you would like to contribute to the newtmgr tool, see
`Setting Up Go Environment to Contribute to Newt and Newtmgr
Tools </faq/go_env>`__.

Adding the Mynewt Homebrew Tap
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

You should have added the **runtimeco/homebrew-mynewt** tap when you
installed the **newt** tool. Run the following commands if you have not
done so:

.. code-block:: console


    $ brew tap runtimeco/homebrew-mynewt
    $ brew update

Upgrading to or Installing the Latest Release Version
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Perform the following to upgrade or install the latest release version
of newtmgr.

Upgrading to the Latest Release Version of Newtmgr
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If you have installed an earlier version of newtmgr using brew, run the
following commands to upgrade to the latest version of newtmgr:

.. code-block:: console


    $ brew update
    $ brew upgrade mynewt-newtmgr

 #### Installing the Latest Release Version of Newtmgr

Run the following command to install the latest release version of
newtmgr:

.. code-block:: console


    $ brew update
    $ brew install mynewt-newtmgr
    ==> Installing mynewt-newtmgr from runtimeco/mynewt
    ==> Downloading https://github.com/runtimeco/binary-releases/raw/master/mynewt-newt-tools_1.3.0/mynewt-newtmgr-1.3.0.sierra.bottle.tar.gz
    ==> Downloading from https://raw.githubusercontent.com/runtimeco/binary-releases/master/mynewt-newt-tools_1.3.0/mynewt-newtmgr-1.3.0.sierra.bottle.tar.gz
    ######################################################################## 100.0%
    ==> Pouring mynewt-newtmgr-1.3.0.sierra.bottle.tar.gz
    🍺  /usr/local/Cellar/mynewt-newtmgr/1.3.0: 3 files, 17.3MB

 **Notes:** Homebrew bottles for newtmgr 1.3.0 are available for Mac OS
Sierra, El Captian. If you are running an earlier version of Mac OS, the
installation will install the latest version of Go and compile newtmgr
locally.

 ### Checking the Installed Version

Check that you are using the installed version of newtmgr:

.. code-block:: console

    $which newtmgr
    /usr/local/bin/newtmgr
    ls -l /usr/local/bin/newtmgr
    lrwxr-xr-x  1 user  staff  42 Sep 11 21:15 /usr/local/bin/newtmgr -> ../Cellar/mynewt-newtmgr/1.3.0/bin/newtmgr

**Note:** If you previously built newtmgr from source and the output of
``which newtmgr`` shows
":math:`GOPATH/bin/newtmgr", you will need to move "`\ GOPATH/bin" after
"/usr/local/bin" for your PATH in ~/.bash\_profile, and source
~/.bash\_profile.

 Get information about newtmgr:

.. code-block:: console


    $ newtmgr help
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

Installing Newtmgr from the Master Branch
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

We recommend that you use the latest release version of newtmgr. If you
would like to use the master branch with the latest updates, you can
install newtmgr from the HEAD of the master branch.

\*\* Notes: \*\*

-  The master branch may be unstable.
-  This installation will install the latest version of Go on your
   computer, if it is not installed, and compile newtmgr locally.

 If you already installed newtgmr, unlink the current version:

.. code-block:: console

    $brew unlink mynewt-newtmgr

 Install the latest unstable version of newtmgr from the master branch:

.. code-block:: console

    $brew install mynewt-newtmgr --HEAD
    ==> Installing mynewt-newtmgr from runtimeco/mynewt
    ==> Cloning https://github.com/apache/mynewt-newtmgr.git
    Cloning into '/Users/wanda/Library/Caches/Homebrew/mynewt-newtmgr--git'...
    remote: Counting objects: 2169, done.
    remote: Compressing objects: 100% (1752/1752), done.
    remote: Total 2169 (delta 379), reused 2042 (delta 342), pack-reused 0
    Receiving objects: 100% (2169/2169), 8.13 MiB | 5.47 MiB/s, done.
    Resolving deltas: 100% (379/379), done.
    ==> Checking out branch master
    ==> go get github.com/currantlabs/ble
    ==> go get github.com/raff/goble
    ==> go get github.com/mgutz/logxi/v1
    ==> go install
    🍺  /usr/local/Cellar/mynewt-newtmgr/HEAD-2d5217f: 3 files, 17.3MB, built in 1 minute 10 seconds

 To switch back to the latest stable release version of newtmgr, you can
run:

.. code-block:: console

    $brew switch mynewt-newtmgr 1.3.0
    Cleaning /usr/local/Cellar/mynewt-newtmgr/1.3.0
    Cleaning /usr/local/Cellar/mynewt-newtmgr/HEAD-2d5217f
    1 links created for /usr/local/Cellar/mynewt-newtmgr/1.3.0


