fs\_closedir
------------

.. code:: c

    int fs_closedir(struct fs_dir *dir)

Closes the specified directory handle.

Arguments
^^^^^^^^^

+--------------+--------------------------------------+
| *Argument*   | *Description*                        |
+==============+======================================+
| dir          | The name of the directory to close   |
+--------------+--------------------------------------+

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
