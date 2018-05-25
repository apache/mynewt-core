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
#include "os/os.h"
#include "log/log.h"
#include "log/log_fcb_slot1.h"

#if MYNEWT_VAL(LOG_FCB_SLOT1)

static struct os_mutex g_log_slot1_mutex;

static bool g_slot1_locked;

/*
 * This is convenience macro to call proper log handler depending on current log
 * state.
 */
#define LOG_FCB_SLOT1_CALL(_log, _name, ...) \
        ({                                                          \
            int ret;                                                \
            struct log_fcb_slot1 *s1;                               \
            s1 = _log->l_arg;                                       \
            os_mutex_pend(&s1->mutex, OS_TIMEOUT_NEVER);            \
            if (s1->l_current) {                                    \
                ret = s1->l_current->l_log->_name(s1->l_current,    \
                                                  ## __VA_ARGS__);  \
            } else {                                                \
                ret = -OS_EBUSY;                                    \
            }                                                       \
            os_mutex_release(&s1->mutex);                           \
            ret;                                                    \
        })

static int
log_fcb_slot1_append(struct log *log, void *buf, int len)
{
    return LOG_FCB_SLOT1_CALL(log, log_append, buf, len);
}

static int
log_fcb_slot1_append_mbuf(struct log *log, struct os_mbuf *om)
{
    return LOG_FCB_SLOT1_CALL(log, log_append_mbuf, om);
}

static int
log_fcb_slot1_read(struct log *log, void *dptr, void *buf, uint16_t offset,
                   uint16_t len)
{
    return LOG_FCB_SLOT1_CALL(log, log_read, dptr, buf, offset, len);
}

static int
log_fcb_slot1_read_mbuf(struct log *log, void *dptr, struct os_mbuf *om,
                        uint16_t offset, uint16_t len)
{
    return LOG_FCB_SLOT1_CALL(log, log_read_mbuf, dptr, om, offset, len);
}

static int
log_fcb_slot1_walk(struct log *log, log_walk_func_t walk_func,
                   struct log_offset *log_offset)
{
    return LOG_FCB_SLOT1_CALL(log, log_walk, walk_func, log_offset);
}

static int
log_fcb_slot1_flush(struct log *log)
{
    return LOG_FCB_SLOT1_CALL(log, log_flush);
}

static int
log_fcb_slot1_registered(struct log *log)
{
    struct log_fcb_slot1 *s1 = log->l_arg;

    /* l_log and l_arg are set in log_fcb_slot1_init() */

    s1->l_fcb.l_name = log->l_name;
    s1->l_fcb.l_level = log->l_level;

    s1->l_cbmem.l_name = log->l_name;
    s1->l_cbmem.l_level = log->l_level;

    os_mutex_pend(&g_log_slot1_mutex, OS_TIMEOUT_NEVER);

    if (!g_slot1_locked) {
        s1->fcb_reinit_fn(s1->l_fcb.l_arg);
        s1->l_current = &s1->l_fcb;
    } else if (s1->l_cbmem.l_log) {
        s1->l_current = &s1->l_cbmem;
    } else {
        s1->l_current = NULL;
    }

    os_mutex_release(&g_log_slot1_mutex);

    return 0;
}

const struct log_handler log_fcb_slot1_handler = {
    .log_type = LOG_TYPE_STORAGE,
    .log_read = log_fcb_slot1_read,
    .log_read_mbuf = log_fcb_slot1_read_mbuf,
    .log_append = log_fcb_slot1_append,
    .log_append_mbuf = log_fcb_slot1_append_mbuf,
    .log_walk = log_fcb_slot1_walk,
    .log_flush = log_fcb_slot1_flush,
    .log_registered = log_fcb_slot1_registered,
};

int
log_fcb_slot1_init(struct log_fcb_slot1 *s1, struct fcb_log *fcb_arg,
                   struct cbmem *cbmem_arg,
                   log_fcb_slot1_reinit_fcb_fn fcb_reinit_fn)
{
    if (!fcb_arg || !fcb_reinit_fn) {
        return -1;
    }

    memset(s1, 0, sizeof(*s1));

    s1->l_fcb.l_log = &log_fcb_handler;
    s1->l_fcb.l_arg = fcb_arg;

    if (cbmem_arg) {
        s1->l_cbmem.l_log = &log_cbmem_handler;
        s1->l_cbmem.l_arg = cbmem_arg;
    }

    s1->fcb_reinit_fn = fcb_reinit_fn;

    return 0;
}

void
log_fcb_slot1_lock(void)
{
    struct log *log = NULL;
    struct log_fcb_slot1 *s1;

    if (g_slot1_locked) {
        return;
    }

    os_mutex_pend(&g_log_slot1_mutex, OS_TIMEOUT_NEVER);

    /*
     * When locking slot, we go through all slot1 handlers and:
     * - update current handler to CBMEM
     * - flush it before using
     */

    while (1) {
        log = log_list_get_next(log);
        if (log == NULL) {
            break;
        }

        if (log->l_log != &log_fcb_slot1_handler) {
            continue;
        }

        s1 = log->l_arg;

        os_mutex_pend(&s1->mutex, OS_TIMEOUT_NEVER);

        if (s1->l_cbmem.l_log) {
            s1->l_current = &s1->l_cbmem;
            log_flush(s1->l_current);
        } else {
            s1->l_current = NULL;
        }

        os_mutex_release(&s1->mutex);
    }

    g_slot1_locked = true;

    os_mutex_release(&g_log_slot1_mutex);
}

static int
walk_copy_entry(struct log *log, struct log_offset *log_offset, void *dptr,
                uint16_t len)
{
    struct os_mbuf *om;
    struct log *l_target;
    int rc;

    om = os_msys_get(len, 0);
    if (!om) {
        return -1;
    }

    rc = log_read_mbuf(log, dptr, om, 0, len);
    if (rc != len) {
        os_mbuf_free_chain(om);
        return -1;
    }

    l_target = log_offset->lo_arg;
    l_target->l_log->log_append_mbuf(l_target, om);

    os_mbuf_free_chain(om);

    return (0);
}

void
log_fcb_slot1_unlock(void)
{
    struct log *log = NULL;
    struct log_fcb_slot1 *s1;
    struct log_offset lo;
    int rc;

    if (!g_slot1_locked) {
        return;
    }

    os_mutex_pend(&g_log_slot1_mutex, OS_TIMEOUT_NEVER);

    /*
     * When unlocking slot, we go through all slot1 handlers and:
     * - ask application to reinitialize FCB
     * - copy entries from CBMEM to FCB
     * - update current log handler to FCB
     */

    while (1) {
        log = log_list_get_next(log);
        if (log == NULL) {
            break;
        }

        if (log->l_log != &log_fcb_slot1_handler) {
            continue;
        }

        s1 = log->l_arg;

        rc = s1->fcb_reinit_fn(s1->l_fcb.l_arg);
        if (rc) {
            continue;
        }

        os_mutex_pend(&s1->mutex, OS_TIMEOUT_NEVER);

        if (s1->l_current) {
            lo.lo_arg = &s1->l_fcb;
            lo.lo_ts = 0;
            lo.lo_index = 0;
            lo.lo_data_len = 0;

            log_walk(s1->l_current, walk_copy_entry, &lo);
        }

        s1->l_current = &s1->l_fcb;

        os_mutex_release(&s1->mutex);
    }

    g_slot1_locked = false;

    os_mutex_release(&g_log_slot1_mutex);
}

bool
log_fcb_slot1_is_locked(void)
{
    return g_slot1_locked;
}

void
log_init_slot1(void)
{
    os_mutex_init(&g_log_slot1_mutex);
}

#endif
