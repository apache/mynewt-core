fs\_read
--------

.. code:: c

    int fs_read(struct fs_file *file, uint32_t len, void *out_data, uint32_t *out_len)

Reads data from the specified file. If more data is requested than
remains in the file, all available data is retrieved and a success code
is returned.

Arguments
^^^^^^^^^

+--------------+----------------+
| *Argument*   | *Description*  |
+==============+================+
| file         | Pointer to the |
|              | the file to    |
|              | read from      |
+--------------+----------------+
| len          | The number of  |
|              | bytes to       |
|              | attempt to     |
|              | read           |
+--------------+----------------+
| out\_data    | The            |
|              | destination    |
|              | buffer to read |
|              | into           |
+--------------+----------------+
| out\_len     | On success,    |
|              | the number of  |
|              | bytes actually |
|              | read gets      |
|              | written here.  |
|              | Pass null if   |
|              | you don't      |
|              | care.          |
+--------------+----------------+

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
