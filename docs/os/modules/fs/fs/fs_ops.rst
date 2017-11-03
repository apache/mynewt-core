struct fs\_ops
--------------

.. code:: c

    struct fs_ops {
        int (*f_open)(const char *filename, uint8_t access_flags,
                  struct fs_file **out_file);
        int (*f_close)(struct fs_file *file);
        int (*f_read)(struct fs_file *file, uint32_t len, void *out_data,
          uint32_t *out_len);
        int (*f_write)(struct fs_file *file, const void *data, int len);

        int (*f_seek)(struct fs_file *file, uint32_t offset);
        uint32_t (*f_getpos)(const struct fs_file *file);
        int (*f_filelen)(const struct fs_file *file, uint32_t *out_len);

        int (*f_unlink)(const char *filename);
        int (*f_rename)(const char *from, const char *to);
        int (*f_mkdir)(const char *path);

        int (*f_opendir)(const char *path, struct fs_dir **out_dir);
        int (*f_readdir)(struct fs_dir *dir, struct fs_dirent **out_dirent);
        int (*f_closedir)(struct fs_dir *dir);

        int (*f_dirent_name)(const struct fs_dirent *dirent, size_t max_len,
          char *out_name, uint8_t *out_name_len);
        int (*f_dirent_is_dir)(const struct fs_dirent *dirent);

        const char *f_name;
    };

This data structure consists of a set of function pointers. Each
function pointer corresponds to a file system operation. When
registering a file system with the abstraction layer, each function
pointer must be pointed at the corresponding routine in the custom file
system package.

The required behavior of each corresponding function is documented in
the `file system abstraction layer API <fs.md#api>`__.

Header file
^^^^^^^^^^^

.. code:: c

    #include "fs/fs_if.h"
