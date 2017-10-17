Other File Systems
==================

Libraries use Mynewt's file system abstraction layer (``fs/fs``) for all
file operations. Because clients use an abstraction layer, the
underlying file system can be swapped out without affecting client code.
This page documents the procedure for plugging a custom file system into
the Mynewt file system abstraction layer.

1. Specify ``fs/fs`` as a dependency of your file system package.
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The file system package must register itself with the ``fs/fs`` package,
so it must specify ``fs/fs`` as a dependency. As an example, part of the
Newtron Flash File System (nffs) ``pkg.yml`` is reproduced below. Notice
the first item in the ``pkg.deps`` list.

.. code-block:: console

    pkg.name: fs/nffs
    pkg.deps:
        - fs/fs
        - hw/hal
        - libs/os
        - libs/testutil
        - sys/log

2. Register your package's API with the ``fs/fs`` interface.
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``fs/fs`` package calls into the underlying file system via a
collection of function pointers. To plug your file system into the
``fs/fs`` API, you must assign these function pointers to the
corresponding routines in your file system package.

For example, ``nffs`` registers itself with ``fs/fs`` as follows (from
``fs/nffs/src/nffs.c``):

.. code:: c

    static const struct fs_ops nffs_ops = {
        .f_open = nffs_open,
        .f_close = nffs_close,
        .f_read = nffs_read,
        .f_write = nffs_write,

        .f_seek = nffs_seek,
        .f_getpos = nffs_getpos,
        .f_filelen = nffs_file_len,

        .f_unlink = nffs_unlink,
        .f_rename = nffs_rename,
        .f_mkdir = nffs_mkdir,

        .f_opendir = nffs_opendir,
        .f_readdir = nffs_readdir,
        .f_closedir = nffs_closedir,

        .f_dirent_name = nffs_dirent_name,
        .f_dirent_is_dir = nffs_dirent_is_dir,

        .f_name = "nffs"
    };

    int
    nffs_init(void)
    {
        /* [...] */
        fs_register(&nffs_ops);
    }

Header Files
~~~~~~~~~~~~

To gain access to ``fs/fs``'s registration interface, include the
following header:

.. code:: c

    #include "fs/fs_if.h"
