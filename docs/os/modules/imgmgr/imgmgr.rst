Image Manager
=============

.. toctree::
   :maxdepth: 1

   imgmgr_module_init <imgmgr_module_init>
   imgr_ver_parse <imgr_ver_parse>
   imgr_ver_str <imgr_ver_str>

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


List of Functions
~~~~~~~~~~~~~~~~~

The functions available in imgmgr are:

+------------------------------+----------------+
| Function                     | Description    |
+==============================+================+
| :c:func:`imgr_ver_parse`     | Parses         |
|                              | character      |
|                              | string         |
|                              | containing     |
|                              | specified      |
|                              | image version  |
|                              | number and     |
|                              | writes that to |
|                              | given          |
|                              | image\_version |
|                              | struct.        |
+------------------------------+----------------+
| :c:func:`imgr_ver_str`       | Takes version  |
|                              | string from    |
|                              | specified      |
|                              | image\_version |
|                              | struct and     |
|                              | formats it     |
|                              | into a         |
|                              | printable      |
|                              | string.        |
+------------------------------+----------------+
