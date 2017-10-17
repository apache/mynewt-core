fs\_dirent\_is\_dir
-------------------

.. code:: c

    int fs_dirent_is_dir(const struct fs_dirent *dirent)

Tells you whether the specified directory entry is a sub-directory or a
regular file.

Arguments
^^^^^^^^^

+--------------+-------------------------------------------+
| *Argument*   | *Description*                             |
+==============+===========================================+
| dirent       | Pointer to the directory entry to query   |
+--------------+-------------------------------------------+

Returned values
^^^^^^^^^^^^^^^

-  1: The entry is a directory
-  0: The entry is a regular file.

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
