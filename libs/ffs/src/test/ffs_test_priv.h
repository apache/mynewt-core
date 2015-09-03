#ifndef H_FFS_TEST_PRIV_
#define H_FFS_TEST_PRIV_

struct ffs_test_block_desc {
    const char *data;
    int data_len;
};

struct ffs_test_file_desc {
    const char *filename;
    int is_dir;
    const char *contents;
    int contents_len;
    struct ffs_test_file_desc *children;
};

int ffs_test(void);

extern const struct ffs_test_file_desc *ffs_test_system_01;
extern const struct ffs_test_file_desc *ffs_test_system_01_rm_1014_mk10;

#endif

