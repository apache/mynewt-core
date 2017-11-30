Installing Newtmgr on Linux
---------------------------

You can install the latest release (1.3.0) of the newtmgr tool from a
Debian binary package (amd64). You can also download and build the
latest release version of newtmgr from source.

This page shows you how to:

1. Set up your computer to download Debian binary packages from the
   runtimeco APT repository.

   **Note:** The key for signing the repository has changed. If you set
   up your computer before release 1.1.0, you will need to download and
   import the public key again.

2. Install the latest release version of newtmgr from a Debian binary
   package. You can use apt-get to install the package or manually
   download and install the Debian binary package.

3. Download, build, and install the latest release version of newtmgr
   from source.

See `Installing Previous Releases of Newtgmr </newtmgr/prev_releases>`__
to install an earlier version of newtmgr.

If you are installing on an amd64 platform, we recommend that you
install from the binary package.

**Note:** We have tested the newtmgr tool binary and apt-get install
from the runtimeco APT repository for Ubuntu version 16. Earlier Ubuntu
versions (for example: Ubuntu 14) may have incompatibility with the
repository. You can manually download and install the Debian binary
package.

**Note:** See `Setting Up a Go Environment to Contribute to Newt and
Newtmgr Tools </faq/go_env>`__ if you want to:

-  Use the newtmgr tool with the latest updates from the master branch.
   The master branch may be unstable and we recommend that you use the
   latest stable release version.
-  Contribute to the newtmgr tool.

Setting Up Your Computer to use apt-get to Install the Package
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The newtmgr Debian packages are stored in a private APT repository on
**https://github/runtimeco/debian-mynewt**. To use apt-get, you must set
up the following on your computer to retrieve packages from the
repository:

**Note**: You only need to perform this setup once on your computer.
However, if you previously downloaded and imported the public key for
the runtimeco APT repository, you will need to perform step 2 again as
the key has changed.

1. Install the ``apt-transport-https`` package to use HTTPS to retrieve
   packages.
2. Download the public key for the runtimeco APT repository and import
   the key into the apt keychain.
3. Add the repository for the binary and source packages to the apt
   source list.

 1. Install the apt-transport-https package:

.. code-block:: console

    $sudo apt-get update
    $sudo apt-get install apt-transport-https

 2. Download the public key for the runtimeco apt repo (**Note:** There
is a ``-`` after ``apt-key add``):

.. code-block:: console

    wget -qO - https://raw.githubusercontent.com/runtimeco/debian-mynewt/master/mynewt.gpg.key | sudo apt-key add -

 3. Add the repository for the binary packages to the ``mynewt.list``
apt source list file.

.. code-block:: console

    $sudo -s
    [sudo] password for <user>:
    root$ cat > /etc/apt/sources.list.d/mynewt.list <<EOF
    deb https://raw.githubusercontent.com/runtimeco/debian-mynewt/master latest main
    EOF
    root$exit

**Note:** Do not forget to exit the root shell.

 4. Verify the content of the source list file:

.. code-block:: console

    $more /etc/apt/sources.list.d/mynewt.list
    deb https://raw.githubusercontent.com/runtimeco/debian-mynewt/master latest main

 5. Update the available packages:

.. code-block:: console

    $sudo apt-get update

 **Note:** If you are not using Ubuntu version 16, you may see the
following errors. We have provided instructions on how to manually
download and install the binary package.

.. code-block:: console


    W: Failed to fetch https://raw.githubusercontent.com/runtimeco/debian-mynewt/master/dists/latest/main/source/Sources  Ht
    tpError404

 ### Installing the Latest Release of Newtmgr from a Binary Package

You can use either apt-get to install the package, or manually download
and install the Debian binary package.

 #### Method 1: Using apt-get to Upgrade or to Install

Run the following commands to upgrade or install the latest version of
newtmgr:

.. code-block:: console


    $ sudo apt-get update 
    $ sudo apt-get install newtmgr

Method 2: Downloading and Installing the Debian Package Manually
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Download and install the package manually.

.. code-block:: console

    $wget https://raw.githubusercontent.com/runtimeco/debian-mynewt/master/pool/main/n/newtmgr/newtmgr_1.3.0-1_amd64.deb
    $sudo dpkg -i newtmgr_1.3.0-1_amd64.deb

See `Checking the Installed Version of Newtmgr <#check>`__ to verify
that you are using the installed version of newtmgr.

 ### Installing the Latest Release Version of Newtmgr from Source

If you are running Linux on a different architecture, you can build and
install the latest release version of newtmgr from source.

 1. Download and install the latest version of
`Go <https://golang.org/dl/>`__. Newtmgr requires Go version 1.7.6 or
higher.

 2. Create a Go workspace in the /tmp directory:

.. code-block:: console


    $ cd /tmp
    $ mkdir go
    $ cd go
    $ export GOPATH=/tmp/go

 3. Run ``go get`` to download the newtmgr source. Note that ``go get``
pulls down the HEAD from the master branch in git, builds, and installs
newtmgr.

.. code-block:: console


    $ go get mynewt.apache.org/newtmgr/newtmgr
    $ ls -l /tmp/go/bin/newtmgr
    -rwxr-xr-x  1 user staff  17884488 Jul 29 16:25 /tmp/go/bin/newtmgr

 4. Check out the source from the latest release version:

.. code-block:: console


    $ cd src/mynewt.apache.org/newtmgr
    $ git checkout mynewt_1_3_0_tag
    Note: checking out 'mynewt_1_3_0_tag'.

 5. Build newtmgr from the latest release version:

.. code-block:: console


    $ cd newtmgr
    $ go install
    $ ls /tmp/go/bin/newtmgr
    -rwxr-xr-x  1 user  staff  17888680 Jul 29 16:28 /tmp/go/bin/newtmgr

 6. If you have a Go workspace, remember to reset your GOPATH to your Go
workspace.

 7. Copy the newtmgr executable to a bin directory in your path. You can
put it in the /usr/bin or the $GOPATH/bin directory.

 ### Checking the Latest Version of Newtmgr is Installed

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


