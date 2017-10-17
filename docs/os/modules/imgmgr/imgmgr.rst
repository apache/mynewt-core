Image Manager
=============

Description
~~~~~~~~~~~

This library accepts incoming image management commands from newtmgr and
acts on them.

Images can be uploaded, present images listed, and system can be told to
switch to another image.

Currently the package assumes that there are 2 image slots, one active
one and another one in standby. When new image is uploaded, it replaces
the one in standby slot. This is the model for scenario when MCU has
internal flash only, it executes the code from that flash, and there is
enough space to store 2 full images.

Image manager interacts with bootloader by telling it to boot to a
specific image. At the moment this has to be done by writing a file
which contains a version number of the image to boot. Note that image
manager itself does not replace the active image.

Image manager also can upload files to filesystem as well as download
them.

Note that commands accessing filesystems (next boot target, file
upload/download) will not be available unless project includes
filesystem implementation.

Data structures
~~~~~~~~~~~~~~~

N/A.

List of Functions
~~~~~~~~~~~~~~~~~

The functions available in imgmgr are:

+------------+----------------+
| Function   | Description    |
+============+================+
| `imgr\_ver | Parses         |
| \_parse <i | character      |
| mgr_ver_pa | string         |
| rse.html>`__ | containing     |
|            | specified      |
|            | image version  |
|            | number and     |
|            | writes that to |
|            | given          |
|            | image\_version |
|            | struct.        |
+------------+----------------+
| `imgr\_ver | Takes version  |
| \_str <img | string from    |
| r_ver_str. | specified      |
| md>`__     | image\_version |
|            | struct and     |
|            | formats it     |
|            | into a         |
|            | printable      |
|            | string.        |
+------------+----------------+
