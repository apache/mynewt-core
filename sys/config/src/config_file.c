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

#include <fs/fs.h>
#include <json/json.h>

#include "config/config.h"
#include "config/config_file.h"
#include "config_priv.h"

static int conf_file_load(struct conf_store *, load_cb cb, void *cb_arg);
static int conf_file_save(struct conf_store *, char *name, char *value);

static struct conf_store_itf conf_file_itf = {
    .csi_load = conf_file_load,
    .csi_save = conf_file_save
};

struct conf_file_jbuf {
    struct json_buffer jb;
    int off;
    int len;
    char *buf;
};

/*
 * Register a file to be a source of configuration.
 */
int
conf_file_register(struct conf_file *cf)
{
    if (!cf->cf_name) {
        return -1;
    }
    cf->cf_store.cs_itf = &conf_file_itf;
    conf_src_register(&cf->cf_store);
    return 0;
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
        return -1;
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
    return rc;
}

/*
 * Called to save a configuration item. Needs to override previous value for
 * variable identified by 'name'.
 */
static int
conf_file_save(struct conf_store *cs, char *name, char *value)
{
    return -1;
}

#endif
