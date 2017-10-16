Installing Previous Releases of Newt
------------------------------------

This page shows you how to install previous releases of newt for Mac OS,
Linux, and Windows.

Mac OS
~~~~~~

You can install previous releases of newt using ``mynewt-newt@X.Y``
Homebrew formulas, where X.Y is a version number.

For example, if you want to install newt 1.0, run the following
commands:

.. code-block:: console


    $ brew update
    $ brew install mynewt-newt@1.0

**Note:** This is a keg-only installation. newt 1.0 is installed in
/usr/local/Cellar/mynewt-newt@1.0/1.0.0/bin but not symlinked into
/usr/local/bin.

If you need this version of newt first in your PATH, run the following
commands:

.. code-block:: console


    $ echo 'export PATH=/usr/local/Cellar/mynewt-newt@1.0/1.0.0/bin:$PATH' >> ~/.bash_profile
    $ source ~/.bash_profile

 You can also manually symlink into /usr/local/bin as follows:

 1. Unlink newt if you have the latest version of newt installed:

.. code-block:: console

    $ brew unlink mynewt-newt

 2. Link mynewt-newt@1.0 into /usr/local/bin:

.. code-block:: console

    $ brew link -f mynewt-newt@1.0

 ### Linux 1. Download the binary:

+-----------+----------------------------------------------------------------------------------------------------------------------------------------------------+
| Version   | Download                                                                                                                                           |
+===========+====================================================================================================================================================+
| 1.0.0     | `newt\_1.0.0-1\_amd64.deb <https://raw.githubusercontent.com/runtimeco/binary-releases/master/mynewt-newt-tools_1.0.0/newt_1.0.0-1_amd64.deb>`__   |
+-----------+----------------------------------------------------------------------------------------------------------------------------------------------------+
| 1.1.0     | `newt\_1.1.0-1\_amd64.deb <https://raw.githubusercontent.com/runtimeco/binary-releases/master/mynewt-newt-tools_1.1.0/newt_1.1.0-1_amd64.deb>`__   |
+-----------+----------------------------------------------------------------------------------------------------------------------------------------------------+

 2. Run the ``sudo apt-get remove newt`` command to remove the the
current installation.

 3. Install the package. For example, run
``sudo dpkg -i newt_1.0.0-1_amd64.deb`` to install newt 1.0.0

 ### Windows 1. Download the binary:

+-----------+----------------------------------------------------------------------------------------------------------------------------------------------------------------+
| Version   | Download                                                                                                                                                       |
+===========+================================================================================================================================================================+
| 1.1.0     | `newt\_1\_1\_0\_windows\_amd64.tar.gz <https://raw.githubusercontent.com/runtimeco/binary-releases/master/mynewt-newt-tools_1.1.0/newt_1.1.0-1_amd64.deb>`__   |
+-----------+----------------------------------------------------------------------------------------------------------------------------------------------------------------+

 2. Extract the file:

-  If you previously built newt from the master branch, you can extract
   the file into your $GOPATH/bin directory. Note: This overwrites the
   current newt.exe in the directory and assumes that you are using
   $GOPATH/bin for your Go applications.

   ::

       tar -xzf newt_1_1_0_windows_amd64.tar.gz -C $GOPATH/bin

-  If you are installing newt for the first time and do not have a Go
   workspace setup, you can extract into /usr/bin directory:

   ::

       tar -xzf newt_1_1_0_windows_amd64.tar.gz -C /usr/bin
