struct nffs\_config
-------------------

.. code:: c

    struct nffs_config {
        /** Maximum number of inodes; default=1024. */
        uint32_t nc_num_inodes;

        /** Maximum number of data blocks; default=4096. */
        uint32_t nc_num_blocks;

        /** Maximum number of open files; default=4. */
        uint32_t nc_num_files;

        /** Inode cache size; default=4. */
        uint32_t nc_num_cache_inodes;

        /** Data block cache size; default=64. */
        uint32_t nc_num_cache_blocks;
    };

The file system is configured by populating fields in a global
``struct nffs_config`` instance. Each field in the structure corresponds
to a setting. All configuration must be done prior to calling
nffs\_init().

Any fields that are set to 0 (or not set at all) inherit the
corresponding default value. This means that it is impossible to
configure any setting with a value of zero.

Notes
^^^^^

The global ``struct nffs_config`` instance is exposed in ``nffs/nffs.h``
as follows:

.. code:: c

    extern struct nffs_config nffs_config;

Header file
^^^^^^^^^^^

.. code:: c

    #include "nffs/nffs.h"
