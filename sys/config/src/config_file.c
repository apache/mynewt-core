/**
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

#ifdef FS_PRESENT

#include <string.h>
#include <assert.h>
#include <ctype.h>

#include <os/os.h>
#include <fs/fs.h>

#include "config/config.h"
#include "config/config_file.h"
#include "config_priv.h"

static int conf_file_load(struct conf_store *, load_cb cb, void *cb_arg);
static int conf_file_save_start(struct conf_store *cs);
static int conf_file_save(struct conf_store *cs, struct conf_handler *ch,
  char *name, char *value);
static int conf_file_save_end(struct conf_store *cs);

static struct conf_store_itf conf_file_itf = {
    .csi_load = conf_file_load,
    .csi_save_start = conf_file_save_start,
    .csi_save = conf_file_save,
    .csi_save_end = conf_file_save_end
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
    cf->cf_save_fp = NULL;
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

static int
conf_file_line(char *buf, char **namep, char **valp)
{
    char *cp;
    enum {
        FIND_NAME,
        FIND_NAME_END,
        FIND_VAL,
        FIND_VAL_END
    } state = FIND_NAME;

    for (cp = buf; *cp != '\0'; cp++) {
        switch (state) {
        case FIND_NAME:
            if (!isspace(*cp)) {
                *namep = cp;
                state = FIND_NAME_END;
            }
            break;
        case FIND_NAME_END:
            if (*cp == '=') {
                *cp = '\0';
                state = FIND_VAL;
            } else if (isspace(*cp)) {
                *cp = '\0';
            }
            break;
        case FIND_VAL:
            if (!isspace(*cp)) {
                *valp = cp;
                state = FIND_VAL_END;
            }
            break;
        case FIND_VAL_END:
            if (isspace(*cp)) {
                *cp = '\0';
            }
            break;
        }
    }
    if (state == FIND_VAL_END) {
        return 0;
    } else {
        return -1;
    }
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

    rc = fs_open(cf->cf_name, FS_ACCESS_READ, &file);
    if (rc != FS_EOK) {
        return OS_EINVAL;
    }

    loc = 0;
    while (1) {
        rc = conf_getnext_line(file, tmpbuf, sizeof(tmpbuf), &loc);
        if (loc == 0) {
            break;
        }
        if (rc < 0) {
            continue;
        }
        rc = conf_file_line(tmpbuf, &name_str, &val_str);
        if (rc != 0) {
            continue;
        }
        cb(name_str, val_str, cb_arg);
    }
    fs_close(file);
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
 * Called to save configuration.
 */
static int
conf_file_save_start(struct conf_store *cs)
{
    struct conf_file *cf = (struct conf_file *)cs;
    char name[CONF_FILE_NAME_MAX];

    assert(cf->cf_save_fp == NULL);
    conf_tmpfile(name, cf->cf_name, ".tmp");

    if (fs_open(name, FS_ACCESS_WRITE | FS_ACCESS_TRUNCATE, &cf->cf_save_fp)) {
        return OS_EINVAL;
    }

    return OS_OK;
}

static int
conf_file_save(struct conf_store *cs, struct conf_handler *ch,
  char *name, char *value)
{
    struct conf_file *cf = (struct conf_file *)cs;
    char tmpbuf[CONF_MAX_NAME_LEN + CONF_MAX_VAL_LEN + 32];
    int len;
    int off;

    len = strlen(ch->ch_name);
    memcpy(tmpbuf, ch->ch_name, len);
    off = len;
    tmpbuf[off++] = '/';

    len = strlen(name);
    memcpy(tmpbuf + off, name, len);
    off += len;
    tmpbuf[off++] = '=';

    len = strlen(value);
    memcpy(tmpbuf + off, value, len);
    off += len;
    tmpbuf[off++] = '\n';

    if (fs_write(cf->cf_save_fp, tmpbuf, off)) {
        return OS_EINVAL;
    }
    return OS_OK;
}

static int
conf_file_save_end(struct conf_store *cs)
{
    struct conf_file *cf = (struct conf_file *)cs;
    char name1[64];
    char name2[64];
    int rc;

    fs_close(cf->cf_save_fp);
    cf->cf_save_fp = NULL;

    conf_tmpfile(name1, cf->cf_name, ".tm2");
    conf_tmpfile(name2, cf->cf_name, ".tmp");

    rc = fs_rename(cf->cf_name, name1);
    if (rc && rc != FS_ENOENT) {
        return OS_EINVAL;
    }
    rc = fs_rename(name2, cf->cf_name);
    if (rc) {
        return OS_EINVAL;
    }

    fs_unlink(name1);
    return OS_OK;
}

#endif
