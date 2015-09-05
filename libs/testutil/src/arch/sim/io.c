#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include "testutil/testutil.h"

static char tu_io_buf[1024];

int
tu_io_write(const char *path, const void *contents, size_t len)
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

int
tu_io_mkdir(const char *path)
{
    int rc;

    rc = mkdir(path, 0755);
    if (rc == -1 && errno != EEXIST) {
        return -1;
    }

    return 0;
}

/* XXX security risk, not portable, blah blah blah */
int
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
tu_io_read(const char *path, void *out_data, size_t len, size_t *out_len)
{
    FILE *fp;
    uint8_t *dst;
    int rc;
    int i;

    fp = NULL;

    fp = fopen(path, "rb");
    if (fp == NULL) {
        rc = -1;
        goto done;
    }

    dst = out_data;
    for (i = 0; i < len; i++) {
        rc = getc(fp);
        if (rc == EOF) {
            rc = -1;
            goto done;
        }

        dst[i] = rc;
    }

    *out_len = i;
    rc = 0;

done:
    if (fp != NULL) {
        fclose(fp);
    }

    return rc;
}

int
tu_io_delete(const char *path)
{
    int rc;

    rc = remove(path);

    return rc;
}
