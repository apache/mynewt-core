 os\_memblock\_get
------------------

.. code:: c

    void *os_memblock_get(struct os_mempool *mp)

Allocate an element from the memory pool. If successful, you'll get a
pointer to allocated element. If there are no elements available, you'll
get NULL as response.

Arguments
^^^^^^^^^

+-------------+------------------------------------------------+
| Arguments   | Description                                    |
+=============+================================================+
| mp          | Pool where element is getting allocated from   |
+-------------+------------------------------------------------+

Returned values
^^^^^^^^^^^^^^^

NULL: no elements available. : pointer to allocated element.

Notes
^^^^^

Example
^^^^^^^

.. code:: c

        struct nffs_file *file;

        file = os_memblock_get(&nffs_file_pool);
        if (file != NULL) {
            memset(file, 0, sizeof *file);
        }

