#include "ffs/ffs.h"
#include "ffsutil/ffsutil.h"

int
ffsutil_read_file(const char *path, uint32_t offset, uint32_t len, void *dst,
                  uint32_t *out_len)
{
    struct ffs_file *file;
    int rc;

    rc = ffs_open(path, FFS_ACCESS_READ, &file);
    if (rc != 0) {
        goto done;
    }

    rc = ffs_read(file, len, dst, out_len);
    if (rc != 0) {
        goto done;
    }

    rc = 0;

done:
    ffs_close(file);
    return rc;
}

int
ffsutil_write_file(const char *path, const void *data, uint32_t len)
{
    struct ffs_file *file;
    int rc;

    rc = ffs_open(path, FFS_ACCESS_WRITE | FFS_ACCESS_TRUNCATE, &file);
    if (rc != 0) {
        goto done;
    }

    rc = ffs_write(file, data, len);
    if (rc != 0) {
        goto done;
    }

    rc = 0;

done:
    ffs_close(file);
    return rc;
}
