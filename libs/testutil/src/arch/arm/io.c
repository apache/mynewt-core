#include <stddef.h>
#include "ffs/ffs.h"
#include "ffsutil/ffsutil.h"
#include "testutil/testutil.h"

int
tu_io_write(const char *path, const uint8_t *contents, size_t len)
{
    int rc;

    if (contents != NULL && len > 0) {
        rc = ffsutil_write_file(path, contents, len);
        if (rc != 0) {
            return -1;
        }
    }

    return 0;
}

int
tu_io_mkdir(const char *path)
{
    int rc;

    rc = ffs_mkdir(path);
    if (rc == FFS_EEXIST) {
        rc = ffs_unlink(path);
        if (rc != 0) {
            return -1;
        }

        rc = ffs_mkdir(path);
    }

    if (rc != 0) {
        return -1;
    }

    return 0;
}
