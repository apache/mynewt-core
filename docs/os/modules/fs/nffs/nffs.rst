Newtron Flash Filesystem (nffs)
===============================

Mynewt includes the Newtron Flash File System (nffs). This file system
is designed with two priorities that makes it suitable for embedded use:

-  Minimal RAM usage
-  Reliability

Mynewt also provides an abstraction layer API (fs) to allow you to swap
out nffs with a different file system of your choice.

Description
~~~~~~~~~~~

Areas
^^^^^

At the top level, an nffs disk is partitioned into *areas*. An area is a
region of disk with the following properties:

1. An area can be fully erased without affecting any other areas.
2. Writing to one area does not restrict writes to other areas.

**Regarding property 1:** Generally, flash hardware divides its memory
space into "blocks." When erasing flash, entire blocks must be erased in
a single operation; partial erases are not possible.

**Regarding property 2:** Furthermore, some flash hardware imposes a
restriction with regards to writes: writes within a block must be
strictly sequential. For example, if you wish to write to the first 16
bytes of a block, you must write bytes 1 through 15 before writing byte
16. This restriction only applies at the block level; writes to one
block have no effect on what parts of other blocks can be written.

Thus, each area must comprise a discrete number of blocks.

Initialization
^^^^^^^^^^^^^^

As part of overall system initialization, mynewt re-initialized the
filesystem as follows:

1. Restores an existing file system via detection.
2. Creates a new file system via formatting.

A typical initialization sequence is the following:

1. Detect an nffs file system in a specific region of flash.
2. If no file system was detected, if configured to do so, format a new
   file system in the same flash region.

Note that in the latter case, the behavior is controlled with a variable
in the syscfg.yml file. If NFFS\_DETECT\_FAIL is set to 1, the system
ignores NFFS filesystem detection issues, but unless a new filesystem is
formatted manually, all filesystem access will fail. If
NFFS\_DETECT\_FAIL is set to 2, the system will format a new filesystem
- note however this effectively deletes all existing data in the NFFS
flash areas.

Both methods require the user to describe how the flash memory should be
divided into nffs areas. This is accomplished with an array of `struct
nffs\_area\_desc <nffs_area_desc.html>`__ configured as part of the BSP
configureation.

After nffs has been initialized, the application can access the file
system via the `file system abstraction layer <../fs/fs.html>`__.

Data Structures
~~~~~~~~~~~~~~~

The ``fs/nffs`` package exposes the following data structures:

+---------------------------------------------------+--------------------------------------+
| Struct                                            | Description                          |
+===================================================+======================================+
| `struct nffs\_area\_desc <nffs_area_desc.html>`__   | Descriptor for a single nffs area.   |
+---------------------------------------------------+--------------------------------------+
| `struct nffs\_config <nffs_config.html>`__          | Configuration struct for nffs.       |
+---------------------------------------------------+--------------------------------------+

API
~~~

The functions available in this OS feature are:

+------------+----------------+
| Function   | Description    |
+============+================+
| `nffs\_det | Searches for a |
| ect <nffs_ | valid nffs     |
| detect.html> | file system    |
| `__        | among the      |
|            | specified      |
|            | areas.         |
+------------+----------------+
| `nffs\_for | Erases all the |
| mat <nffs_ | specified      |
| format.html> | areas and      |
| `__        | initializes    |
|            | them with a    |
|            | clean nffs     |
|            | file system.   |
+------------+----------------+
| `nffs\_ini | Initializes    |
| t <nffs_in | internal nffs  |
| it.html>`__  | memory and     |
|            | data           |
|            | structures.    |
+------------+----------------+

Miscellaneous measures
~~~~~~~~~~~~~~~~~~~~~~

-  RAM usage:

   -  24 bytes per inode
   -  12 bytes per data block
   -  36 bytes per inode cache entry
   -  32 bytes per data block cache entry

-  Maximum filename size: 256 characters (no null terminator required)
-  Disallowed filename characters: '/' and ':raw-latex:`\0`'

Internals
~~~~~~~~~

nffs implementation details can be found here:

-  `nffs\_internals <nffs_internals.html>`__

Future enhancements
~~~~~~~~~~~~~~~~~~~

-  Error correction.
-  Encryption.
-  Compression.
