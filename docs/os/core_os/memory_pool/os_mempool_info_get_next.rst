 os\_mempool\_info\_get\_next
-----------------------------

.. code:: c

    struct os_mempool * os_mempool_info_get_next(struct os_mempool *mp, struct os_mempool_info *omi)

Populates the os\_mempool\_info structure pointed to by ``omi`` with
memory pool information. The value of ``mp`` specifies the memory pool
information to populate. If ``mp`` is **NULL**, it populates the
information for the first memory pool on the memory pool list. If ``mp``
is not NULL, it populates the information for the next memory pool after
``mp``.

 #### Arguments

+--------------+----------------+
| Arguments    | Description    |
+==============+================+
| ``mp``       | Pointer to the |
|              | memory pool in |
|              | the memory     |
|              | pool list from |
|              | the previous   |
|              | ``os_mempool_i |
|              | nfo_get_next`` |
|              | function call. |
|              | The next       |
|              | memory pool    |
|              | after ``mp``   |
|              | is populated.  |
|              | If ``mp`` is   |
|              | NULL, the      |
|              | first memory   |
|              | pool on the    |
|              | memory pool    |
|              | list is        |
|              | populated.     |
+--------------+----------------+
| ``omi``      | Pointer to     |
|              | ``os_mempool_i |
|              | nfo``          |
|              | structure      |
|              | where memory   |
|              | information    |
|              | will be        |
|              | stored.        |
+--------------+----------------+

 #### Returned values

-  A pointer to the memory pool structure that was used to populate the
   memory pool information structure.

-  NULL indicates ``mp`` is the last memory pool on the list and ``omi``
   is not populated with memory pool information.

 #### Example

.. code:: c


    shell_os_mpool_display_cmd(int argc, char **argv)
    {
        struct os_mempool *mp;
        struct os_mempool_info omi;
        char *name;

        name = NULL;
        found = 0;

        if (argc > 1 && strcmp(argv[1], "")) {
            name = argv[1];                  
        }
        console_printf("Mempools: \n");
        mp = NULL;
        console_printf("%32s %5s %4s %4s %4s\n", "name", "blksz", "cnt", "free",
                       "min");
        while (1) {
            mp = o{_mempool_info_get_next(mp, &omi);
            if (mp == NULL) {
                break;      
            }
            if (name) {
                if (strcmp(name, omi.omi_name)) {
                    continue;
                } else {
                    found = 1;
                }
            }

            console_printf("%32s %5d %4d %4d %4d\n", omi.omi_name,
                           omi.omi_block_size, omi.omi_num_blocks,
                           omi.omi_num_free, omi.omi_min_free);
        }

        if (name && !found) {
            console_printf("Couldn't find a memory pool with name %s\n",
                    name);
        }
        return (0);
    }

