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
#ifndef __SYS_LOG_STUB_H__
#define __SYS_LOG_STUB_H__

#include <inttypes.h>
#include "os/mynewt.h"
#include "log_common/log_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LOG_DEBUG(__l, __mod, ...) IGNORE(__VA_ARGS__)
#define LOG_INFO(__l, __mod, ...) IGNORE(__VA_ARGS__)
#define LOG_WARN(__l, __mod, ...) IGNORE(__VA_ARGS__)
#define LOG_ERROR(__l, __mod, ...) IGNORE(__VA_ARGS__)
#define LOG_CRITICAL(__l, __mod, ...) IGNORE(__VA_ARGS__)

struct log {
};

struct log_handler {
};

static inline int
log_register(char *name, struct log *log, const struct log_handler *h,
             void *arg, uint8_t level)
{
    return 0;
}

static inline void
log_set_append_cb(struct log *log, log_append_cb *cb)
{
}

static inline struct log *
log_find(const char *name)
{
    return NULL;
}

static inline int
log_append_typed(struct log *log, uint8_t module, uint8_t level, uint8_t etype,
                 void *data, uint16_t len)
{
    return 0;
}

static inline int
log_append_mbuf_typed_no_free(struct log *log, uint8_t module, uint8_t level,
                              uint8_t etype, struct os_mbuf **om_ptr)
{
    return 0;
}

static inline int
log_append_mbuf_typed(struct log *log, uint8_t module, uint8_t level,
                      uint8_t etype, struct os_mbuf *om)
{
    os_mbuf_free_chain(om);
    return 0;
}

static inline int
log_append_mbuf_body_no_free(struct log *log, uint8_t module, uint8_t level,
                             uint8_t etype, struct os_mbuf *om)
{
    return 0;
}

static inline int
log_append_mbuf_body(struct log *log, uint8_t module, uint8_t level,
                     uint8_t etype, struct os_mbuf *om)
{
    os_mbuf_free_chain(om);
    return 0;
}

static inline void
log_init(void)
{
}

static inline void log_set_level(struct log *log, uint8_t level)
{
    return;
}

static inline uint8_t log_get_level(const struct log *log)
{
    return 0;
}

/**
 * @brief Set maximum length of an entry in the log. If set to
 *        0, no check will be made for maximum write length.
 *        Note that this is maximum log body length; the log
 *        entry header is not included in the check.
 *
 * @param log                   Log to set max entry length
 * @param level                 New max entry length
 */
static inline void
log_set_max_entry_len(struct log *log, uint16_t max_entry_len)
{
    return;
}

#define log_printf(...)

/*
 * Dummy handler exports.
 */
extern const struct log_handler log_console_handler;
extern const struct log_handler log_cbmem_handler;
extern const struct log_handler log_fcb_handler;
#if MYNEWT_VAL(LOG_FCB_SLOT1)
extern const struct log_handler log_fcb_slot1_handler;
#endif

#if MYNEWT_VAL(LOG_CONSOLE)
static inline struct log *
log_console_get(void)
{
    return NULL;
}
#endif

#ifdef __cplusplus
}
#endif

#endif /* __SYS_LOG_STUB_H__ */
