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

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "os/mynewt.h"
#include "cbmem/cbmem.h"
#include "log/log.h"
#if MYNEWT_VAL(LOG_STORAGE_WATERMARK)
#include "config/config.h"
#endif

#if MYNEWT_VAL(LOG_CLI)
#include "shell/shell.h"
#endif

struct log_info g_log_info;

static STAILQ_HEAD(, log) g_log_list = STAILQ_HEAD_INITIALIZER(g_log_list);
static const char *g_log_module_list[ MYNEWT_VAL(LOG_MAX_USER_MODULES) ];
static uint8_t log_written;

#if MYNEWT_VAL(LOG_CLI)
int shell_log_dump_cmd(int, char **);
struct shell_cmd g_shell_log_cmd = {
    .sc_cmd = "log",
    .sc_cmd_func = shell_log_dump_cmd
};

#if MYNEWT_VAL(LOG_FCB_SLOT1)
int shell_log_slot1_cmd(int, char **);
struct shell_cmd g_shell_slot1_cmd = {
    .sc_cmd = "slot1",
    .sc_cmd_func = shell_log_slot1_cmd,
};
#endif

#if MYNEWT_VAL(LOG_STORAGE_INFO)
int shell_log_storage_cmd(int, char **);
struct shell_cmd g_shell_storage_cmd = {
    .sc_cmd = "log-storage",
    .sc_cmd_func = shell_log_storage_cmd,
};
#endif
#endif

#if MYNEWT_VAL(LOG_STATS)
STATS_NAME_START(logs)
  STATS_NAME(logs, writes)
  STATS_NAME(logs, drops)
  STATS_NAME(logs, errs)
  STATS_NAME(logs, lost)
  STATS_NAME(logs, too_long)
STATS_NAME_END(logs)
#endif

#if MYNEWT_VAL(LOG_STORAGE_WATERMARK)
static int log_conf_set(int argc, char **argv, char *val);

static struct conf_handler log_conf = {
    .ch_name = "log",
    .ch_get = NULL,
    .ch_set = log_conf_set,
    .ch_commit = NULL,
    .ch_export = NULL,
};

static int
log_conf_set(int argc, char **argv, char *val)
{
    struct log *cur;

    if (argc < 2) {
        return -1;
    }

    /* Only support log/<name>/mark entries for now */
    if (strcmp("mark", argv[1])) {
        return -1;
    }

    /* Find proper log */
    STAILQ_FOREACH(cur, &g_log_list, l_next) {
        if (!strcmp(argv[0], cur->l_name)) {
            break;
        }
    }

    if (!cur) {
        return -1;
    }

    /* Set watermark if supported */
    if (cur->l_log->log_set_watermark) {
        cur->l_log->log_set_watermark(cur, atoi(val));
    }

    return 0;
}
#endif

void
log_init(void)
{
    int rc;

    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    (void)rc;

    memset(g_log_module_list, 0, sizeof(g_log_module_list));
    log_written = 0;

    STAILQ_INIT(&g_log_list);
    g_log_info.li_version = MYNEWT_VAL(LOG_VERSION);
    g_log_info.li_next_index = 0;

#if MYNEWT_VAL(LOG_CLI)
    shell_cmd_register(&g_shell_log_cmd);
#if MYNEWT_VAL(LOG_FCB_SLOT1)
    shell_cmd_register(&g_shell_slot1_cmd);
#endif
#if MYNEWT_VAL(LOG_STORAGE_INFO)
    shell_cmd_register(&g_shell_storage_cmd);
#endif
#endif

#if MYNEWT_VAL(LOG_NEWTMGR)
    rc = log_nmgr_register_group();
    SYSINIT_PANIC_ASSERT(rc == 0);
#endif

#if MYNEWT_VAL(LOG_CONSOLE)
    log_console_init();
#endif

#if MYNEWT_VAL(LOG_STORAGE_WATERMARK)
    rc = conf_register(&log_conf);
    SYSINIT_PANIC_ASSERT(rc == 0);
#endif
}

struct log *
log_list_get_next(struct log *log)
{
    struct log *next;

    if (log == NULL) {
        next = STAILQ_FIRST(&g_log_list);
    } else {
        next = STAILQ_NEXT(log, l_next);
    }

    return (next);
}

uint8_t
log_module_register(uint8_t id, const char *name)
{
    uint8_t idx;

    if (id == 0) {
        /* Find free idx */
        for (idx = 0;
             idx < MYNEWT_VAL(LOG_MAX_USER_MODULES) && g_log_module_list[idx];
             idx++) {
        }

        if (idx == MYNEWT_VAL(LOG_MAX_USER_MODULES)) {
            /* No free idx */
            return 0;
        }
    } else {
        if ((id < LOG_MODULE_PERUSER) ||
                (id >= LOG_MODULE_PERUSER + MYNEWT_VAL(LOG_MAX_USER_MODULES))) {
            /* Invalid id */
            return 0;
        }

        idx = id - LOG_MODULE_PERUSER;
    }

    if (g_log_module_list[idx]) {
        /* Already registered with selected id */
        return 0;
    }

    g_log_module_list[idx] = name;

    return idx + LOG_MODULE_PERUSER;
}

const char *
log_module_get_name(uint8_t module)
{
    if (module < LOG_MODULE_PERUSER) {
        switch (module) {
        case LOG_MODULE_DEFAULT:
            return "DEFAULT";
        case LOG_MODULE_OS:
            return "OS";
        case LOG_MODULE_NEWTMGR:
            return "NEWTMGR";
        case LOG_MODULE_NIMBLE_CTLR:
            return "NIMBLE_CTLR";
        case LOG_MODULE_NIMBLE_HOST:
            return "NIMBLE_HOST";
        case LOG_MODULE_NFFS:
            return "NFFS";
        case LOG_MODULE_REBOOT:
            return "REBOOT";
        case LOG_MODULE_IOTIVITY:
            return "IOTIVITY";
        case LOG_MODULE_TEST:
            return "TEST";
        }
    } else if (module - LOG_MODULE_PERUSER < MYNEWT_VAL(LOG_MAX_USER_MODULES)) {
        return g_log_module_list[module - LOG_MODULE_PERUSER];
    }

    return NULL;
}

/**
 * Indicates whether the specified log has been regiestered.
 */
static int
log_registered(struct log *log)
{
    struct log *cur;

    STAILQ_FOREACH(cur, &g_log_list, l_next) {
        if (cur == log) {
            return 1;
        }
    }

    return 0;
}

struct log *
log_find(const char *name)
{
    struct log *log;

    log = NULL;
    do {
        log = log_list_get_next(log);
        if (strcmp(log->l_name, name) == 0) {
            break;
        }
    } while (log != NULL);

    return log;
}

struct log_read_hdr_arg {
    struct log_entry_hdr *hdr;
    int read_success;
};

static int
log_read_hdr_walk(struct log *log, struct log_offset *log_offset, void *dptr,
                  uint16_t len)
{
    struct log_read_hdr_arg *arg;
    int rc;

    arg = log_offset->lo_arg;

    rc = log_read(log, dptr, arg->hdr, 0, sizeof *arg->hdr);
    if (rc >= sizeof *arg->hdr) {
        arg->read_success = 1;
    }

    /* Abort the walk; only one header needed. */
    return 1;
}

/**
 * Reads the final log entry's header from the specified log.
 *
 * @param log                   The log to read from.
 * @param out_hdr               On success, the last entry header gets written
 *                                  here.
 *
 * @return                      0 on success; nonzero on failure.
 */
static int
log_read_last_hdr(struct log *log, struct log_entry_hdr *out_hdr)
{
    struct log_read_hdr_arg arg;
    struct log_offset log_offset;

    arg.hdr = out_hdr;
    arg.read_success = 0;

    log_offset.lo_arg = &arg;
    log_offset.lo_ts = -1;
    log_offset.lo_index = 0;
    log_offset.lo_data_len = 0;

    log_walk(log, log_read_hdr_walk, &log_offset);
    if (!arg.read_success) {
        return -1;
    }

    return 0;
}

/*
 * Associate an instantiation of a log with the logging infrastructure
 */
int
log_register(char *name, struct log *log, const struct log_handler *lh,
             void *arg, uint8_t level)
{
    struct log_entry_hdr hdr;
    int sr;
    int rc;

    assert(!log_written);

    if (level > LOG_LEVEL_MAX) {
        level = LOG_LEVEL_MAX;
    }

    log->l_name = name;
    log->l_log = lh;
    log->l_arg = arg;
    log->l_level = level;
    log->l_append_cb = NULL;
    log->l_max_entry_len = 0;

    if (!log_registered(log)) {
        STAILQ_INSERT_TAIL(&g_log_list, log, l_next);
#if MYNEWT_VAL(LOG_STATS)
        stats_init(STATS_HDR(log->l_stats),
                   STATS_SIZE_INIT_PARMS(log->l_stats, STATS_SIZE_32),
                   STATS_NAME_INIT_PARMS(logs));
        stats_register(log->l_name, STATS_HDR(log->l_stats));
#endif
    }

    /* Call registered handler now - log structure is set and put on list */
    if (log->l_log->log_registered) {
        log->l_log->log_registered(log);
    }

    /* If this is a persisted log, read the index from its most recent entry.
     * We need to ensure the index of all subseqently written entries is
     * monotonically increasing.
     */
    if (log->l_log->log_type == LOG_TYPE_STORAGE) {
        rc = log_read_last_hdr(log, &hdr);
        if (rc == 0) {
            OS_ENTER_CRITICAL(sr);
            if (hdr.ue_index >= g_log_info.li_next_index) {
                g_log_info.li_next_index = hdr.ue_index + 1;
            }
            OS_EXIT_CRITICAL(sr);
        }
    }

    return (0);
}

void
log_set_append_cb(struct log *log, log_append_cb *cb)
{
    log->l_append_cb = cb;
}

static int
log_chk_type(uint8_t etype)
{
    int rc;

    rc = OS_OK;

#if MYNEWT_VAL(LOG_VERSION) > 2
    switch(etype) {
        case LOG_ETYPE_STRING:
        case LOG_ETYPE_BINARY:
        case LOG_ETYPE_CBOR:
            break;
        default:
            rc = OS_ERROR;
            break;
    }
#endif

    return rc;
}

static int
log_chk_max_entry_len(struct log *log, uint16_t len)
{
    int rc;

    rc = OS_OK;
    if (log->l_max_entry_len != 0) {
        if (len > log->l_max_entry_len) {
            LOG_STATS_INC(log, too_long);
            rc = OS_ENOMEM;
        }
    }

    return rc;
}

static int
log_append_prepare(struct log *log, uint8_t module, uint8_t level,
                   uint8_t etype, struct log_entry_hdr *ue)
{
    int rc;
    int sr;
    struct os_timeval tv;
    uint32_t idx;

    rc = 0;

    rc = log_chk_type(etype);
    assert(rc == OS_OK);

    if (log->l_name == NULL || log->l_log == NULL) {
        rc = -1;
        goto err;
    }

    if (level > LOG_LEVEL_MAX) {
        level = LOG_LEVEL_MAX;
    }

    if (log->l_log->log_type == LOG_TYPE_STORAGE) {
        /* Remember that a log entry has been persisted since boot. */
        log_written = 1;
    }

    /*
     * If the log message is below what this log instance is
     * configured to accept, then just drop it.
     */
    if (level < log->l_level) {
        rc = -1;
        goto err;
    }

    /* Check if this module has a minimum level. */
    if (level < log_level_get(module)) {
        rc = -1;
        goto err;
    }

    OS_ENTER_CRITICAL(sr);
    idx = g_log_info.li_next_index++;
    OS_EXIT_CRITICAL(sr);

    /* Try to get UTC Time */
    rc = os_gettimeofday(&tv, NULL);
    if (rc || tv.tv_sec < UTC01_01_2016) {
        ue->ue_ts = os_get_uptime_usec();
    } else {
        ue->ue_ts = tv.tv_sec * 1000000 + tv.tv_usec;
    }

    ue->ue_level = level;
    ue->ue_module = module;
    ue->ue_index = idx;
#if MYNEWT_VAL(LOG_VERSION) > 2
    ue->ue_etype = etype;
#else
    assert(etype == LOG_ETYPE_STRING);
#endif

err:
    return (rc);
}

/**
 * Calls the given log's append callback, if it has one.
 */
static void
log_call_append_cb(struct log *log, uint32_t idx)
{
    /* Qualify this as `volatile` to prevent a race condition.  This prevents
     * the compiler from optimizing this temp variable away.  We copy the
     * original pointer value into this variable, then inspect and use the temp
     * variable.  This allows us to read the original pointer only once,
     * preventing a TOCTTOU race.
     * (This all assumes that function pointer reads and writes are atomic.)
     */
    log_append_cb * volatile cb;

    cb = log->l_append_cb;
    if (cb != NULL) {
        cb(log, idx);
    }
}

int
log_append_typed(struct log *log, uint8_t module, uint8_t level, uint8_t etype,
                 void *data, uint16_t len)
{
    struct log_entry_hdr *hdr;
    int rc;

    LOG_STATS_INC(log, writes);

    rc = log_chk_max_entry_len(log, len);
    if (rc != OS_OK) {
        goto err;
    }

    hdr = (struct log_entry_hdr *)data;
    rc = log_append_prepare(log, module, level, etype, hdr);
    if (rc != 0) {
        LOG_STATS_INC(log, drops);
        goto err;
    }

    rc = log->l_log->log_append(log, data, len + LOG_ENTRY_HDR_SIZE);
    if (rc != 0) {
        LOG_STATS_INC(log, errs);
        goto err;
    }

    log_call_append_cb(log, hdr->ue_index);

    return (0);
err:
    return (rc);
}

int
log_append_body(struct log *log, uint8_t module, uint8_t level, uint8_t etype,
                const void *body, uint16_t body_len)
{
    struct log_entry_hdr hdr;
    int rc;

    LOG_STATS_INC(log, writes);

    rc = log_chk_max_entry_len(log, body_len);
    if (rc != OS_OK) {
        return rc;
    }

    rc = log_append_prepare(log, module, level, etype, &hdr);
    if (rc != 0) {
        LOG_STATS_INC(log, drops);
        return rc;
    }

    rc = log->l_log->log_append_body(log, &hdr, body, body_len);
    if (rc != 0) {
        LOG_STATS_INC(log, errs);
        return rc;
    }

    log_call_append_cb(log, hdr.ue_index);

    return 0;
}

int
log_append_mbuf_typed_no_free(struct log *log, uint8_t module, uint8_t level,
                              uint8_t etype, struct os_mbuf **om_ptr)
{
    struct log_entry_hdr *hdr;
    struct os_mbuf *om;
    uint16_t len;
    int rc;

    /* Remove a loyer of indirection for convenience. */
    om = *om_ptr;

    LOG_STATS_INC(log, writes);
    if (!log->l_log->log_append_mbuf) {
        rc = SYS_ENOTSUP;
        goto err;
    }

    om = os_mbuf_pullup(om, sizeof(struct log_entry_hdr));
    if (!om) {
        rc = -1;
        goto err;
    }

    /*
     * Check that the log body length is less than the maximum entry. This code
     * may appear a bit odd in that it checks that the length is greater than
     * a log entry header length. The reason for this check is to ensure any
     * error handling of this case to be the same as it was before the
     * maximum entry length was checked.
     */
    len = os_mbuf_len(om);
    if (len > LOG_ENTRY_HDR_SIZE) {
        rc = log_chk_max_entry_len(log, len - LOG_ENTRY_HDR_SIZE);
        if (rc != OS_OK) {
            goto drop;
        }
    }

    hdr = (struct log_entry_hdr *)om->om_data;

    rc = log_append_prepare(log, module, level, etype, hdr);
    if (rc != 0) {
        LOG_STATS_INC(log, drops);
        goto drop;
    }

    rc = log->l_log->log_append_mbuf(log, om);
    if (rc != 0) {
        goto err;
    }

    log_call_append_cb(log, hdr->ue_index);

    *om_ptr = om;

    return 0;

err:
    LOG_STATS_INC(log, errs);
drop:
    if (om) {
        os_mbuf_free_chain(om);
        *om_ptr = NULL;
    }
    return rc;
}

int
log_append_mbuf_typed(struct log *log, uint8_t module, uint8_t level,
                      uint8_t etype, struct os_mbuf *om)
{
    int rc;

    rc = log_append_mbuf_typed_no_free(log, module, level, etype, &om);
    if (rc != 0) {
        return rc;
    }

    os_mbuf_free_chain(om);

    return 0;
}

int
log_append_mbuf_body_no_free(struct log *log, uint8_t module, uint8_t level,
                             uint8_t etype, struct os_mbuf *om)
{
    struct log_entry_hdr hdr;
    uint16_t len;
    int rc;

    LOG_STATS_INC(log, writes);
    if (!log->l_log->log_append_mbuf_body) {
        rc = SYS_ENOTSUP;
        goto err;
    }

    len = os_mbuf_len(om);
    rc = log_chk_max_entry_len(log, len);
    if (rc != OS_OK) {
        goto drop;
    }

    rc = log_append_prepare(log, module, level, etype, &hdr);
    if (rc != 0) {
        LOG_STATS_INC(log, drops);
        goto drop;
    }

    rc = log->l_log->log_append_mbuf_body(log, &hdr, om);
    if (rc != 0) {
        goto err;
    }

    log_call_append_cb(log, hdr.ue_index);

    return 0;
err:
    LOG_STATS_INC(log, errs);
drop:
    return rc;
}

int
log_append_mbuf_body(struct log *log, uint8_t module, uint8_t level,
                     uint8_t etype, struct os_mbuf *om)
{
    int rc;

    rc = log_append_mbuf_body_no_free(log, module, level, etype, om);
    os_mbuf_free_chain(om);

    return rc;
}

void
log_printf(struct log *log, uint8_t module, uint8_t level,
           const char *msg, ...)
{
    va_list args;
    char buf[LOG_PRINTF_MAX_ENTRY_LEN];
    int len;

    va_start(args, msg);
    len = vsnprintf(buf, LOG_PRINTF_MAX_ENTRY_LEN, msg, args);
    va_end(args);
    if (len >= LOG_PRINTF_MAX_ENTRY_LEN) {
        len = LOG_PRINTF_MAX_ENTRY_LEN-1;
    }

    log_append_body(log, module, level, LOG_ETYPE_STRING, buf, len);
}

int
log_walk(struct log *log, log_walk_func_t walk_func,
         struct log_offset *log_offset)
{
    int rc;

    rc = log->l_log->log_walk(log, walk_func, log_offset);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

/**
 * Argument passed to `log_walk` to perform a log body walk.  Wraps the
 * original walk argument and the body walk callback in a single object.
 */
struct log_walk_body_arg {
    /** The body walk function to call on each entry. */
    log_walk_body_func_t fn;

    /** The original argument passed to `log_walk`. */
    void *arg;
};

/**
 * Performs a body walk on a single log entry.  This function reads the entry
 * header, subtracts the header length from the total entry length, and
 * forwards the data to the body walk callback.
 */
static int
log_walk_body_fn(struct log *log, struct log_offset *log_offset, void *dptr,
                 uint16_t len)
{
    struct log_walk_body_arg *lwba;
    struct log_entry_hdr ueh;
    int rc;

    lwba = log_offset->lo_arg;

    /* Read the log entry header.  This gets passed to the body walk
     * callback.
     */
    rc = log_read_hdr(log, dptr, &ueh);
    if (rc != 0) {
        return rc;
    }
    if (log_offset->lo_index <= ueh.ue_index) {
        len -= sizeof ueh;

        /* Pass the wrapped callback argument to the body walk function. */
        log_offset->lo_arg = lwba->arg;
        rc = lwba->fn(log, log_offset, &ueh, dptr, len);

        /* Restore the original body walk argument. */
        log_offset->lo_arg = lwba;

        if (rc != 0) {
            return rc;
        }
    }

    return 0;
}

int
log_walk_body(struct log *log, log_walk_body_func_t walk_body_func,
              struct log_offset *log_offset)
{
    struct log_walk_body_arg lwba = {
        .fn = walk_body_func,
        .arg = log_offset->lo_arg,
    };
    int rc;

    log_offset->lo_arg = &lwba;
    rc = log->l_log->log_walk(log, log_walk_body_fn, log_offset);
    log_offset->lo_arg = lwba.arg;

    return rc;
}

/**
 * Reads from the specified log.
 *
 * @return                      The number of bytes read; 0 on failure.
 */
int
log_read(struct log *log, void *dptr, void *buf, uint16_t off,
         uint16_t len)
{
    int rc;

    rc = log->l_log->log_read(log, dptr, buf, off, len);

    return (rc);
}

int
log_read_hdr(struct log *log, void *dptr, struct log_entry_hdr *hdr)
{
    int bytes_read;

    bytes_read = log_read(log, dptr, hdr, 0, LOG_ENTRY_HDR_SIZE);
    if (bytes_read != LOG_ENTRY_HDR_SIZE) {
        return SYS_EIO;
    }

    return 0;
}

int
log_read_body(struct log *log, void *dptr, void *buf, uint16_t off,
              uint16_t len)
{
    return log_read(log, dptr, buf, LOG_ENTRY_HDR_SIZE + off, len);
}

int
log_read_mbuf(struct log *log, void *dptr, struct os_mbuf *om, uint16_t off,
              uint16_t len)
{
    int rc;

    if (!om || !log->l_log->log_read_mbuf) {
        return 0;
    }

    rc = log->l_log->log_read_mbuf(log, dptr, om, off, len);

    return (rc);
}

int
log_read_mbuf_body(struct log *log, void *dptr, struct os_mbuf *om,
                   uint16_t off, uint16_t len)
{
    return log_read_mbuf(log, dptr, om, LOG_ENTRY_HDR_SIZE + off, len);
}

int
log_flush(struct log *log)
{
    int rc;

    rc = log->l_log->log_flush(log);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

#if MYNEWT_VAL(LOG_STORAGE_INFO)
int
log_storage_info(struct log *log, struct log_storage_info *info)
{
    int rc;

    if (!log->l_log->log_storage_info) {
        rc = OS_ENOENT;
        goto err;
    }

    rc = log->l_log->log_storage_info(log, info);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}
#endif

#if MYNEWT_VAL(LOG_STORAGE_WATERMARK)
int
log_set_watermark(struct log *log, uint32_t index)
{
    char log_path[CONF_MAX_NAME_LEN];
    char mark_val[10]; /* fits uint32_t + \0 */
    int rc;

    if (!log->l_log->log_set_watermark) {
        rc = OS_ENOENT;
        goto err;
    }

    rc = log->l_log->log_set_watermark(log, index);
    if (rc != 0) {
        goto err;
    }

    snprintf(log_path, CONF_MAX_NAME_LEN, "log/%s/mark", log->l_name);
    log_path[CONF_MAX_NAME_LEN - 1] = '\0';

    sprintf(mark_val, "%u", (unsigned)index);

    conf_save_one(log_path, mark_val);

    return (0);
err:
    return (rc);
}
#endif

void
log_set_level(struct log *log, uint8_t level)
{
    assert(log);
    log->l_level = level;
}

uint8_t
log_get_level(const struct log *log)
{
    assert(log);
    return log->l_level;
}

void
log_set_max_entry_len(struct log *log, uint16_t max_entry_len)
{
    assert(log);
    log->l_max_entry_len = max_entry_len;
}
