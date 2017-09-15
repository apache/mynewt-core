OS\_MEMPOOL\_BYTES
------------------

.. code:: c

    OS_MEMPOOL_BYTES(n,blksize)

Calculates how many bytes of memory is used by *n* number of elements,
when individual element size is *blksize* bytes.

 #### Arguments

+-------------+-----------------------------------------+
| Arguments   | Description                             |
+=============+=========================================+
| n           | Number of elements                      |
+-------------+-----------------------------------------+
| blksize     | Size of an element is number of bytes   |
+-------------+-----------------------------------------+

Returned values
^^^^^^^^^^^^^^^

The number of bytes used by the memory pool.

 #### Notes OS\_MEMPOOL\_BYTES is a macro and not a function.

 #### Example

Here we allocate memory to be used as a pool.

.. code:: c

        void *nffs_file_mem;

        nffs_file_mem = malloc(OS_MEMPOOL_BYTES(nffs_config.nc_num_files, sizeof (struct nffs_file)));
        if (nffs_file_mem == NULL) {
            return FS_ENOMEM;
        }
