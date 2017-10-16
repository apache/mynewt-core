fs\_filelen
-----------

.. code:: c

    int fs_filelen(const struct fs_file *file, uint32_t *out_len)

Retrieves the current length of the specified open file.

Arguments
^^^^^^^^^

+--------------+-----------------------------------------------------------------+
| *Argument*   | *Description*                                                   |
+==============+=================================================================+
| file         | Pointer to the file to query                                    |
+--------------+-----------------------------------------------------------------+
| out\_len     | On success, the number of bytes in the file gets written here   |
+--------------+-----------------------------------------------------------------+

Returned values
^^^^^^^^^^^^^^^

-  0 on success
-  `FS error code <fs_return_codes.html>`__ on failure

Header file
^^^^^^^^^^^

.. code:: c

    #include "fs/fs.h"

Example
^^^^^^^

.. code:: c

    int
    write_config(void)
    {
        struct fs_file *file;
        int rc;

        /* If the file doesn't exist, create it.  If it does exist, truncate it to
         * zero bytes.
         */
        rc = fs_open("/settings/config.txt", FS_ACCESS_WRITE | FS_ACCESS_TRUNCATE,
                     &file);
        if (rc == 0) {
            /* Write 5 bytes of data to the file. */
            rc = fs_write(file, "hello", 5);
            if (rc == 0) {
                /* The file should now contain exactly five bytes. */
                assert(fs_filelen(file) == 5);
            }

            /* Close the file. */
            fs_close(file);
        }

        return rc == 0 ? 0 : -1;
    }
