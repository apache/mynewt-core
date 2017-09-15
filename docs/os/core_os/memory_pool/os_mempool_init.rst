 os\_mempool\_init
------------------

.. code:: c

    os_error_t os_mempool_init(struct os_mempool *mp, int blocks, int block_size, void *membuf, char *name)

Initializes the memory pool. Memory pointed to by *membuf* is divided
into *blocks* number of elements of size OS\_ALIGN(\ *block\_size*). The
*name* is optional, and names the memory pool.

It is assumed that the amount of memory pointed by *membuf* has at least
*OS\_MEMPOOL\_BYTES(blocks, block\_size)* number of bytes.

*name* is not copied, so caller should make sure that the memory does
not get reused.

Arguments
^^^^^^^^^

+---------------+-------------------------------------------------+
| Arguments     | Description                                     |
+===============+=================================================+
| mp            | Memory pool being initialized                   |
+---------------+-------------------------------------------------+
| blocks        | Number of elements in the pool                  |
+---------------+-------------------------------------------------+
| block\_size   | Minimum size of an individual element in pool   |
+---------------+-------------------------------------------------+
| membuf        | Backing store for the memory pool elements      |
+---------------+-------------------------------------------------+
| name          | Name of the memory pool                         |
+---------------+-------------------------------------------------+

Returned values
^^^^^^^^^^^^^^^

| OS\_OK: operation was successful.
| OS\_INVALID\_PARAM: invalid parameters. Block count or block size was
  negative, or membuf or mp was NULL.
| OS\_MEM\_NOT\_ALIGNED: membuf was not aligned on correct byte
  boundary.

Notes
^^^^^

Note that os\_mempool\_init() does not allocate backing storage;
*membuf* has to be allocated by the caller.

It's recommended that you use *OS\_MEMPOOL\_BYTES()* or
*OS\_MEMPOOL\_SIZE()* to figure out how much memory to allocate for the
pool.

Example
^^^^^^^

.. code:: c

        void *nffs_file_mem;
       
        nffs_file_mem = malloc(OS_MEMPOOL_BYTES(nffs_config.nc_num_files, sizeof (struct nffs_file)));
                                                  
        rc = os_mempool_init(&nffs_file_pool, nffs_config.nc_num_files,
                             sizeof (struct nffs_file), nffs_file_mem,
                             "nffs_file_pool");
        if (rc != 0) {
            /* Memory pool initialization failure */
        }

