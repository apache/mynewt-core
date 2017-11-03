File System Abstraction
=======================

Mynewt provides a file system abstraction layer (``fs/fs``) to allow
client code to be file system agnostic. By accessing the file system via
the ``fs/fs`` API, client code can perform file system operations
without being tied to a particular implementation. When possible,
library code should use the ``fs/fs`` API rather than accessing the
underlying file system directly.

Description
~~~~~~~~~~~

Applications should aim to minimize the amount of code which depends on
a particular file system implementation. When possible, only depend on
the ``fs/fs`` package. In terms of the Mynewt hierarchy, an **app**
package must depend on a specific file system package, while **library**
packages should only depend on ``fs/fs``.

Applications wanting to access a filesystem are required to include the
necessary packages in their applications pkg.yml file. In the following
example, the ```Newtron Flash File System`` <../nffs/nffs.html>`__ is
used.

.. code-block:: console

    # repos/apache-mynewt-core/apps/slinky/pkg.yml

    pkg.name: repos/apache-mynewt-core/apps/slinky
    pkg.deps:
        - fs/fs         # include the file operations interfaces
        - fs/nffs       # include the NFFS filesystem implementation

::

    # repos/apache-mynewt-core/apps/slinky/syscfg.yml
    # [...]
     # Package: apps/<example app>
    # [...]
        CONFIG_NFFS: 1  # initialize and configure NFFS into the system
    #   NFFS_DETECT_FAIL: 1   # Ignore NFFS detection issues 
    #   NFFS_DETECT_FAIL: 2   # Format a new NFFS file system on failure to detect

    # [...]

Consult the documentation for ```nffs`` <../nffs/nffs.html>`__ for a more
detailed explanation of NFFS\_DETECT\_FAIL

Code which uses the file system after the system has been initialized
need only depend on ``fs/fs``. For example, the ``libs/imgmgr`` package
is a library which provides firmware upload and download functionality
via the use of a file system. This library is only used after the system
has been initialized, and therefore only depends on the ``fs/fs``
package.

.. code-block:: console

    # repos/apache-mynewt-core/libs/imgmgr/pkg.yml
    pkg.name: libs/imgmgr
    pkg.deps:
        - fs/fs

    # [...]

The ``libs/imgmgr`` package uses the ``fs/fs`` API for all file system
operations.

Support for multiple filesystems
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When using a single filesystem/disk, it is valid to provide paths in the
standard unix way, eg, ``/<dir-name>/<file-name>``. When trying to run
more than one filesystem or a single filesystem in multiple devices
simultaneosly, an extra name has to be given to the disk that is being
used. The abstraction for that was added as the ``fs/disk`` package
which is a dependency of ``fs/fs``. It adds the following extra user
function:

.. code:: c

    int disk_register(const char *disk_name, const char *fs_name, struct disk_ops *dops)

As an example os usage:

.. code:: c

    disk_register("mmc0", "fatfs", &mmc_ops);
    disk_register("flash0", "nffs", NULL);

This registers the name ``mmc0`` to use ``fatfs`` as the filesystem and
``mmc_ops`` for the low-level disk driver and also registers ``flash0``
to use ``nffs``. ``nffs`` is currently strongly bound to the
``hal_flash`` interface, ignoring any other possible ``disk_ops`` given.

struct disk\_ops
^^^^^^^^^^^^^^^^

To support a new low-level disk interface, the ``struct disk_ops``
interface must be implemented by the low-level driver. Currently only
``read`` and ``write`` are effectively used (by ``fatfs``).

.. code:: c

    struct disk_ops {
        int (*read)(uint8_t, uint32_t, void *, uint32_t);
        int (*write)(uint8_t, uint32_t, const void *, uint32_t);
        int (*ioctl)(uint8_t, uint32_t, void *);
        SLIST_ENTRY(disk_ops) sc_next;
    }

Thread Safety
~~~~~~~~~~~~~

All ``fs/fs`` functions are thread safe.

Header Files
~~~~~~~~~~~~

All code which uses the ``fs/fs`` package needs to include the following
header:

.. code:: c

    #include "fs/fs.h"

Data Structures
~~~~~~~~~~~~~~~

All ``fs/fs`` data structures are opaque to client code.

.. code:: c

    struct fs_file;
    struct fs_dir;
    struct fs_dirent;

API
~~~

Functions in ``fs/fs`` that indicate success or failure do so with the
following set of return codes:

-  `Return Codes <fs_return_codes.html>`__

The functions available in this OS feature are:

+------------+----------------+
| Function   | Description    |
+============+================+
| `fs\_close | Closes the     |
|  <fs_close | specified file |
| .html>`__    | and            |
|            | invalidates    |
|            | the file       |
|            | handle.        |
+------------+----------------+
| `fs\_close | Closes the     |
| dir <fs_cl | specified      |
| osedir.html> | directory      |
| `__        | handle.        |
+------------+----------------+
| `fs\_diren | Tells you      |
| t\_is\_dir | whether the    |
|  <fs_diren | specified      |
| t_is_dir.m | directory      |
| d>`__      | entry is a     |
|            | sub-directory  |
|            | or a regular   |
|            | file.          |
+------------+----------------+
| `fs\_diren | Retrieves the  |
| t\_name <f | filename of    |
| s_dirent_n | the specified  |
| ame.html>`__ | directory      |
|            | entry.         |
+------------+----------------+
| `fs\_filel | Retrieves the  |
| en <fs_fil | current length |
| elen.html>`_ | of the         |
| _          | specified open |
|            | file.          |
+------------+----------------+
| `fs\_getpo | Retrieves the  |
| s <fs_getp | current read   |
| os.html>`__  | and write      |
|            | position of    |
|            | the specified  |
|            | open file.     |
+------------+----------------+
| `fs\_mkdir | Creates the    |
|  <fs_mkdir | directory      |
| .html>`__    | represented by |
|            | the specified  |
|            | path.          |
+------------+----------------+
| `fs\_open  | Opens a file   |
| <fs_open.m | at the         |
| d>`__      | specified      |
|            | path.          |
+------------+----------------+
| `fs\_opend | Opens the      |
| ir <fs_ope | directory at   |
| ndir.html>`_ | the specified  |
| _          | path.          |
+------------+----------------+
| `fs\_read  | Reads data     |
| <fs_read.m | from the       |
| d>`__      | specified      |
|            | file.          |
+------------+----------------+
| `fs\_readd | Reads the next |
| ir <fs_rea | entry in an    |
| ddir.html>`_ | open           |
| _          | directory.     |
+------------+----------------+
| `fs\_regis | Registers a    |
| ter <fs_re | file system    |
| gister.html> | with the       |
| `__        | abstraction    |
|            | layer.         |
+------------+----------------+
| `fs\_renam | Performs a     |
| e <fs_rena | rename and/or  |
| me.html>`__  | move of the    |
|            | specified      |
|            | source path to |
|            | the specified  |
|            | destination.   |
+------------+----------------+
| `fs\_seek  | Positions a    |
| <fs_seek.m | file's read    |
| d>`__      | and write      |
|            | pointer at the |
|            | specified      |
|            | offset.        |
+------------+----------------+
| `fs\_unlin | Unlinks the    |
| k <fs_unli | file or        |
| nk.html>`__  | directory at   |
|            | the specified  |
|            | path.          |
+------------+----------------+
| `fs\_write | Writes the     |
|  <fs_write | supplied data  |
| .html>`__    | to the current |
|            | offset of the  |
|            | specified file |
|            | handle.        |
+------------+----------------+

Additional file system utilities that bundle some of the basic functions
above are:

+------------+----------------+
| Function   | Description    |
+============+================+
| `fsutil\_r | Opens a file   |
| ead\_file  | at the         |
| <fsutil_re | specified      |
| ad_file.md | path, retrieve |
| >`__       | data from the  |
|            | file starting  |
|            | from the       |
|            | specified      |
|            | offset, and    |
|            | close the file |
|            | and invalidate |
|            | the file       |
|            | handle.        |
+------------+----------------+
| `fsutil\_w | Open a file at |
| rite\_file | the specified  |
|  <fsutil_w | path, write    |
| rite_file. | the supplied   |
| md>`__     | data to the    |
|            | current offset |
|            | of the         |
|            | specified file |
|            | handle, and    |
|            | close the file |
|            | and invalidate |
|            | the file       |
|            | handle.        |
+------------+----------------+
