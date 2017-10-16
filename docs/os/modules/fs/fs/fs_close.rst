fs\_close
---------

.. code:: c

    int fs_close(struct fs_file *file)

Closes the specified file and invalidates the file handle.

Arguments
^^^^^^^^^

+--------------+--------------------------------+
| *Argument*   | *Description*                  |
+==============+================================+
| file         | Pointer to the file to close   |
+--------------+--------------------------------+

Returned values
^^^^^^^^^^^^^^^

-  0 on success
-  `FS error code <fs_return_codes.html>`__ on failure

Notes
^^^^^

If the file has already been unlinked, and the file has no other open
handles, the ``fs_close()`` function causes the file to be deleted from
the disk.

Header file
^^^^^^^^^^^

.. code:: c

    #include "fs/fs.h"

Example
^^^^^^^

The below code opens the file ``/settings/config.txt`` for reading,
reads some data, and then closes the file.

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
