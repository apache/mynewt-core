File System Abstraction
=======================

.. toctree::
   :hidden:

   nffs
   fatfs
   otherfs
   fs_add

Mynewt provides a file system abstraction layer (``fs/fs``) to allow
client code to be file system agnostic. By accessing the file system via
the ``fs/fs`` API, client code can perform file system operations
without being tied to a particular implementation. When possible,
library code should use the ``fs/fs`` API rather than accessing the
underlying file system directly.

.. contents::
  :local:
  :depth: 2

Description
~~~~~~~~~~~

Applications should aim to minimize the amount of code which depends on
a particular file system implementation. When possible, only depend on
the ``fs/fs`` package. In terms of the Mynewt hierarchy, an **app**
package must depend on a specific file system package, while **library**
packages should only depend on ``fs/fs``.

Applications wanting to access a filesystem are required to include the
necessary packages in their applications pkg.yml file. In the following
example, the :doc:`Newtron Flash File System <nffs>` is
used.

.. code-block:: console

    # repos/apache-mynewt-core/apps/slinky/pkg.yml

    pkg.name: repos/apache-mynewt-core/apps/slinky
    pkg.deps:
        - "@apache-mynewt-core/fs/fs"         # include the file operations interfaces
        - "@apache-mynewt-core/fs/nffs"       # include the NFFS filesystem implementation

::

    # repos/apache-mynewt-core/apps/slinky/syscfg.yml
    # [...]
     # Package: apps/<example app>
    # [...]
        CONFIG_NFFS: 1  # initialize and configure NFFS into the system
    #   NFFS_DETECT_FAIL: 1   # Ignore NFFS detection issues 
    #   NFFS_DETECT_FAIL: 2   # Format a new NFFS file system on failure to detect

    # [...]

Consult the ``nffs`` :doc:`documentation <nffs>` for a more
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
        - "@apache-mynewt-core/fs/fs"

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

Examples
~~~~~~~~

Example 1 below opens the file /settings/config.txt for reading, reads some data, and then closes the file.

.. code:: c

    int
    read_config(void)
    {
        struct fs_file *file;
        uint32_t bytes_read;
        uint8_t buf[16];
        int rc;
    
        /* Open the file for reading. */
        rc = fs_open("/settings/config.txt", FS_ACCESS_READ, &file);
        if (rc != 0) {
            return -1;
        }

        /* Read up to 16 bytes from the file. */
        rc = fs_read(file, sizeof buf, buf, &bytes_read);
        if (rc == 0) {
            /* buf now contains up to 16 bytes of file data. */
            console_printf("read %u bytes\n", bytes_read)
        }
    
        /* Close the file. */
        fs_close(file);
    
        return rc == 0 ? 0 : -1;
    }

Example 2 below  iterates through the contents of a directory, printing the name of each child node. When the traversal is complete, the code closes the directory handle.

.. code:: c

    int
    traverse_dir(const char *dirname)
    {
        struct fs_dirent *dirent;
        struct fs_dir *dir;
        char buf[64];
        uint8_t name_len;
        int rc;
    
        rc = fs_opendir(dirname, &dir);
        if (rc != 0) {
            return -1;
        }
    
        /* Iterate through the parent directory, printing the name of each child
         * entry.  The loop only terminates via a function return.
         */
        while (1) {
            /* Retrieve the next child node. */
            rc = fs_readdir(dir, &dirent); 
            if (rc == FS_ENOENT) {
                /* Traversal complete. */
                return 0;
            } else if (rc != 0) {
                /* Unexpected error. */
                return -1;
            }
    
            /* Read the child node's name from the file system. */
            rc = fs_dirent_name(dirent, sizeof buf, buf, &name_len);
            if (rc != 0) {
                return -1;
            }
    
            /* Print the child node's name to the console. */
            if (fs_dirent_is_dir(dirent)) {
                console_printf(" dir: ");
            } else {
                console_printf("file: ");
            }
            console_printf("%s\n", buf);
        }
    }

Example 3 below demonstrates creating a series of nested directories.

.. code:: c
    
    int
    create_path(void)
    {
        int rc;
    
        rc = fs_mkdir("/data");
        if (rc != 0) goto err;
    
        rc = fs_mkdir("/data/logs");
        if (rc != 0) goto err;
    
        rc = fs_mkdir("/data/logs/temperature");
        if (rc != 0) goto err;
    
        rc = fs_mkdir("/data/logs/temperature/current");
        if (rc != 0) goto err;
    
        return 0;
    
    err:
        /* Clean up the incomplete directory tree, if any. */
        fs_unlink("/data");
        return -1;
    }

Example 4 below demonstrates reading a small text file in its entirety and printing its contents to the console.

.. code:: c
    
    int
    print_status(void)
    {
        uint32_t bytes_read;
        uint8_t buf[16];
        int rc;
    
        /* Read up to 15 bytes from the start of the file. */
        rc = fsutil_read_file("/cfg/status.txt", 0, sizeof buf - 1, buf,
                              &bytes_read);
        if (rc != 0) return -1;
    
        /* Null-terminate the string just read. */
        buf[bytes_read] = '\0';
    
        /* Print the file contents to the console. */
        console_printf("%s\n", buf);
    
        return 0;
    }

Example 5  creates a 4-byte file.

.. code:: c
    
    int
    write_id(void)
    {
        int rc;
    
        /* Create the parent directory. */
        rc = fs_mkdir("/cfg");
        if (rc != 0 && rc != FS_EALREADY) {
            return -1;
        }
    
        /* Create a file and write four bytes to it. */
        rc = fsutil_write_file("/cfg/id.txt", "1234", 4);
        if (rc != 0) {
            return -1;
        }
    
        return 0;
    }

API
~~~
.. doxygenfile:: fs/fs/include/fs/fs.h
.. doxygenfile:: fs/fs/include/fs/fsutil.h

