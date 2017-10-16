fs\_seek
--------

.. code:: c

    int fs_seek(struct fs_file *file, uint32_t offset)

Positions a file's read and write pointer at the specified offset. The
offset is expressed as the number of bytes from the start of the file
(i.e., seeking to offset 0 places the pointer at the first byte in the
file).

Arguments
^^^^^^^^^

+--------------+--------------------------------------+
| *Argument*   | *Description*                        |
+==============+======================================+
| file         | Pointer to the file to reposition    |
+--------------+--------------------------------------+
| offset       | The 0-based file offset to seek to   |
+--------------+--------------------------------------+

Returned values
^^^^^^^^^^^^^^^

-  0 on success
-  `FS error code <fs_return_codes.html>`__ on failure

Notes
^^^^^

If a file is opened in append mode, its write pointer is always
positioned at the end of the file. Calling this function on such a file
only affects the read pointer.

Header file
^^^^^^^^^^^

.. code:: c

    #include "fs/fs.h"

Example
^^^^^^^

The following example reads four bytes from a file, starting at an
offset of eight.

.. code:: c

    int
    read_part1_middle(void)
    {
        struct fs_file *file;
        uint32_t bytes_read;
        uint8_t buf[4];
        int rc;

        rc = fs_open("/data/parts/1.bin", FS_ACCESS_READ, &file);
        if (rc == 0) {
            /* Advance to offset 8. */
            rc = fs_seek(file, 8);
            if (rc == 0) {
                /* Read bytes 8, 9, 10, and 11. */
                rc = fs_read(file, 4, buf, &bytes_read);
                if (rc == 0) {
                    /* buf now contains up to 4 bytes of file data. */
                    console_printf("read %u bytes\n", bytes_read)
                }
            }

            /* Close the file. */
            fs_close(file);
        }

        return rc == 0 ? 0 : -1;
    }
