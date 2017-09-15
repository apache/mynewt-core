fs\_readdir
-----------

.. code:: c

    int fs_readdir(struct fs_dir *dir, struct fs_dirent **out_dirent);

Reads the next entry in an open directory.

Arguments
^^^^^^^^^

+--------------+----------------+
| *Argument*   | *Description*  |
+==============+================+
| dir          | The directory  |
|              | handle to read |
|              | from           |
+--------------+----------------+
| out\_dirent  | On success,    |
|              | points to the  |
|              | next child     |
|              | entry in the   |
|              | specified      |
|              | directory      |
+--------------+----------------+

Returned values
^^^^^^^^^^^^^^^

-  0 on success
-  FS\_ENOENT if there are no more entries in the parent directory
-  Other `FS error code <fs_return_codes.html>`__ on error.

Header file
^^^^^^^^^^^

.. code:: c

    #include "fs/fs.h"

Example
^^^^^^^

This example iterates through the contents of a directory, printing the
name of each child node. When the traversal is complete, the code closes
the directory handle.

.. code:: c

    int
    traverse_dir(const char *dirname)
    {
        struct fs_dirent *dirent;
        struct fs_dir *dir;
        char buf[64];
        uint8_t name_len;
        int rc;

        rc = fs_opendir(dirname, &dir);
        if (rc != 0) {
            return -1;
        }

        /* Iterate through the parent directory, printing the name of each child
         * entry.  The loop only terminates via a function return.
         */
        while (1) {
            /* Retrieve the next child node. */
            rc = fs_readdir(dir, &dirent); 
            if (rc == FS_ENOENT) {
                /* Traversal complete. */
                return 0;
            } else if (rc != 0) {
                /* Unexpected error. */
                return -1;
            }

            /* Read the child node's name from the file system. */
            rc = fs_dirent_name(dirent, sizeof buf, buf, &name_len);
            if (rc != 0) {
                return -1;
            }

            /* Print the child node's name to the console. */
            if (fs_dirent_is_dir(dirent)) {
                console_printf(" dir: ");
            } else {
                console_printf("file: ");
            }
            console_printf("%s\n", buf);
        }
    }
