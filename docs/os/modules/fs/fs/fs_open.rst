fs\_open
--------

.. code:: c

    int fs_open(const char *filename, uint8_t access_flags,
                struct fs_file **out_file)

Opens a file at the specified path. The result of opening a nonexistent
file depends on the access flags specified. All intermediate directories
must already exist.

The access flags are best understood by comparing them to their
equivalent mode strings accepted by the C standard library function
``fopen()``. The mode strings passed to ``fopen()`` map to
``fs_open()``'s access flags as follows:

.. code-block:: console

    "r"  -  FS_ACCESS_READ
    "r+" -  FS_ACCESS_READ  | FS_ACCESS_WRITE
    "w"  -  FS_ACCESS_WRITE | FS_ACCESS_TRUNCATE
    "w+" -  FS_ACCESS_READ  | FS_ACCESS_WRITE    | FS_ACCESS_TRUNCATE
    "a"  -  FS_ACCESS_WRITE | FS_ACCESS_APPEND
    "a+" -  FS_ACCESS_READ  | FS_ACCESS_WRITE    | FS_ACCESS_APPEND

Arguments
^^^^^^^^^

+-------------+----------------+
| *Argument*  | *Description*  |
+=============+================+
| filename    | Null-terminate |
|             | d              |
|             | string         |
|             | indicating the |
|             | full path of   |
|             | the file to    |
|             | open           |
+-------------+----------------+
| access\_fla | Flags          |
| gs          | controlling    |
|             | file access;   |
|             | see above      |
|             | table          |
+-------------+----------------+
| out\_file   | On success, a  |
|             | pointer to the |
|             | newly-created  |
|             | file handle    |
|             | gets written   |
|             | here           |
+-------------+----------------+

Returned values
^^^^^^^^^^^^^^^

-  0 on success
-  `FS error code <fs_return_codes.html>`__ on failure

Notes
^^^^^

-  There is no concept of current working directory. Therefore all file
   names should start with '/'.

-  Always close files when you are done using them. If you forget to
   close a file, the file stays open forever. Do this too many times,
   and the underlying file system will run out of file handles, causing
   subsequent open operations to fail. This type of bug is known as a
   file handle leak or a file descriptor leak.

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
