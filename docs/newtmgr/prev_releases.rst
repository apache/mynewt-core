Installing Previous Releases of Newtmgr
---------------------------------------

This page shows you how to install previous releases of newtmgr for Mac
OS, Linux, and Windows.

Mac OS
~~~~~~

You can install previous releases of newtmgr using
``mynewt-newtmgr@X.Y`` Homebrew formulas, where X.Y is a version number.

For example, if you want to install newtmgr 1.0, run the following
commands:

.. code-block:: console


    $ brew update
    $ brew install mynewt-newtmgr@1.0

**Note:** This is a keg-only installation. newtgmr 1.0 is installed in
/usr/local/Cellar/mynewt-newtmgr@1.0/1.0.0/bin but not symlinked into
/usr/local/bin.

If you need this version of newtmgr first in your PATH, run the
following commands:

.. code-block:: console


    $ echo 'export PATH=/usr/local/Cellar/mynewt-newtmgr@1.0/1.0.0/bin:$PATH' >> ~/.bash_profile
    $ source ~/.bash_profile

 You can also manually symlink into /usr/local/bin as follows:

 1. Unlink newtmgr if you have the latest version of newtmgr installed:

.. code-block:: console

    $ brew unlink mynewt-newtmgr

 2. Link mynewt-newt@1.0 into /usr/local/bin:

.. code-block:: console

    $ brew link -f mynewt-newtmgr@1.0

 ### Linux 1. Download the binary:

+-----------+----------------------------------------------------------------------------------------------------------------------------------------------------------+
| Version   | Download                                                                                                                                                 |
+===========+==========================================================================================================================================================+
| 1.0.0     | `newtmgr\_1.0.0-1\_amd64.deb <https://raw.githubusercontent.com/runtimeco/binary-releases/master/mynewt-newt-tools_1.0.0/newtmgr_1.0.0-1_amd64.deb>`__   |
+-----------+----------------------------------------------------------------------------------------------------------------------------------------------------------+
| 1.1.0     | `newtmgr\_1.1.0-1\_amd64.deb <https://raw.githubusercontent.com/runtimeco/binary-releases/master/mynewt-newt-tools_1.1.0/newtmgr_1.1.0-1_amd64.deb>`__   |
+-----------+----------------------------------------------------------------------------------------------------------------------------------------------------------+

 2. Run the ``sudo apt-get remove newtmgr`` command to remove the the
current installation.

 3. Install the package. For example, run
``sudo dpkg -i newtmgr_1.0.0-1_amd64.deb`` to install newtmgr 1.0.0

 ### Windows 1. Download the binary:

+-----------+----------------------------------------------------------------------------------------------------------------------------------------------------------------------+
| Version   | Download                                                                                                                                                             |
+===========+======================================================================================================================================================================+
| 1.1.0     | `newtmgr\_1\_1\_0\_windows\_amd64.tar.gz <https://raw.githubusercontent.com/runtimeco/binary-releases/master/mynewt-newt-tools_1.1.0/newtmgr_1.1.0-1_amd64.deb>`__   |
+-----------+----------------------------------------------------------------------------------------------------------------------------------------------------------------------+

 2. Extract the file:

-  If you previously built newtmgr from the master branch, you can
   extract the file into your $GOPATH/bin directory. Note: This
   overwrites the current newtmgr.exe in the directory and assumes that
   you are using $GOPATH/bin for your Go applications.

   ::

       tar -xzf newtmgr_1_1_0_windows_amd64.tar.gz -C $GOPATH/bin

-  If you are installing newtmgr for the first time and do not have a Go
   workspace setup, you can extract into /usr/bin directory:

   ::

       tar -xzf newtmgr_1_1_0_windows_amd64.tar.gz -C /usr/bin
