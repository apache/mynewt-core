#ifndef H_FFS_
#define H_FFS_

#include <inttypes.h>

#define FFS_ACCESS_READ         0x01
#define FFS_ACCESS_WRITE        0x02
#define FFS_ACCESS_APPEND       0x04
#define FFS_ACCESS_TRUNCATE     0x08

#define FFS_FILENAME_MAX_LEN    256  /* Does not require null terminator. */

#define FFS_MAX_AREAS           256

#define FFS_EOK                 0
#define FFS_ECORRUPT            1
#define FFS_EFLASH_ERROR        2
#define FFS_ERANGE              3
#define FFS_EINVAL              4
#define FFS_ENOMEM              5
#define FFS_ENOENT              6
#define FFS_EEMPTY              7
#define FFS_EFULL               8
#define FFS_EUNEXP              9
#define FFS_EOS                 10
#define FFS_EEXIST              11
#define FFS_ERDONLY             12
#define FFS_EUNINIT             13

struct ffs_config {
    uint32_t fc_hash_size;
    uint32_t fc_num_inodes;
    uint32_t fc_num_blocks;
    uint32_t fc_num_files;
    uint32_t fc_num_cache_inodes;
    uint32_t fc_num_cache_blocks;
};

extern struct ffs_config ffs_config;

struct ffs_area_desc {
    uint32_t fad_offset;    /* Flash offset of start of area. */
    uint32_t fad_length;    /* Size of area, in bytes. */
};

struct ffs_file;

int ffs_open(const char *filename, uint8_t access_flags,
             struct ffs_file **out_file);
int ffs_close(struct ffs_file *file);
int ffs_init(void);
int ffs_detect(const struct ffs_area_desc *area_descs);
int ffs_format(const struct ffs_area_desc *area_descs);
int ffs_read(struct ffs_file *file, void *data, uint32_t *len);
int ffs_write(struct ffs_file *file, const void *data, int len);
int ffs_seek(struct ffs_file *file, uint32_t offset);
uint32_t ffs_getpos(const struct ffs_file *file);
int ffs_file_len(struct ffs_file *file, uint32_t *out_len);
int ffs_rename(const char *from, const char *to);
int ffs_unlink(const char *filename);
int ffs_mkdir(const char *path);
int ffs_ready(void);

#endif

