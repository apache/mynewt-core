fs\_rename
----------

.. code:: c

    int fs_rename(const char *from, const char *to)

Performs a rename and / or move of the specified source path to the
specified destination. The source path can refer to a file or a
directory.

Arguments
^^^^^^^^^

+--------------+------------------------+
| *Argument*   | *Description*          |
+==============+========================+
| from         | The source path        |
+--------------+------------------------+
| to           | The destination path   |
+--------------+------------------------+

Returned values
^^^^^^^^^^^^^^^

-  0 on success
-  `FS error code <fs_return_codes.html>`__ on failure

Notes
^^^^^

The source path can refer to either a file or a directory. All
intermediate directories in the destination path must already exist. If
the source path refers to a file, the destination path must contain a
full filename path, rather than just the new parent directory. If an
object already exists at the specified destination path, this function
causes it to be unlinked prior to the rename (i.e., the destination gets
clobbered).

Header file
^^^^^^^^^^^

.. code:: c

    #include "fs/fs.h"

Example
^^^^^^^

This example demonstrates how to use fs\_rename() to perform a log
rotation. In this example, there is one primary log and three archived
logs. ``FS_ENOENT`` errors returned by ``fs_rename()`` are ignored; it
is not an error if an archived log was never created.

.. code:: c

    int
    rotate_logs(void)
    {
        struct fs_file *file;
        int rc;

        /* Rotate each of the log files. */
        rc = fs_rename("/var/log/messages.2", "/var/log/messages.3")
        if (rc != 0 && rc != FS_ENOENT) return -1;

        rc = fs_rename("/var/log/messages.1", "/var/log/messages.2")
        if (rc != 0 && rc != FS_ENOENT) return -1;

        rc = fs_rename("/var/log/messages.0", "/var/log/messages.1")
        if (rc != 0 && rc != FS_ENOENT) return -1;

        rc = fs_rename("/var/log/messages", "/var/log/messages.0")
        if (rc != 0 && rc != FS_ENOENT) return -1;

        /* Now create the new log file. */
        rc = fs_open("/var/log/messages", FS_ACCESS_WRITE | FS_ACCESS_TRUNCATE,
                     &file);
        if (rc != 0) return -1;

        rc = fs_write(file, "Creating new log file.\n", 23);
        fs_close(file);

        return rc == 0 ? 0 : -1;
    }
