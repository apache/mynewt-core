newtmgr image 
--------------

Manage images on a device.

Usage:
^^^^^^

.. code-block:: console

        newtmgr image [command] -c <connection_profile> [flags] 

Flags:
^^^^^^

The coredownload subcommand uses the following local flags:

.. code-block:: console

        -n, --bytes uint32         Number of bytes of the core to download 
        -e, --elfify               Create an ELF file 
            --offset unint32       Offset of the core file to start the download 

 #### Global Flags:

.. code-block:: console

      -c, --conn string       connection profile to use
      -h, --help              help for newtmgr
      -l, --loglevel string   log level to use (default "info")
          --name string       name of target BLE device; overrides profile setting
      -t, --timeout float     timeout in seconds (partial seconds allowed) (default 10)
      -r, --tries int         total number of tries in case of timeout (default 1)

 ####Description The image command provides subcommands to manage core
and image files on a device. Newtmgr uses the ``conn_profile``
connection profile to connect to the device.

+----------------+---------------------------+
| Sub-command    | Explanation               |
+================+===========================+
| confirm        | The newtmgr image confirm |
|                | [hex-image-hash] command  |
|                | makes an image setup      |
|                | permanent on a device. If |
|                | a ``hex-image-hash`` hash |
|                | value is specified,       |
|                | Mynewt permanently        |
|                | switches to the image     |
|                | identified by the hash    |
|                | value. If a hash value is |
|                | not specified, the        |
|                | current image is made     |
|                | permanent.                |
+----------------+---------------------------+
| coreconvert    | The newtmgr image         |
|                | coreconvert               |
|                | <core-filename>           |
|                | <elf-file> command        |
|                | converts the              |
|                | ``core-filename`` core    |
|                | file to an ELF format and |
|                | names it ``elf-file``.    |
|                | **Note**: This command    |
|                | does not download the     |
|                | core file from a device.  |
|                | The core file must exist  |
|                | on your host.             |
+----------------+---------------------------+
| coredownload   | The newtmgr image         |
|                | coredownload              |
|                | <core-filename> command   |
|                | downloads the core file   |
|                | from a device and names   |
|                | the file                  |
|                | ``core-filename`` on your |
|                | host. Use the local flags |
|                | under Flags to customize  |
|                | the command.              |
+----------------+---------------------------+
| coreerase      | The newtmgr image         |
|                | coreerase command erases  |
|                | the core file on a        |
|                | device.                   |
+----------------+---------------------------+
| corelist       | The newtmgr image         |
|                | corelist command lists    |
|                | the core(s) on a device.  |
+----------------+---------------------------+
| erase          | The newtmgr image erase   |
|                | command erases an unused  |
|                | image from the secondary  |
|                | image slot on a device.   |
|                | The image cannot be       |
|                | erased if the image is a  |
|                | confirmed image, is       |
|                | marked for test on the    |
|                | next reboot, or is an     |
|                | active image for a split  |
|                | image setup.              |
+----------------+---------------------------+
| list           | The newtmgr image list    |
|                | command displays          |
|                | information for the       |
|                | images on a device.       |
+----------------+---------------------------+
| test           | The newtmgr test          |
|                | <hex-image-hash> command  |
|                | tests the image,          |
|                | identified by the         |
|                | ``hex-image-hash`` hash   |
|                | value, on next reboot.    |
+----------------+---------------------------+
| upload         | The newtmgr image upload  |
|                | <image-file> command      |
|                | uploads the               |
|                | ``image-file`` image file |
|                | to a device.              |
+----------------+---------------------------+

Examples
^^^^^^^^

+----------------+--------------------------+--------------------+
| Sub-command    | Usage                    | Explanation        |
+================+==========================+====================+
| confirm        | newtmgr confirm-c        | Makes the current  |
|                | profile01                | image setup on a   |
|                |                          | device permanent.  |
|                |                          | Newtmgr connects   |
|                |                          | to the device over |
|                |                          | a connection       |
|                |                          | specified in the   |
|                |                          | ``profile01``      |
|                |                          | connection         |
|                |                          | profile.           |
+----------------+--------------------------+--------------------+
| confirm        | newtmgr                  | Makes the image,   |
|                | confirmbe9699809a049...7 | identified by the  |
|                | 3d77f-c                  | ``be9699809a049... |
|                | profile01                | 73d77f``           |
|                |                          | hash value, setup  |
|                |                          | on a device        |
|                |                          | permanent. Newtmgr |
|                |                          | connects to the    |
|                |                          | device over a      |
|                |                          | connection         |
|                |                          | specified in the   |
|                |                          | ``profile01``      |
|                |                          | connection         |
|                |                          | profile.           |
+----------------+--------------------------+--------------------+
| coreconvert    | newtmgr image            | Converts the       |
|                | coreconvert mycore       | ``mycore`` file to |
|                | mycore.elf               | the ELF format and |
|                |                          | saves it in the    |
|                |                          | ``mycore.elf``     |
|                |                          | file.              |
+----------------+--------------------------+--------------------+
| coredownload   | newtmgr image            | Downloads the core |
|                | coredownload mycore -c   | from a device and  |
|                | profile01                | saves it in the    |
|                |                          | ``mycore`` file.   |
|                |                          | Newtmgr connects   |
|                |                          | to the device over |
|                |                          | a connection       |
|                |                          | specified in the   |
|                |                          | ``profile01``      |
|                |                          | connection         |
|                |                          | profile.           |
+----------------+--------------------------+--------------------+
| coredownload   | newtmgr image            | Downloads the core |
|                | coredownload mycore -e   | from a device,     |
|                | -c profile01             | converts the core  |
|                |                          | file into the ELF  |
|                |                          | format, and saves  |
|                |                          | it in the          |
|                |                          | ``mycore`` file.   |
|                |                          | Newtmgr connects   |
|                |                          | to the device over |
|                |                          | a connection       |
|                |                          | specified in the   |
|                |                          | ``profile01``      |
|                |                          | connection         |
|                |                          | profile.           |
+----------------+--------------------------+--------------------+
| coredownload   | newtmgr image            | Downloads 30       |
|                | coredownload mycore      | bytes, starting at |
|                | --offset 10 -n 30-c      | offset 10, of the  |
|                | profile01                | core from a device |
|                |                          | and saves it in    |
|                |                          | the ``mycore``     |
|                |                          | file. Newtmgr      |
|                |                          | connects to the    |
|                |                          | device over a      |
|                |                          | connection         |
|                |                          | specified in the   |
|                |                          | ``profile01``      |
|                |                          | connection         |
|                |                          | profile.           |
+----------------+--------------------------+--------------------+
| coreerase      | newtmgr image coreerase  | Erases the core    |
|                | -c profile01             | file on a device.  |
|                |                          | Newtmgr connects   |
|                |                          | to the device over |
|                |                          | a connection       |
|                |                          | specified in the   |
|                |                          | ``profile01``      |
|                |                          | connection         |
|                |                          | profile.           |
+----------------+--------------------------+--------------------+
| corelist       | newtmgr image corelist-c | Lists the core     |
|                | profile01                | files on a device. |
|                |                          | Newtmgr connects   |
|                |                          | to the device over |
|                |                          | a connection       |
|                |                          | specified in the   |
|                |                          | ``profile01``      |
|                |                          | connection         |
|                |                          | profile.           |
+----------------+--------------------------+--------------------+
| erase          | newtmgr image erase-c    | Erases the image,  |
|                | profile01                | if unused, from    |
|                |                          | the secondary      |
|                |                          | image slot on a    |
|                |                          | device. Newtmgr    |
|                |                          | connects to the    |
|                |                          | device over a      |
|                |                          | connection         |
|                |                          | specified in the   |
|                |                          | ``profile01``      |
|                |                          | connection         |
|                |                          | profile.           |
+----------------+--------------------------+--------------------+
| list           | newtmgr image list-c     | Lists the images   |
|                | profile01                | on a device.       |
|                |                          | Newtmgr connects   |
|                |                          | to the device over |
|                |                          | a connection       |
|                |                          | specified in the   |
|                |                          | ``profile01``      |
|                |                          | connection         |
|                |                          | profile.           |
+----------------+--------------------------+--------------------+
| test           | newtmgr image test       | Tests the image,   |
|                | be9699809a049...73d77f   | identified by the  |
|                |                          | ``be9699809a049... |
|                |                          | 73d77f``           |
|                |                          | hash value, during |
|                |                          | the next reboot on |
|                |                          | a device. Newtmgr  |
|                |                          | connects to the    |
|                |                          | device over a      |
|                |                          | connection         |
|                |                          | specified in the   |
|                |                          | ``profile01``      |
|                |                          | connection         |
|                |                          | profile.           |
+----------------+--------------------------+--------------------+
| upload         | newtmgr image upload     | Uploads the        |
|                | bletiny.img-c profile01  | ``bletiny.img``    |
|                |                          | image to a device. |
|                |                          | Newtmgr connects   |
|                |                          | to the device over |
|                |                          | a connection       |
|                |                          | specified in the   |
|                |                          | ``profile01``      |
|                |                          | connection         |
|                |                          | profile.           |
+----------------+--------------------------+--------------------+
