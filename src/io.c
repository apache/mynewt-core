#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include "testutil/testutil.h"
#include "testutil_priv.h"

static char tu_io_buf[1024];

int
tu_io_write(const char *path, const uint8_t *contents, size_t len)
{
    FILE *fp;
    int rc;

    fp = NULL;

    fp = fopen(path, "w+");
    if (fp == NULL) {
        rc = -1;
        goto done;
    }

    if (contents != NULL && len > 0) {
        rc = fwrite(contents, len, 1, fp);
        if (rc != 1) {
            rc = -1;
            goto done;
        }
    }

    rc = 0;

done:
    if (fp != NULL) {
        fclose(fp);
    }

    return rc;
}

/* XXX TEMP, security risk, not portable, blah blah blah */
static int
tu_io_rmdir(const char *path)
{
    int rc; 

    rc = snprintf(tu_io_buf, sizeof tu_io_buf,
                  "rm -rf '%s'", path);
    if (rc >= sizeof tu_io_buf) {
        return -1;
    }

    rc = system(tu_io_buf);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int
tu_io_mkdir(const char *path)
{
    int rc;

    tu_io_rmdir(path);
    rc = mkdir(path, 0755);
    return rc;
}
