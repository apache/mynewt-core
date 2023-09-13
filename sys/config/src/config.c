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
#include <stdlib.h>
#include <float.h>

#include "os/mynewt.h"
#include "base64/base64.h"

#include "config/config.h"
#include "config_priv.h"

struct conf_handler_head conf_handlers;

static struct os_mutex conf_mtx;

#if MYNEWT_VAL(OS_SCHEDULING)
static os_event_fn conf_ev_fn_load;

/* OS event - causes persisted config values to be loaded at startup. */
static struct os_event conf_ev_load = {
    .ev_cb = conf_ev_fn_load,
};
#endif

void
conf_init(void)
{
    int rc;

    os_mutex_init(&conf_mtx);

    SLIST_INIT(&conf_handlers);
    conf_store_init();

    (void)rc;

#if MYNEWT_VAL(CONFIG_CLI)
    rc = conf_cli_register();
    SYSINIT_PANIC_ASSERT(rc == 0);
#endif
#if MYNEWT_VAL(CONFIG_MGMT)
    rc = conf_mgmt_register();
    SYSINIT_PANIC_ASSERT(rc == 0);
#endif

    /* Delay loading the configuration until the default event queue is
     * processed.  This gives main() a chance to configure the underlying
     * storage first.
     */
#if MYNEWT_VAL(OS_SCHEDULING)
    os_eventq_put(os_eventq_dflt_get(), &conf_ev_load);
#endif
}

void
conf_lock(void)
{
    os_mutex_pend(&conf_mtx, 0xFFFFFFFF);
}

void
conf_unlock(void)
{
    os_mutex_release(&conf_mtx);
}

int
conf_register(struct conf_handler *handler)
{
    conf_lock();
    SLIST_INSERT_HEAD(&conf_handlers, handler, ch_list);
    conf_unlock();
    return 0;
}

#if MYNEWT_VAL(OS_SCHEDULING)
static void
conf_ev_fn_load(struct os_event *ev)
{
    conf_ensure_loaded();
}
#endif

/*
 * Find conf_handler based on name.
 */
struct conf_handler *
conf_handler_lookup(char *name)
{
    struct conf_handler *ch;

    SLIST_FOREACH(ch, &conf_handlers, ch_list) {
        if (!strcmp(name, ch->ch_name)) {
            return ch;
        }
    }
    return NULL;
}

/*
 * Separate string into argv array.
 */
int
conf_parse_name(char *name, int *name_argc, char *name_argv[])
{
    char *tok;
    char *tok_ptr;
    char *sep = CONF_NAME_SEPARATOR;
    int i;

    tok = strtok_r(name, sep, &tok_ptr);

    i = 0;
    while (tok) {
        name_argv[i++] = tok;
        tok = strtok_r(NULL, sep, &tok_ptr);
    }
    *name_argc = i;

    return 0;
}

struct conf_handler *
conf_parse_and_lookup(char *name, int *name_argc, char *name_argv[])
{
    int rc;

    rc = conf_parse_name(name, name_argc, name_argv);
    if (rc) {
        return NULL;
    }
    return conf_handler_lookup(name_argv[0]);
}

int
conf_value_from_str(char *val_str, enum conf_type type, void *vp, int maxlen)
{
    int64_t val;
    uint64_t uval;
    float fval;
    char *eptr;

    if (!val_str) {
        goto err;
    }
    switch (type) {
    case CONF_BOOL:
    case CONF_INT8:
    case CONF_INT16:
    case CONF_INT32:
    case CONF_INT64:
        val = strtoll(val_str, &eptr, 0);
        if (*eptr != '\0') {
            goto err;
        }
        if (type == CONF_BOOL) {
            if (val < 0 || val > 1) {
                goto err;
            }
            *(bool *)vp = val;
        } else if (type == CONF_INT8) {
            if (val < INT8_MIN || val > INT8_MAX) {
                goto err;
            }
            *(int8_t *)vp = val;
        } else if (type == CONF_INT16) {
            if (val < INT16_MIN || val > INT16_MAX) {
                goto err;
            }
            *(int16_t *)vp = val;
        } else if (type == CONF_INT32) {
            if (val < INT32_MIN || val > INT32_MAX) {
                goto err;
            }
            *(int32_t *)vp = val;
        } else {
            *(int64_t *)vp = val;
        }
        break;
    case CONF_UINT8:
    case CONF_UINT16:
    case CONF_UINT32:
    case CONF_UINT64:
        uval = strtoull(val_str, &eptr, 0);
        if (*eptr != '\0') {
            goto err;
        }
        if (type == CONF_UINT8) {
            if (uval > UINT8_MAX) {
                goto err;
            }
            *(uint8_t *)vp = uval;
        } else if (type == CONF_UINT16) {
            if (uval > UINT16_MAX) {
                goto err;
            }
            *(uint16_t *)vp = uval;
        } else if (type == CONF_UINT32) {
            if (uval > UINT32_MAX) {
                goto err;
            }
            *(uint32_t *)vp = uval;
        } else {
            *(uint64_t *)vp = uval;
        }
        break;
    case CONF_FLOAT:
        fval = strtof(val_str, &eptr);
        if (*eptr != '\0') {
            goto err;
        }
        *(float *)vp = fval;
        break;
    case CONF_STRING:
        val = strlen(val_str);
        if (val + 1 > maxlen) {
            goto err;
        }
        strcpy(vp, val_str);
        break;
    default:
        goto err;
    }
    return 0;
err:
    return OS_INVALID_PARM;
}

int
conf_bytes_from_str(char *val_str, void *vp, int *len)
{
    int tmp;

    if (base64_decode_len(val_str) > *len) {
        return OS_INVALID_PARM;
    }
    tmp = base64_decode(val_str, vp);
    if (tmp < 0) {
        return OS_INVALID_PARM;
    }
    *len = tmp;
    return 0;
}

char *
conf_str_from_value(enum conf_type type, void *vp, char *buf, int buf_len)
{
    int64_t val;
    uint64_t uval;
    float fval;

    if (type == CONF_STRING) {
        return vp;
    }
    switch (type) {
    case CONF_BOOL:
    case CONF_INT8:
    case CONF_INT16:
    case CONF_INT32:
    case CONF_INT64:
        if (type == CONF_BOOL) {
            val = *(bool *)vp;
        } else if (type == CONF_INT8) {
            val = *(int8_t *)vp;
        } else if (type == CONF_INT16) {
            val = *(int16_t *)vp;
        } else if (type == CONF_INT32) {
            val = *(int32_t *)vp;
        } else {
            val = *(int64_t *)vp;
        }
        snprintf(buf, buf_len, "%lld", val);
        return buf;
    case CONF_UINT8:
    case CONF_UINT16:
    case CONF_UINT32:
    case CONF_UINT64:
        if (type == CONF_UINT8) {
            uval = *(uint8_t *)vp;
        } else if (type == CONF_UINT16) {
            uval = *(uint16_t *)vp;
        } else if (type == CONF_UINT32) {
            uval = *(uint32_t *)vp;
        } else {
            uval = *(uint64_t *)vp;
        }
        snprintf(buf, buf_len, "%llu", uval);
        return buf;
    case CONF_FLOAT:
        fval = *(float *)vp;
        snprintf(buf, buf_len, "%f", fval);
        return buf;
    default:
        return NULL;
    }
}

char *
conf_str_from_bytes(void *vp, int vp_len, char *buf, int buf_len)
{
    if (BASE64_ENCODE_SIZE(vp_len) > buf_len) {
        return NULL;
    }
    base64_encode(vp, vp_len, buf, 1);
    return buf;
}

/**
 * Executes a conf_handler's "get" callback and returns the result.
 */
static char *
conf_get_cb(struct conf_handler *ch, int argc, char **argv, char *val,
            int val_len_max)
{
    if (ch->ch_ext) {
        if (ch->ch_get_ext != NULL) {
            return ch->ch_get_ext(argc, argv, val, val_len_max, ch->ch_arg);
        }
    } else {
        if (ch->ch_get != NULL) {
            return ch->ch_get(argc, argv, val, val_len_max);
        }
    }

    return NULL;
}

/**
 * Executes a conf_handler's "set" callback and returns the result.
 */
static int
conf_set_cb(struct conf_handler *ch, int argc, char **argv, char *val)
{
    if (ch->ch_ext) {
        if (ch->ch_set_ext != NULL) {
            return ch->ch_set_ext(argc, argv, val, ch->ch_arg);
        }
    } else {
        if (ch->ch_set != NULL) {
            return ch->ch_set(argc, argv, val);
        }
    }

    return OS_ERROR;
}

/**
 * Executes a conf_handler's "commit" callback and returns the result.
 */
static int
conf_commit_cb(struct conf_handler *ch)
{
    if (ch->ch_ext) {
        if (ch->ch_commit_ext != NULL) {
            return ch->ch_commit_ext(ch->ch_arg);
        }
    } else {
        if (ch->ch_commit != NULL) {
            return ch->ch_commit();
        }
    }

    return 0;
}

/**
 * Executes a conf_handler's "export" callback and returns the result.
 */
int
conf_export_cb(struct conf_handler *ch, conf_export_func_t export_func,
               conf_export_tgt_t tgt)
{
    if (ch->ch_ext) {
        if (ch->ch_export_ext != NULL) {
            return ch->ch_export_ext(export_func, tgt, ch->ch_arg);
        }
    } else {
        if (ch->ch_export != NULL) {
            return ch->ch_export(export_func, tgt);
        }
    }

    return 0;
}

int
conf_set_value(char *name, char *val_str)
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

    rc = conf_set_cb(ch, name_argc - 1, &name_argv[1], val_str);

out:
    conf_unlock();
    return rc;
}

/*
 * Get value in printable string form. If value is not string, the value
 * will be filled in *buf.
 * Return value will be pointer to beginning of that buffer,
 * except for string it will pointer to beginning of string.
 */
char *
conf_get_value(char *name, char *buf, int buf_len)
{
    int name_argc;
    char *name_argv[CONF_MAX_DIR_DEPTH];
    struct conf_handler *ch;
    char *rval = NULL;

    conf_lock();
    ch = conf_parse_and_lookup(name, &name_argc, name_argv);
    if (!ch) {
        goto out;
    }

    rval = conf_get_cb(ch, name_argc - 1, &name_argv[1], buf, buf_len);

out:
    conf_unlock();
    return rval;
}


int
conf_commit(char *name)
{
    int name_argc;
    char *name_argv[CONF_MAX_DIR_DEPTH];
    struct conf_handler *ch;
    int rc;
    int rc2;

    conf_lock();
    if (name) {
        ch = conf_parse_and_lookup(name, &name_argc, name_argv);
        if (!ch) {
            rc = OS_INVALID_PARM;
            goto out;
        }
        rc = conf_commit_cb(ch);
    } else {
        rc = 0;
        SLIST_FOREACH(ch, &conf_handlers, ch_list) {
            if (ch->ch_commit) {
                rc2 = conf_commit_cb(ch);
                if (!rc) {
                    rc = rc2;
                }
            }
        }
    }
out:
    conf_unlock();
    return rc;
}
