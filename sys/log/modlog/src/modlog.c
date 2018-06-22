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

#include <stdarg.h>
#include "os/mynewt.h"
#include "log/log.h"
#include "modlog/modlog.h"

struct modlog_mapping {
    SLIST_ENTRY(modlog_mapping) next;
    struct modlog_desc desc;
};

struct os_mempool modlog_mapping_pool;
static os_membuf_t modlog_mapping_buf[
    OS_MEMPOOL_SIZE(MYNEWT_VAL(MODLOG_MAX_MAPPINGS),
                    sizeof (struct modlog_mapping))
];

static struct os_mutex modlog_mtx;

SLIST_HEAD(modlog_list, modlog_mapping);

/** List of configured mappings; sorted by module. */
static struct modlog_list modlog_mappings =
    SLIST_HEAD_INITIALIZER(&modlog_mappings);

/**
 * Points to the first default mapping in the list.  Since 255 is the default
 * module, the default mappings are guaranteed to come last.
 */
static struct modlog_mapping *modlog_first_dflt;

static void
modlog_lock(void)
{
    int rc;

    rc = os_mutex_pend(&modlog_mtx, OS_TIMEOUT_NEVER);
    assert(rc == 0 || rc == OS_NOT_STARTED);
}

static void
modlog_unlock(void)
{
    int rc;

    rc = os_mutex_release(&modlog_mtx);
    assert(rc == 0 || rc == OS_NOT_STARTED);
}

static struct modlog_mapping *
modlog_alloc(void)
{
    struct modlog_mapping *mm;

    mm = os_memblock_get(&modlog_mapping_pool);
    if (mm != NULL) {
        *mm = (struct modlog_mapping) { 0 };
    }

    return mm;
}

static void
modlog_free(struct modlog_mapping *mm)
{
    os_memblock_put(&modlog_mapping_pool, mm);
}

static uint8_t
modlog_infer_handle(const struct modlog_mapping *mm)
{
    uintptr_t off;
    size_t elem_sz;
    int idx;

    elem_sz = sizeof modlog_mapping_buf / MYNEWT_VAL(MODLOG_MAX_MAPPINGS);

    off = (uintptr_t)mm - (uintptr_t)modlog_mapping_buf;
    idx = off / elem_sz;

    assert(idx >= 0 && idx < MYNEWT_VAL(MODLOG_MAX_MAPPINGS));
    assert(off % elem_sz == 0);

    return idx;
}

static struct modlog_mapping *
modlog_find(uint8_t handle, struct modlog_mapping **out_prev)
{
    struct modlog_mapping *prev;
    struct modlog_mapping *cur;

    prev = NULL;
    SLIST_FOREACH(cur, &modlog_mappings, next) {
        if (cur->desc.handle == handle) {
            break;
        }

        prev = cur;
    }

    if (out_prev != NULL) {
        *out_prev = prev;
    }

    return cur;
}

static struct modlog_mapping *
modlog_find_by_module(uint8_t module, struct modlog_mapping **out_prev)
{
    struct modlog_mapping *prev;
    struct modlog_mapping *cur;

    prev = NULL;
    SLIST_FOREACH(cur, &modlog_mappings, next) {
        if (cur->desc.module == module) {
            break;
        }

        if (cur->desc.module > module) {
            cur = NULL;
            break;
        }

        prev = cur;
    }

    if (out_prev != NULL) {
        *out_prev = prev;
    }

    return cur;
}

static void
modlog_insert(struct modlog_mapping *mm)
{
    struct modlog_mapping *prev;

    modlog_find_by_module(mm->desc.module, &prev);
    if (prev == NULL) {
        SLIST_INSERT_HEAD(&modlog_mappings, mm, next);
    } else {
        SLIST_INSERT_AFTER(prev, mm, next);
    }

    if (mm->desc.module == MODLOG_MODULE_DFLT) {
        modlog_first_dflt = mm;
    }
}

static void
modlog_remove(struct modlog_mapping *mm, struct modlog_mapping *prev)
{
    if (mm == modlog_first_dflt) {
        modlog_first_dflt = SLIST_NEXT(mm, next);
    }
        
    if (prev == NULL) {
        SLIST_REMOVE_HEAD(&modlog_mappings, next);
    } else {
        SLIST_NEXT(prev, next) = SLIST_NEXT(mm, next);
    }
}

static int
modlog_register_no_lock(uint8_t module, struct log *log, uint8_t min_level,
                        uint8_t *out_handle)
{
    struct modlog_mapping *mm;

    if (log == NULL) {
        return SYS_EINVAL;
    }

    mm = modlog_alloc();
    if (mm == NULL) {
        return SYS_ENOMEM;
    }

    mm->desc = (struct modlog_desc) {
        .log = log,
        .handle = modlog_infer_handle(mm),
        .module = module,
        .min_level = min_level,
    };

    modlog_insert(mm);

    if (out_handle != NULL) {
        *out_handle = mm->desc.handle;
    }

    return 0;
}

static int
modlog_delete_no_lock(uint8_t handle)
{
    struct modlog_mapping *prev;
    struct modlog_mapping *mm;

    mm = modlog_find(handle, &prev);
    if (mm == NULL) {
        return SYS_ENOENT;
    }

    modlog_remove(mm, prev);
    modlog_free(mm);

    return 0;
}

static int
modlog_append_one(struct modlog_mapping *mm, uint8_t module, uint8_t level,
                  uint8_t etype, void *data, uint16_t len)
{
    int rc;

    if (level >= mm->desc.min_level) {
        rc = log_append_typed(mm->desc.log, module,
                              level, etype, data, len);
        if (rc != 0) {
            return SYS_EIO;
        }
    }

    return 0;
}

static int
modlog_append_no_lock(uint8_t module, uint8_t level, uint8_t etype,
                      void *data, uint16_t len)
{
    struct modlog_mapping *mm;
    int rc;

    if (module == MODLOG_MODULE_DFLT) {
        return SYS_EINVAL;
    }

    mm = modlog_find_by_module(module, NULL);
    if (mm != NULL) {
        while (mm != NULL && mm->desc.module == module) {
            rc = modlog_append_one(mm, module, level, etype, data, len);
            if (rc != 0) {
                return rc;
            }

            mm = SLIST_NEXT(mm, next);
        }
        return 0;
    }

    /* No mappings match the specified module; write to the default set. */
    for (mm = modlog_first_dflt;
         mm != NULL;
         mm = SLIST_NEXT(mm, next)) {

        rc = modlog_append_one(mm, module, level, etype, data, len);
        if (rc != 0) {
            return rc;
        }
    }

    return 0;
}

static int
modlog_append_mbuf_one(struct modlog_mapping *mm, uint8_t module,
                       uint8_t level, uint8_t etype, struct os_mbuf **om)
{
    int rc;

    if (level >= mm->desc.min_level) {
        rc = log_append_mbuf_typed_no_free(mm->desc.log, module,
                                           level, etype, om);
        if (rc != 0) {
            return SYS_EIO;
        }
    }

    return 0;
}

static int
modlog_append_mbuf_no_lock(uint8_t module, uint8_t level, uint8_t etype,
                           struct os_mbuf *om)
{
    struct modlog_mapping *mm;
    bool found;
    int rc;

    found = false;
    SLIST_FOREACH(mm, &modlog_mappings, next) {
        if (mm->desc.module == module) {
            found = true;

            rc = modlog_append_mbuf_one(mm, module, level, etype, &om);
            if (rc != 0) {
                return rc;
            }
        } else if (mm->desc.module > module) {
            break;
        }
    }

    /* If no mappings match the specified module, write to the default set. */
    if (!found) {
        for (mm = modlog_first_dflt;
             mm != NULL;
             mm = SLIST_NEXT(mm, next)) {

            rc = modlog_append_mbuf_one(mm, module, level, etype, &om);
            if (rc != 0) {
                return rc;
            }
        }
    }

    os_mbuf_free_chain(om);

    return 0;
}

static int
modlog_foreach_no_lock(modlog_foreach_fn *fn, void *arg)
{
    struct modlog_mapping *next;
    struct modlog_mapping *cur;
    int rc;

    cur = SLIST_FIRST(&modlog_mappings);
    while (cur != NULL) {
        next = SLIST_NEXT(cur, next);

        rc = fn(&cur->desc, arg);
        if (rc != 0) {
            return rc;
        }

        cur = next;
    }

    return 0;
}

int
modlog_get(uint8_t handle, struct modlog_desc *out_desc)
{
    struct modlog_mapping *mm;
    int rc;

    modlog_lock();

    mm = modlog_find(handle, NULL);
    if (mm == NULL) {
        rc = SYS_ENOENT;
    } else {
        if (out_desc != NULL) {
            *out_desc = mm->desc;
        }
        rc = 0;
    }

    modlog_unlock();

    return rc;
}

int
modlog_register(uint8_t module, struct log *log, uint8_t min_level,
                uint8_t *out_handle)
{
    int rc;

    modlog_lock();
    rc = modlog_register_no_lock(module, log, min_level, out_handle);
    modlog_unlock();

    return rc;
}

int
modlog_delete(uint8_t handle)
{
    int rc;

    modlog_lock();
    rc = modlog_delete_no_lock(handle);
    modlog_unlock();

    return rc;
}

void
modlog_clear(void)
{
    struct modlog_mapping *mm;

    modlog_lock();

    while ((mm = SLIST_FIRST(&modlog_mappings)) != NULL) {
        modlog_remove(mm, NULL);
        modlog_free(mm);
    }

    modlog_unlock();
}

int
modlog_append(uint8_t module, uint8_t level, uint8_t etype,
              void *data, uint16_t len)
{
    int rc;

    modlog_lock();
    rc = modlog_append_no_lock(module, level, etype, data, len);
    modlog_unlock();

    return rc;
}

int
modlog_append_mbuf(uint8_t module, uint8_t level, uint8_t etype,
                   struct os_mbuf *om)
{
    int rc;

    modlog_lock();
    rc = modlog_append_mbuf_no_lock(module, level, etype, om);
    modlog_unlock();

    return rc;
}

int
modlog_foreach(modlog_foreach_fn *fn, void *arg)
{
    int rc;

    modlog_lock();
    rc = modlog_foreach_no_lock(fn, arg);
    modlog_unlock();

    return rc;
}

void
modlog_printf(uint16_t module, uint16_t level, const char *msg, ...)
{
    va_list args;
    char buf[LOG_ENTRY_HDR_SIZE + MYNEWT_VAL(MODLOG_MAX_PRINTF_LEN)];
    int len;

    va_start(args, msg);
    len = vsnprintf(buf + LOG_ENTRY_HDR_SIZE,
                    MYNEWT_VAL(MODLOG_MAX_PRINTF_LEN),
                    msg, args);
    va_end(args);

    if (len >= MYNEWT_VAL(MODLOG_MAX_PRINTF_LEN)) {
        len = MYNEWT_VAL(MODLOG_MAX_PRINTF_LEN) - 1;
    }

    modlog_append(module, level, LOG_ETYPE_STRING, buf, len);
}

void
modlog_init(void)
{
    int rc;

    SYSINIT_ASSERT_ACTIVE();

    rc = os_mempool_init(&modlog_mapping_pool, MYNEWT_VAL(MODLOG_MAX_MAPPINGS),
                         sizeof (struct modlog_mapping), modlog_mapping_buf,
                         "modlog_mapping_pool");
    SYSINIT_PANIC_ASSERT(rc == 0);

    SLIST_INIT(&modlog_mappings);
    modlog_first_dflt = NULL;

    /* Register the default console mapping if configured. */
#if MYNEWT_VAL(MODLOG_CONSOLE_DFLT)
    rc = modlog_register(MODLOG_MODULE_DFLT, log_console_get(),
                         LOG_LEVEL_DEBUG, NULL);
    SYSINIT_PANIC_ASSERT(rc == 0);
#endif
}
