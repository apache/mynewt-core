os\_memblock\_put
-----------------

.. code:: c

    os_error_t os_memblock_put(struct os_mempool *mp, void *block_addr)

Releases previously allocated element back to the pool.

 #### Arguments

+---------------+---------------------------------------------------------+
| Arguments     | Description                                             |
+===============+=========================================================+
| mp            | Pointer to memory pool from which block was allocated   |
+---------------+---------------------------------------------------------+
| block\_addr   | Pointer to element getting freed                        |
+---------------+---------------------------------------------------------+

 #### Returned values

| OS\_OK: operation was a success:
| OS\_INVALID\_PARAM: If either mp or block\_addr were NULL, or the
  block being freed was outside the range of the memory buffer or not on
  a true block size boundary.

 #### Example

.. code:: c

        if (file != NULL) {
            rc = os_memblock_put(&nffs_file_pool, file);
            if (rc != 0) {
                /* Error freeing memory block */
            }
        }
