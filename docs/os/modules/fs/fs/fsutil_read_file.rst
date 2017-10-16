fsutil\_read\_file
------------------

.. code:: c

    int fsutil_read_file(const char *path, uint32_t offset, uint32_t len,
                         void *dst, uint32_t *out_len)

Calls fs\_open(), fs\_read(), and fs\_close() to open a file at the
specified path, retrieve data from the file starting from the specified
offset, and close the file and invalidate the file handle.

Arguments
^^^^^^^^^

+--------------+----------------+
| *Argument*   | *Description*  |
+==============+================+
| path         | Pointer to the |
|              | directory      |
|              | entry to query |
+--------------+----------------+
| offset       | Position of    |
|              | the file's     |
|              | read pointer   |
+--------------+----------------+
| len          | Number of      |
|              | bytes to       |
|              | attempt to     |
|              | read           |
+--------------+----------------+
| dst          | Destination    |
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

Notes
^^^^^

This is a convenience function. It is useful when the amount of data to
be read from the file is small (i.e., all the data read can easily fit
in a single buffer).

Header file
^^^^^^^^^^^

.. code:: c

    #include "fs/fs.h"

Example
^^^^^^^

This example demonstrates reading a small text file in its entirety and
printing its contents to the console.

.. code:: c

    int
    print_status(void)
    {
        uint32_t bytes_read;
        uint8_t buf[16];
        int rc;

        /* Read up to 15 bytes from the start of the file. */
        rc = fsutil_read_file("/cfg/status.txt", 0, sizeof buf - 1, buf,
                              &bytes_read);
        if (rc != 0) return -1;

        /* Null-terminate the string just read. */
        buf[bytes_read] = '\0';

        /* Print the file contents to the console. */
        console_printf("%s\n", buf);

        return 0;
    }
