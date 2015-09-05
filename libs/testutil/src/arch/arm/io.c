#include <stddef.h>
#include "ffs/ffs.h"
#include "ffsutil/ffsutil.h"
#include "testutil/testutil.h"

int
tu_io_write(const char *path, const uint8_t *contents, size_t len)
{
    int rc;

    rc = ffsutil_write_file(path, contents, len);
    if (rc != 0) {
        return -1;
    }

    return 0;
}

int
tu_io_mkdir(const char *path)
{
    int rc;

    rc = ffs_mkdir(path);
    if (rc != 0 && rc != FFS_EEXIST) {
        return -1;
    }

    return 0;
}

int
tu_io_rmdir(const char *path)
{
    int rc;

    rc = ffs_unlink(path);
    if (rc != 0 && rc != FFS_ENOENT) {
        return -1;
    }

    return 0;
}

int
tu_io_read(const char *path, void *out_data, size_t len, size_t *out_len)
{
    uint32_t u32;
    int rc;

    rc = ffsutil_read_file(path, 0, len, out_data, &u32);
    if (rc != 0) {
        return -1;
    }

    *out_len = u32;

    return 0;
}

int
tu_io_delete(const char *path)
{
    int rc;

    rc = ffs_unlink(path);
    if (rc != 0 && rc != FFS_ENOENT) {
        return -1;
    }

    return 0;
}
