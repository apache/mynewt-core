/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "os/mynewt.h"

#if MYNEWT_VAL(CONFIG_NFFS)

#include <string.h>
#include <assert.h>

#include <fs/fs.h>

#include "config/config.h"
#include "config/config_file.h"
#include "config_priv.h"

static int conf_file_load(struct conf_store *, load_cb cb, void *cb_arg);
static int conf_file_save(struct conf_store *, const char *name,
  const char *value);

static struct conf_store_itf conf_file_itf = {
    .csi_load = conf_file_load,
    .csi_save = conf_file_save,
};

/*
 * Register a file to be a source of configuration.
 */
int
conf_file_src(struct conf_file *cf)
{
    if (!cf->cf_name) {
        return OS_INVALID_PARM;
    }
    cf->cf_store.cs_itf = &conf_file_itf;
    conf_src_register(&cf->cf_store);

    return OS_OK;
}

int
conf_file_dst(struct conf_file *cf)
{
    if (!cf->cf_name) {
        return OS_INVALID_PARM;
    }
    cf->cf_store.cs_itf = &conf_file_itf;
    conf_dst_register(&cf->cf_store);

    return OS_OK;
}

int
conf_getnext_line(struct fs_file *file, char *buf, int blen, uint32_t *loc)
{
    int rc;
    char *end;
    uint32_t len;

    rc = fs_seek(file, *loc);
    if (rc < 0) {
        *loc = 0;
        return -1;
    }
    rc = fs_read(file, blen, buf, &len);
    if (rc < 0 || len == 0) {
        *loc = 0;
        return -1;
    }
    if (len == blen) {
        len--;
    }
    buf[len] = '\0';

    end = strchr(buf, '\n');
    if (end) {
        *end = '\0';
    } else {
        end = strchr(buf, '\0');
    }
    blen = end - buf;
    *loc += (blen + 1);
    return blen;
}

/*
 * Called to load configuration items. cb must be called for every configuration
 * item found.
 */
static int
conf_file_load(struct conf_store *cs, load_cb cb, void *cb_arg)
{
    struct conf_file *cf = (struct conf_file *)cs;
    struct fs_file *file;
    uint32_t loc;
    char tmpbuf[CONF_MAX_NAME_LEN + CONF_MAX_VAL_LEN + 32];
    char *name_str;
    char *val_str;
    int rc;
    int lines;

    rc = fs_open(cf->cf_name, FS_ACCESS_READ, &file);
    if (rc != FS_EOK) {
        return OS_EINVAL;
    }

    loc = 0;
    lines = 0;
    while (1) {
        rc = conf_getnext_line(file, tmpbuf, sizeof(tmpbuf), &loc);
        if (loc == 0) {
            break;
        }
        if (rc < 0) {
            continue;
        }
        rc = conf_line_parse(tmpbuf, &name_str, &val_str);
        if (rc != 0) {
            continue;
        }
        lines++;
        cb(name_str, val_str, cb_arg);
    }
    fs_close(file);
    cf->cf_lines = lines;
    return OS_OK;
}

static void
conf_tmpfile(char *dst, const char *src, char *pfx)
{
    int len;
    int pfx_len;

    len = strlen(src);
    pfx_len = strlen(pfx);
    if (len + pfx_len >= CONF_FILE_NAME_MAX) {
        len = CONF_FILE_NAME_MAX - pfx_len - 1;
    }
    memcpy(dst, src, len);
    memcpy(dst + len, pfx, pfx_len);
    dst[len + pfx_len] = '\0';
}

/*
 * Try to compress configuration file by keeping unique names only.
 */
void
conf_file_compress(struct conf_file *cf)
{
    int rc;
    struct fs_file *rf;
    struct fs_file *wf;
    char tmp_file[CONF_FILE_NAME_MAX];
    char buf1[CONF_MAX_NAME_LEN + CONF_MAX_VAL_LEN + 32];
    char buf2[CONF_MAX_NAME_LEN + CONF_MAX_VAL_LEN + 32];
    uint32_t loc1, loc2;
    char *name1, *val1;
    char *name2, *val2;
    int copy;
    int len, len2;
    int lines;

    if (fs_open(cf->cf_name, FS_ACCESS_READ, &rf) != FS_EOK) {
        return;
    }
    conf_tmpfile(tmp_file, cf->cf_name, ".cmp");
    if (fs_open(tmp_file, FS_ACCESS_WRITE | FS_ACCESS_TRUNCATE, &wf)) {
        fs_close(rf);
        return;
    }

    loc1 = 0;
    lines = 0;
    while (1) {
        len = conf_getnext_line(rf, buf1, sizeof(buf1), &loc1);
        if (loc1 == 0 || len < 0) {
            break;
        }
        rc = conf_line_parse(buf1, &name1, &val1);
        if (rc) {
            continue;
        }
        if (!val1) {
            continue;
        }
        loc2 = loc1;
        copy = 1;
        while ((len2 = conf_getnext_line(rf, buf2, sizeof(buf2), &loc2)) > 0) {
            rc = conf_line_parse(buf2, &name2, &val2);
            if (rc) {
                continue;
            }
            if (!strcmp(name1, name2)) {
                copy = 0;
                break;
            }
        }
        if (!copy) {
            continue;
        }

        /*
         * Can't find one. Must copy.
         */
        len = conf_line_make(buf2, sizeof(buf2), name1, val1);
        if (len < 0 || len + 2 > sizeof(buf2)) {
            continue;
        }
        buf2[len++] = '\n';
        fs_write(wf, buf2, len);
	lines++;
    }
    fs_close(wf);
    fs_close(rf);
    fs_unlink(cf->cf_name);
    fs_rename(tmp_file, cf->cf_name);
    cf->cf_lines = lines;
    /*
     * XXX at conf_file_load(), look for .cmp if actual file does not
     * exist.
     */
}

/*
 * Called to save configuration.
 */
static int
conf_file_save(struct conf_store *cs, const char *name, const char *value)
{
    struct conf_file *cf = (struct conf_file *)cs;
    struct fs_file *file;
    char buf[CONF_MAX_NAME_LEN + CONF_MAX_VAL_LEN + 32];
    int len;
    int rc;

    if (!name) {
        return OS_INVALID_PARM;
    }

    if (cf->cf_maxlines && (cf->cf_lines + 1 >= cf->cf_maxlines)) {
        /*
         * Compress before config file size exceeds the max number of lines.
         */
        conf_file_compress(cf);
    }
    len = conf_line_make(buf, sizeof(buf), name, value);
    if (len < 0 || len + 2 > sizeof(buf)) {
        return OS_INVALID_PARM;
    }
    buf[len++] = '\n';

    /*
     * Open the file to add this one value.
     */
    if (fs_open(cf->cf_name, FS_ACCESS_WRITE | FS_ACCESS_APPEND, &file)) {
        return OS_EINVAL;
    }
    if (fs_write(file, buf, len)) {
        rc = OS_EINVAL;
    } else {
        rc = 0;
        cf->cf_lines++;
    }
    fs_close(file);
    return rc;
}

#endif
