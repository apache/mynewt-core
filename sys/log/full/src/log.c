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

#if MYNEWT_VAL(LOG_CLI)
#include "shell/shell.h"
#endif

struct log_info g_log_info;

static STAILQ_HEAD(, log) g_log_list = STAILQ_HEAD_INITIALIZER(g_log_list);
static const char *g_log_module_list[ MYNEWT_VAL(LOG_MAX_USER_MODULES) ];
static uint8_t log_inited;
static uint8_t log_written;

#if MYNEWT_VAL(LOG_CLI)
int shell_log_dump_all_cmd(int, char **);
struct shell_cmd g_shell_log_cmd = {
    .sc_cmd = "log",
    .sc_cmd_func = shell_log_dump_all_cmd
};

#if MYNEWT_VAL(LOG_FCB_SLOT1)
int shell_log_slot1_cmd(int, char **);
struct shell_cmd g_shell_slot1_cmd = {
    .sc_cmd = "slot1",
    .sc_cmd_func = shell_log_slot1_cmd,
};
#endif
#endif

void
log_init(void)
{
    int rc;

    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    (void)rc;

    if (log_inited) {
        return;
    }
    log_inited = 1;
    log_written = 0;

    g_log_info.li_version = MYNEWT_VAL(LOG_VERSION);
    g_log_info.li_next_index = 0;

#if MYNEWT_VAL(LOG_CLI)
    shell_cmd_register(&g_shell_log_cmd);
#if MYNEWT_VAL(LOG_FCB_SLOT1)
    shell_cmd_register(&g_shell_slot1_cmd);
#endif
#endif

#if MYNEWT_VAL(LOG_NEWTMGR)
    rc = log_nmgr_register_group();
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

    log->l_name = name;
    log->l_log = lh;
    log->l_arg = arg;
    log->l_level = level;

    if (!log_registered(log)) {
        STAILQ_INSERT_TAIL(&g_log_list, log, l_next);
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

static int
log_append_prepare(struct log *log, uint8_t module, uint8_t level, uint8_t etype,
                   struct log_entry_hdr *ue)
{
    int rc;
    int sr;
    struct os_timeval tv;
    uint32_t idx;

    rc = 0;

    if (log->l_name == NULL || log->l_log == NULL) {
        rc = -1;
        goto err;
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

int
log_append_typed(struct log *log, uint8_t module, uint8_t level, uint8_t etype,
                 void *data, uint16_t len)
{
    int rc;

    rc = log_append_prepare(log, module, level, etype,
                            (struct log_entry_hdr *)data);
    if (rc != 0) {
        goto err;
    }

    rc = log->l_log->log_append(log, data, len + LOG_ENTRY_HDR_SIZE);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

int
log_append_mbuf_typed(struct log *log, uint8_t module, uint8_t level,
                      uint8_t etype, struct os_mbuf *om)
{
    int rc;

    if (!log->l_log->log_append_mbuf) {
        rc = -1;
        goto err;
    }

    om = os_mbuf_pullup(om, sizeof(struct log_entry_hdr));
    if (!om) {
        rc = -1;
        goto err;
    }

    rc = log_append_prepare(log, module, level, etype,
                            (struct log_entry_hdr *)om->om_data);
    if (rc != 0) {
        goto err;
    }

    rc = log->l_log->log_append_mbuf(log, om);
    if (rc != 0) {
        goto err;
    }

    os_mbuf_free_chain(om);

    return (0);
err:
    if (om) {
        os_mbuf_free_chain(om);
    }
    return (rc);
}

void
log_printf(struct log *log, uint16_t module, uint16_t level, char *msg, ...)
{
    va_list args;
    char buf[LOG_ENTRY_HDR_SIZE + LOG_PRINTF_MAX_ENTRY_LEN];
    int len;

    va_start(args, msg);
    len = vsnprintf(&buf[LOG_ENTRY_HDR_SIZE], LOG_PRINTF_MAX_ENTRY_LEN, msg,
            args);
    va_end(args);
    if (len >= LOG_PRINTF_MAX_ENTRY_LEN) {
        len = LOG_PRINTF_MAX_ENTRY_LEN-1;
    }

    log_append(log, module, level, (uint8_t *) buf, len);
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

int log_read_mbuf(struct log *log, void *dptr, struct os_mbuf *om, uint16_t off,
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
