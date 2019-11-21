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

#include <os/mynewt.h>

#if MYNEWT_VAL(CONFIG_FCB2)

#include <fcb/fcb2.h>
#include <string.h>

#include "config/config.h"
#include "config/config_store.h"
#include "config/config_fcb2.h"
#include "config/config_generic_kv.h"
#include "config_priv.h"

#define CONF_FCB2_VERS		2

struct conf_fcb2_load_cb_arg {
    conf_store_load_cb cb;
    void *cb_arg;
};

struct conf_kv_load_cb_arg {
    const char *name;
    char *value;
    size_t len;
};

static int conf_fcb2_load(struct conf_store *, conf_store_load_cb cb,
                          void *cb_arg);
static int conf_fcb2_save(struct conf_store *, const char *name,
                          const char *value);

static struct conf_store_itf conf_fcb2_itf = {
    .csi_load = conf_fcb2_load,
    .csi_save = conf_fcb2_save,
};

int
conf_fcb2_src(struct conf_fcb2 *cf)
{
    int rc;

    cf->cf2_fcb.f_version = CONF_FCB2_VERS;
    if (cf->cf2_fcb.f_sector_cnt > 1) {
        cf->cf2_fcb.f_scratch_cnt = 1;
    } else {
        cf->cf2_fcb.f_scratch_cnt = 0;
    }
    while (1) {
        rc = fcb2_init(&cf->cf2_fcb);
        if (rc) {
            return OS_INVALID_PARM;
        }

        /*
         * Check if system was reset in middle of emptying a sector. This
         * situation is recognized by checking if the scratch block is missing.
         */
        if (cf->cf2_fcb.f_scratch_cnt &&
            fcb2_free_sector_cnt(&cf->cf2_fcb) < 1) {
            fcb2_sector_erase(&cf->cf2_fcb, cf->cf2_fcb.f_active.fe_sector);
        } else {
            break;
        }
    }

    cf->cf2_store.cs_itf = &conf_fcb2_itf;
    conf_src_register(&cf->cf2_store);

    return OS_OK;
}

int
conf_fcb2_dst(struct conf_fcb2 *cf)
{
    cf->cf2_store.cs_itf = &conf_fcb2_itf;
    conf_dst_register(&cf->cf2_store);

    return OS_OK;
}

static int
conf_fcb2_load_cb(struct fcb2_entry *loc, void *arg)
{
    struct conf_fcb2_load_cb_arg *argp;
    char buf[CONF_MAX_NAME_LEN + CONF_MAX_VAL_LEN + 32];
    char *name_str;
    char *val_str;
    int rc;
    int len;

    argp = (struct conf_fcb2_load_cb_arg *)arg;

    len = loc->fe_data_len;
    if (len >= sizeof(buf)) {
        len = sizeof(buf) - 1;
    }

    rc = fcb2_read(loc, 0, buf, len);
    if (rc) {
        return 0;
    }
    buf[len] = '\0';

    rc = conf_line_parse(buf, &name_str, &val_str);
    if (rc) {
        return 0;
    }
    argp->cb(name_str, val_str, argp->cb_arg);
    return 0;
}

static int
conf_fcb2_load(struct conf_store *cs, conf_store_load_cb cb, void *cb_arg)
{
    struct conf_fcb2 *cf = (struct conf_fcb2 *)cs;
    struct conf_fcb2_load_cb_arg arg;
    int rc;

    arg.cb = cb;
    arg.cb_arg = cb_arg;
    rc = fcb2_walk(&cf->cf2_fcb, FCB2_SECTOR_OLDEST, conf_fcb2_load_cb, &arg);
    if (rc) {
        return OS_EINVAL;
    }
    return OS_OK;
}

static int
conf_fcb2_var_read(struct fcb2_entry *loc, char *buf, char **name, char **val)
{
    int rc;

    rc = fcb2_read(loc, 0, buf, loc->fe_data_len);
    if (rc) {
        return rc;
    }
    buf[loc->fe_data_len] = '\0';
    rc = conf_line_parse(buf, name, val);
    return rc;
}

static void
conf_fcb2_compress_internal(struct fcb2 *fcb,
                            int (*copy_or_not)(const char *name, const char *val,
                                               void *cn_arg),
                            void *cn_arg)
{
    int rc;
    char buf1[CONF_MAX_NAME_LEN + CONF_MAX_VAL_LEN + 32];
    char buf2[CONF_MAX_NAME_LEN + CONF_MAX_VAL_LEN + 32];
    struct fcb2_entry loc1;
    struct fcb2_entry loc2;
    char *name1, *val1;
    char *name2, *val2;
    int copy;

    rc = fcb2_append_to_scratch(fcb);
    if (rc) {
        return; /* XXX */
    }

    loc1.fe_range = NULL;
    loc1.fe_entry_num = 0;
    while (fcb2_getnext(fcb, &loc1) == 0) {
        if (loc1.fe_sector != fcb->f_oldest_sec) {
            break;
        }
        rc = conf_fcb2_var_read(&loc1, buf1, &name1, &val1);
        if (rc) {
            continue;
        }
        if (!val1) {
            continue;
        }
        loc2 = loc1;
        copy = 1;
        while (fcb2_getnext(fcb, &loc2) == 0) {
            rc = conf_fcb2_var_read(&loc2, buf2, &name2, &val2);
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

        if (copy_or_not) {
            if (copy_or_not(name1, val1, cn_arg)) {
                /* Copy rejected */
                continue;
            }
        }
        /*
         * Can't find one. Must copy.
         */
        rc = fcb2_read(&loc1, 0, buf1, loc1.fe_data_len);
        if (rc) {
            continue;
        }
        rc = fcb2_append(fcb, loc1.fe_data_len, &loc2);
        if (rc) {
            continue;
        }
        rc = fcb2_write(&loc2, 0, buf1, loc1.fe_data_len);
        if (rc) {
            continue;
        }
        fcb2_append_finish(&loc2);
    }
    rc = fcb2_rotate(fcb);
    if (rc) {
        /* XXXX */
        ;
    }
}

static int
conf_fcb2_append(struct fcb2 *fcb, char *buf, int len)
{
    int rc;
    int i;
    struct fcb2_entry loc;

    for (i = 0; i < 10; i++) {
        rc = fcb2_append(fcb, len, &loc);
        if (rc != FCB2_ERR_NOSPACE) {
            break;
        }
        if (fcb->f_scratch_cnt == 0) {
            return OS_ENOMEM;
        }
        conf_fcb2_compress_internal(fcb, NULL, NULL);
    }
    if (rc) {
        return OS_EINVAL;
    }
    rc = fcb2_write(&loc, 0, buf, len);
    if (rc) {
        return OS_EINVAL;
    }
    fcb2_append_finish(&loc);
    return OS_OK;
}

static int
conf_fcb2_save(struct conf_store *cs, const char *name, const char *value)
{
    struct conf_fcb2 *cf = (struct conf_fcb2 *)cs;

    return conf_fcb2_kv_save(&cf->cf2_fcb, name, value);
}

void
conf_fcb2_compress(struct conf_fcb2 *cf,
                   int (*copy_or_not)(const char *name, const char *val,
                                      void *cn_arg),
                   void *cn_arg)
{
    conf_fcb2_compress_internal(&cf->cf2_fcb, copy_or_not, cn_arg);
}

static int
conf_kv_load_cb(struct fcb2_entry *loc, void *arg)
{
    struct conf_kv_load_cb_arg *cb_arg = arg;
    char buf[CONF_MAX_NAME_LEN + CONF_MAX_VAL_LEN + 32];
    char *name_str;
    char *val_str;
    int rc;
    int len;

    len = loc->fe_data_len;
    if (len >= sizeof(buf)) {
        len = sizeof(buf) - 1;
    }

    rc = fcb2_read(loc, 0, buf, len);
    if (rc) {
        return 0;
    }
    buf[len] = '\0';

    rc = conf_line_parse(buf, &name_str, &val_str);
    if (rc) {
        return 0;
    }

    if (strcmp(name_str, cb_arg->name)) {
        return 0;
    }

    strncpy(cb_arg->value, val_str, cb_arg->len);
    cb_arg->value[cb_arg->len - 1] = '\0';

    return 0;
}

int
conf_fcb2_kv_load(struct fcb2 *fcb, const char *name, char *value, size_t len)
{
    struct conf_kv_load_cb_arg arg;
    int rc;

    arg.name = name;
    arg.value = value;
    arg.len = len;

    rc = fcb2_walk(fcb, 0, conf_kv_load_cb, &arg);
    if (rc) {
        return OS_EINVAL;
    }

    return OS_OK;
}

int
conf_fcb2_kv_save(struct fcb2 *fcb, const char *name, const char *value)
{
    char buf[CONF_MAX_NAME_LEN + CONF_MAX_VAL_LEN + 32];
    int len;

    if (!name) {
        return OS_INVALID_PARM;
    }

    len = conf_line_make(buf, sizeof(buf), name, value);
    if (len < 0 || len + 2 > sizeof(buf)) {
        return OS_INVALID_PARM;
    }
    return conf_fcb2_append(fcb, buf, len);
}

#endif
