#ifndef H_FFS_
#define H_FFS_

#include <inttypes.h>

#define FFS_ACCESS_READ         0x01
#define FFS_ACCESS_WRITE        0x02
#define FFS_ACCESS_APPEND       0x04
#define FFS_ACCESS_TRUNCATE     0x08

#define FFS_FILENAME_MAX_LEN    256  /* Does not include null terminator. */

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

struct ffs_sector_desc {
    uint32_t fsd_offset;
    uint32_t fsd_length;
};

struct ffs_file;

int ffs_open(struct ffs_file **out_file, const char *filename,
             uint8_t access_flags);
int
ffs_close(struct ffs_file *file);
int
ffs_init(void);
int ffs_detect(const struct ffs_sector_desc *sector_descs);
int ffs_format(const struct ffs_sector_desc *sector_descs);
int
ffs_read(struct ffs_file *file, void *data, uint32_t *len);
int
ffs_write(struct ffs_file *file, const void *data, int len);
int
ffs_seek(struct ffs_file *file, uint32_t offset);
uint32_t ffs_getpos(const struct ffs_file *file);
uint32_t ffs_file_len(const struct ffs_file *file);
int
ffs_rename(const char *from, const char *to);
int ffs_unlink(const char *filename);
int ffs_mkdir(const char *path);

#endif

