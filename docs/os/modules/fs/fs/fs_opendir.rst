fs\_opendir
-----------

.. code:: c

    int fs_opendir(const char *path, struct fs_dir **out_dir)

Opens the directory at the specified path. The directory's contents can
be read with subsequent calls to fs\_readdir(). When you are done with
the directory handle, close it with fs\_closedir().

Arguments
^^^^^^^^^

+--------------+----------------------------------------------+
| *Argument*   | *Description*                                |
+==============+==============================================+
| path         | The name of the directory to open            |
+--------------+----------------------------------------------+
| out\_dir     | On success, points to the directory handle   |
+--------------+----------------------------------------------+

Returned values
^^^^^^^^^^^^^^^

-  0 on success
-  FS\_ENOENT if the specified directory does not exist
-  Other `FS error code <fs_return_codes.html>`__ on error.

Notes
^^^^^

-  Unlinking files from the directory while it is open may result in
   unpredictable behavior during subsequent calls to ``fs_readdir()``.
   New files can be created inside the directory without causing
   problems.

-  Always close a directory when you are done reading from it. If you
   forget to close a directory, the directory stays open forever. Do
   this too many times, and the underlying file system will run out of
   directory handles, causing subsequent open operations to fail. This
   type of bug is known as a file handle leak or a file descriptor leak.

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
