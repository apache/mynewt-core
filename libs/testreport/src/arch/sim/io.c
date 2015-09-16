/**
 * Copyright (c) 2015 Stack Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include "testreport_priv.h"

static char tr_io_buf[1024];

int
tr_io_write(const char *path, const void *contents, size_t len)
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
tr_io_mkdir(const char *path)
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
tr_io_rmdir(const char *path)
{
    int rc; 

    rc = snprintf(tr_io_buf, sizeof tr_io_buf,
                  "rm -rf '%s'", path);
    if (rc >= sizeof tr_io_buf) {
        return -1;
    }

    rc = system(tr_io_buf);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int
tr_io_read(const char *path, void *out_data, size_t len, size_t *out_len)
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
tr_io_delete(const char *path)
{
    int rc;

    rc = remove(path);

    return rc;
}
