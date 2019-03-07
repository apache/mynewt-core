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

#include <string.h>
#include <stdio.h>

#include "os/mynewt.h"
#include "config/config.h"
#include "config/config_store.h"
#include "config_priv.h"

struct conf_dup_check_arg {
    const char *name;
    const char *val;
    int is_dup;
};

struct conf_get_val_arg {
    const char *name;
    char val[CONF_MAX_VAL_LEN + 1];
    int seen;
};

struct conf_store_head conf_load_srcs;
struct conf_store *conf_save_dst;
static bool conf_loading;
static bool conf_loaded;

void
conf_src_register(struct conf_store *cs)
{
    struct conf_store *prev, *cur;

    prev = NULL;
    SLIST_FOREACH(cur, &conf_load_srcs, cs_next) {
        prev = cur;
    }
    if (!prev) {
        SLIST_INSERT_HEAD(&conf_load_srcs, cs, cs_next);
    } else {
        SLIST_INSERT_AFTER(prev, cs, cs_next);
    }
}

void
conf_dst_register(struct conf_store *cs)
{
    conf_save_dst = cs;
}

static void
conf_load_cb(char *name, char *val, void *cb_arg)
{
    if (!cb_arg || !strcmp((char*)cb_arg, name)) {
        /* If cb_arg is set, set specific conf value
         * If cb_arg is not set, just set the value
         * anyways
         */
        conf_set_value(name, val);
    }
}

int
conf_load_one(char *name)
{
    struct conf_store *cs;

    /*
     * for this specific config store
     *    load config
     *    apply config
     *    commit all
     */
    conf_lock();
    conf_loading = true;
    SLIST_FOREACH(cs, &conf_load_srcs, cs_next) {
        cs->cs_itf->csi_load(cs, conf_load_cb, name);
    }
    conf_loading = false;
    conf_unlock();
    return conf_commit(name);
}

int
conf_load(void)
{
    struct conf_store *cs;

    /*
     * for every config store
     *    load config
     *    apply config
     *    commit all
     */
    conf_lock();
    conf_loaded = true;
    conf_loading = true;
    SLIST_FOREACH(cs, &conf_load_srcs, cs_next) {
        cs->cs_itf->csi_load(cs, conf_load_cb, NULL);
        if (SLIST_NEXT(cs, cs_next)) {
            conf_commit(NULL);
        }
    }
    conf_loading = false;
    conf_unlock();
    return conf_commit(NULL);
}

int
conf_ensure_loaded(void)
{
    if (conf_loaded) {
        return 0;
    }

    return conf_load();
}

int
conf_set_from_storage(void)
{
    return conf_loading;
}

static void
conf_get_value_cb(char *name, char *val, void *cb_arg)
{
    struct conf_get_val_arg *cgva = (struct conf_get_val_arg *)cb_arg;

    if (strcmp(name, cgva->name)) {
        return;
    }
    cgva->seen = 1;
    if (!val) {
        cgva->val[0] = '\0';
    } else {
        strncpy(cgva->val, val, sizeof(cgva->val) - 1);
    }
}

int
conf_get_stored_value(char *name, char *buf, int buf_len)
{
    struct conf_store *cs;
    struct conf_get_val_arg cgva;
    int val_len;

    cgva.name = name;
    cgva.val[0] = '\0';
    cgva.val[sizeof(cgva.val) - 1] = '\0';
    cgva.seen = 0;

    /*
     * for every config store
     */
    conf_lock();
    SLIST_FOREACH(cs, &conf_load_srcs, cs_next) {
        cs->cs_itf->csi_load(cs, conf_get_value_cb, &cgva);
    }
    conf_unlock();

    if (!cgva.seen) {
        return OS_ENOENT;
    }
    val_len = strlen(cgva.val);
    if (buf_len < val_len) {
        return OS_EINVAL;
    }
    strcpy(buf, cgva.val);
    return 0;
}

static void
conf_dup_check_cb(char *name, char *val, void *cb_arg)
{
    struct conf_dup_check_arg *cdca = (struct conf_dup_check_arg *)cb_arg;

    if (strcmp(name, cdca->name)) {
        return;
    }
    if (!val) {
        if (!cdca->val || cdca->val[0] == '\0') {
            cdca->is_dup = 1;
        } else {
            cdca->is_dup = 0;
        }
    } else {
        if (cdca->val && !strcmp(val, cdca->val)) {
            cdca->is_dup = 1;
        } else {
            cdca->is_dup = 0;
        }
    }
}

/*
 * Append a single value to persisted config. Don't store duplicate value.
 */
int
conf_save_one(const char *name, char *value)
{
    struct conf_store *cs;
    struct conf_dup_check_arg cdca;
    int rc;

    conf_lock();
    if (!conf_save_dst) {
        rc = OS_ENOENT;
        goto out;
    }

    /*
     * Check if we're writing the same value again.
     */
    cdca.name = name;
    cdca.val = value;
    cdca.is_dup = 0;
    SLIST_FOREACH(cs, &conf_load_srcs, cs_next) {
        cs->cs_itf->csi_load(cs, conf_dup_check_cb, &cdca);
    }
    if (cdca.is_dup == 1) {
        rc = 0;
        goto out;
    }
    cs = conf_save_dst;
    rc = cs->cs_itf->csi_save(cs, name, value);
out:
    conf_unlock();
    return rc;
}

static void
conf_store_one(char *name, char *value)
{
    conf_save_one(name, value);
}

int
conf_save_tree(char *name)
{
    int name_argc;
    char *name_argv[CONF_MAX_DIR_DEPTH];
    struct conf_handler *ch;
    int rc;

    conf_lock();
    ch = conf_parse_and_lookup(name, &name_argc, name_argv);
    if (!ch) {
        rc = OS_INVALID_PARM;
        goto out;
    }
    if (ch->ch_export) {
        rc = ch->ch_export(conf_store_one, CONF_EXPORT_PERSIST);
    } else {
        rc = 0;
    }
out:
    conf_unlock();
    return rc;

}

/*
 * Walk through all registered subsystems, and ask them to export their
 * config variables. Persist these settings.
 */
int
conf_save(void)
{
    struct conf_store *cs;
    struct conf_handler *ch;
    int rc;
    int rc2;

    conf_lock();
    cs = conf_save_dst;
    if (!cs) {
        rc = OS_ENOENT;
        goto out;
    }

    if (cs->cs_itf->csi_save_start) {
        cs->cs_itf->csi_save_start(cs);
    }
    rc = 0;
    SLIST_FOREACH(ch, &conf_handlers, ch_list) {
        if (ch->ch_export) {
            rc2 = ch->ch_export(conf_store_one, CONF_EXPORT_PERSIST);
            if (!rc) {
                rc = rc2;
            }
        }
    }
    if (cs->cs_itf->csi_save_end) {
        cs->cs_itf->csi_save_end(cs);
    }
out:
    conf_unlock();
    return rc;
}

void
conf_store_init(void)
{
    conf_loaded = false;
    SLIST_INIT(&conf_load_srcs);
}
