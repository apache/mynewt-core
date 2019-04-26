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
#ifndef __SYS_LOG_FULL_H__
#define __SYS_LOG_FULL_H__

#include "os/mynewt.h"
#include "cbmem/cbmem.h"
#include "log_common/log_common.h"
#if MYNEWT_VAL(LOG_STATS)
#include "stats/stats.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct log;
struct log_entry_hdr;

/**
 * Used for walks and reads; indicates part of log to access.
 */
struct log_offset {
    /* If   lo_ts == -1: Only access last log entry;
     * Elif lo_ts == 0:  Don't filter by timestamp;
     * Else:             Only access entries whose ts >= lo_ts.
     */
    int64_t lo_ts;

    /* Only access entries whose index >= lo_index. */
    uint32_t lo_index;

    /* On read, lo_data_len gets populated with the number of bytes read. */
    uint32_t lo_data_len;

    /* Specific to walk / read function. */
    void *lo_arg;
};

#if MYNEWT_VAL(LOG_STORAGE_INFO)
/**
 * Log storage information
 */
struct log_storage_info {
    uint32_t size;
    uint32_t used;
#if MYNEWT_VAL(LOG_STORAGE_WATERMARK)
    uint32_t used_unread;
#endif
};
#endif

typedef int (*log_walk_func_t)(struct log *, struct log_offset *log_offset,
        void *dptr, uint16_t len);

typedef int (*log_walk_body_func_t)(struct log *log,
        struct log_offset *log_offset, const struct log_entry_hdr *hdr,
        void *dptr, uint16_t len);

typedef int (*lh_read_func_t)(struct log *, void *dptr, void *buf,
        uint16_t offset, uint16_t len);
typedef int (*lh_read_mbuf_func_t)(struct log *, void *dptr, struct os_mbuf *om,
                                   uint16_t offset, uint16_t len);
typedef int (*lh_append_func_t)(struct log *, void *buf, int len);
typedef int (*lh_append_body_func_t)(struct log *log,
                                     const struct log_entry_hdr *hdr,
                                     const void *body, int body_len);
typedef int (*lh_append_mbuf_func_t)(struct log *, const struct os_mbuf *om);
typedef int (*lh_append_mbuf_body_func_t)(struct log *log,
                                          const struct log_entry_hdr *hdr,
                                          const struct os_mbuf *om);
typedef int (*lh_walk_func_t)(struct log *,
        log_walk_func_t walk_func, struct log_offset *log_offset);
typedef int (*lh_flush_func_t)(struct log *);
#if MYNEWT_VAL(LOG_STORAGE_INFO)
typedef int (*lh_storage_info_func_t)(struct log *, struct log_storage_info *);
#endif
#if MYNEWT_VAL(LOG_STORAGE_WATERMARK)
typedef int (*lh_set_watermark_func_t)(struct log *, uint32_t);
#endif
typedef int (*lh_registered_func_t)(struct log *);

struct log_handler {
    int log_type;
    lh_read_func_t log_read;
    lh_read_mbuf_func_t log_read_mbuf;
    lh_append_func_t log_append;
    lh_append_body_func_t log_append_body;
    lh_append_mbuf_func_t log_append_mbuf;
    lh_append_mbuf_body_func_t log_append_mbuf_body;
    lh_walk_func_t log_walk;
    lh_flush_func_t log_flush;
#if MYNEWT_VAL(LOG_STORAGE_INFO)
    lh_storage_info_func_t log_storage_info;
#endif
#if MYNEWT_VAL(LOG_STORAGE_WATERMARK)
    lh_set_watermark_func_t log_set_watermark;
#endif
    /* Functions called only internally (no API for apps) */
    lh_registered_func_t log_registered;
};

#if MYNEWT_VAL(LOG_VERSION) == 2
struct log_entry_hdr {
    int64_t ue_ts;
    uint32_t ue_index;
    uint8_t ue_module;
    uint8_t ue_level;
}__attribute__((__packed__));
#elif MYNEWT_VAL(LOG_VERSION) == 3
struct log_entry_hdr {
    int64_t ue_ts;
    uint32_t ue_index;
    uint8_t ue_module;
    uint8_t ue_level;
    uint8_t ue_etype;
}__attribute__((__packed__));
#else
#error "Unsupported log version"
#endif

#define LOG_ENTRY_HDR_SIZE (sizeof(struct log_entry_hdr))

#define LOG_MODULE_STR(module)      log_module_get_name(module)

#if MYNEWT_VAL(LOG_LEVEL) <= LOG_LEVEL_DEBUG
#define LOG_DEBUG(__l, __mod, __msg, ...) log_printf(__l, __mod, \
        LOG_LEVEL_DEBUG, __msg, ##__VA_ARGS__)
#else
#define LOG_DEBUG(__l, __mod, ...) IGNORE(__VA_ARGS__)
#endif

#if MYNEWT_VAL(LOG_LEVEL) <= LOG_LEVEL_INFO
#define LOG_INFO(__l, __mod, __msg, ...) log_printf(__l, __mod, \
        LOG_LEVEL_INFO, __msg, ##__VA_ARGS__)
#else
#define LOG_INFO(__l, __mod, ...) IGNORE(__VA_ARGS__)
#endif

#if MYNEWT_VAL(LOG_LEVEL) <= LOG_LEVEL_WARN
#define LOG_WARN(__l, __mod, __msg, ...) log_printf(__l, __mod, \
        LOG_LEVEL_WARN, __msg, ##__VA_ARGS__)
#else
#define LOG_WARN(__l, __mod, ...) IGNORE(__VA_ARGS__)
#endif

#if MYNEWT_VAL(LOG_LEVEL) <= LOG_LEVEL_ERROR
#define LOG_ERROR(__l, __mod, __msg, ...) log_printf(__l, __mod, \
        LOG_LEVEL_ERROR, __msg, ##__VA_ARGS__)
#else
#define LOG_ERROR(__l, __mod, ...) IGNORE(__VA_ARGS__)
#endif

#if MYNEWT_VAL(LOG_LEVEL) <= LOG_LEVEL_CRITICAL
#define LOG_CRITICAL(__l, __mod, __msg, ...) log_printf(__l, __mod, \
        LOG_LEVEL_CRITICAL, __msg, ##__VA_ARGS__)
#else
#define LOG_CRITICAL(__l, __mod, ...) IGNORE(__VA_ARGS__)
#endif

#if MYNEWT_VAL(LOG_STATS)
STATS_SECT_START(logs)
    STATS_SECT_ENTRY(writes)
    STATS_SECT_ENTRY(drops)
    STATS_SECT_ENTRY(errs)
    STATS_SECT_ENTRY(lost)
    STATS_SECT_ENTRY(too_long)
STATS_SECT_END

#define LOG_STATS_INC(log, name)        STATS_INC(log->l_stats, name)
#define LOG_STATS_INCN(log, name, cnt)  STATS_INCN(log->l_stats, name, cnt)
#else
#define LOG_STATS_INC(log, name)
#define LOG_STATS_INCN(log, name, cnt)
#endif

struct log {
    char *l_name;
    const struct log_handler *l_log;
    void *l_arg;
    STAILQ_ENTRY(log) l_next;
    log_append_cb *l_append_cb;
    uint8_t l_level;
    uint16_t l_max_entry_len;   /* Log body length; if 0 disables check. */
#if MYNEWT_VAL(LOG_STATS)
    STATS_SECT_DECL(logs) l_stats;
#endif
};

/* Log system level functions (for all logs.) */
void log_init(void);
struct log *log_list_get_next(struct log *);

/*
 * Register per-user log module
 *
 * This function associates user log module with given name.
 *
 * If \p id is non-zero, module is registered with selected id.
 * If \p id is zero, module id is selected automatically (first available).
 *
 * Up to `LOG_MAX_USER_MODULES` (syscfg) modules can be registered with ids
 * starting from `LOG_MODULE_PERUSER`.
 *
 * @param id    Selected module id
 * @param name  Module name
 *
 * @return  module id on success, 0 on failure
 */
uint8_t log_module_register(uint8_t id, const char *name);

/*
 * Get name for module id
 *
 * This works for both system and user registered modules.
 *
 * @param id  Module id
 *
 * @return  module name or NULL if not a valid module
 */
const char *log_module_get_name(uint8_t id);

/* Log functions, manipulate a single log */
int log_register(char *name, struct log *log, const struct log_handler *,
                 void *arg, uint8_t level);

/**
 * @brief Configures the given log with the specified append callback.
 *
 * A log's append callback is executed each time an entry is appended to the
 * log.
 *
 * @param log                   The log to configure.
 * @param cb                    The callback to associate with the log.
 */
void log_set_append_cb(struct log *log, log_append_cb *cb);

/**
 * @brief Searches the list of registered logs for one with the specified name.
 *
 * @param name                  The name of the log to search for.
 *
 * @return                      The sought after log if found, NULL otherwise.
 */
struct log *log_find(const char *name);

/**
 * @brief Writes the raw contents of a flat buffer to the specified log.
 *
 * NOTE: The flat buffer must have an initial padding of length
 * `LOG_ENTRY_HDR_SIZE`.  This padding is *not* reflected in the specified
 * length.  So, to log the string "abc", you should pass the following
 * arguments to this function:
 *
 *     data: <padding>abc   (total of `LOG_ENTRY_HDR_SIZE`+3 bytes.)
 *     len: 3
 *
 * @param log                   The log to write to.
 * @param module                The log module of the entry to write.
 * @param level                 The severity of the log entry to write.
 * @param etype                 The type of data being written; one of the
 *                                  `LOG_ETYPE_[...]` constants.
 * @param data                  The flat buffer to write.
 * @param len                   The number of bytes in the *message body*.
 *
 * @return                      0 on success; nonzero on failure.
 */
int log_append_typed(struct log *log, uint8_t module, uint8_t level,
                     uint8_t etype, void *data, uint16_t len);

/**
 * @brief Logs the contents of the provided mbuf, only freeing the mbuf on
 * failure.
 *
 * Logs the contents of the provided mbuf, only freeing the mbuf on failure.
 * On success, the mbuf remains allocated, but its structure may have been
 * modified by pullup operations.  The updated mbuf address is passed back to
 * the caller via a write to the supplied mbuf pointer-to-pointer.
 *
 * NOTE: The mbuf must have an initial padding of length
 * `LOG_ENTRY_HDR_SIZE`.  So, to log the string "abc", you should pass an mbuf
 * with the following characteristics:
 *
 *     om_data: <padding>abc
 *     om_len: `LOG_ENTRY_HDR_SIZE` + 3
 *
 * @param log                   The log to write to.
 * @param module                The module ID of the entry to write.
 * @param level                 The severity of the entry to write; one of the
 *                                  `LOG_LEVEL_[...]` constants.
 * @param etype                 The type of data to write; one of the
 *                                  `LOG_ETYPE_[...]` constants.
 * @param om_ptr                Indirectly points to the mbuf to write.  This
 *                                  function updates the mbuf address if it
 *                                  changes.
 *
 * @return                      0 on success; nonzero on failure.
 */
int log_append_mbuf_typed_no_free(struct log *log, uint8_t module,
                                  uint8_t level, uint8_t etype,
                                  struct os_mbuf **om_ptr);

/**
 * @brief Logs the contents of the provided mbuf.
 *
 * Logs the contents of the provided mbuf.  This function always frees the mbuf
 * regardless of the outcome.
 *
 * NOTE: The mbuf must have an initial padding of length
 * `LOG_ENTRY_HDR_SIZE`.  So, to log the string "abc", you should pass an mbuf
 * with the following characteristics:
 *
 *     om_data: <padding>abc
 *     om_len: `LOG_ENTRY_HDR_SIZE` + 3
 *
 * @param log                   The log to write to.
 * @param module                The module ID of the entry to write.
 * @param level                 The severity of the entry to write; one of the
 *                                  `LOG_LEVEL_[...]` constants.
 * @param etype                 The type of data to write; one of the
 *                                  `LOG_ETYPE_[...]` constants.
 * @param om                    The mbuf to write.
 *
 * @return                      0 on success; nonzero on failure.
 */
int log_append_mbuf_typed(struct log *log, uint8_t module, uint8_t level,
                          uint8_t etype, struct os_mbuf *om);

/**
 * @brief Writes the contents of a flat buffer to the specified log.
 *
 * @param log                   The log to write to.
 * @param module                The log module of the entry to write.
 * @param level                 The severity of the log entry to write.
 * @param etype                 The type of data being written; one of the
 *                                  `LOG_ETYPE_[...]` constants.
 * @param data                  The flat buffer to write.
 * @param len                   The number of bytes in the message body.
 *
 * @return                      0 on success; nonzero on failure.
 */
int log_append_body(struct log *log, uint8_t module, uint8_t level,
                    uint8_t etype, const void *body, uint16_t body_len);

/**
 * @brief Logs the contents of the provided mbuf, only freeing the mbuf on
 * failure.
 *
 * Logs the contents of the provided mbuf, only freeing the mbuf on failure.
 * On success, the mbuf remains allocated, but its structure may have been
 * modified by pullup operations.  The updated mbuf address is passed back to
 * the caller via a write to the supplied mbuf pointer-to-pointer.
 *
 * @param log                   The log to write to.
 * @param module                The module ID of the entry to write.
 * @param level                 The severity of the entry to write; one of the
 *                                  `LOG_LEVEL_[...]` constants.
 * @param etype                 The type of data to write; one of the
 *                                  `LOG_ETYPE_[...]` constants.
 * @param om_ptr                Indirectly points to the mbuf to write.  This
 *                                  function updates the mbuf address if it
 *                                  changes.
 *
 * @return                      0 on success; nonzero on failure.
 */
int log_append_mbuf_body_no_free(struct log *log, uint8_t module,
                                 uint8_t level, uint8_t etype,
                                 struct os_mbuf *om);

/**
 * @brief Logs the contents of the provided mbuf.
 *
 * Logs the contents of the provided mbuf.  This function always frees the mbuf
 * regardless of the outcome.
 *
 * @param log                   The log to write to.
 * @param module                The module ID of the entry to write.
 * @param level                 The severity of the entry to write; one of the
 *                                  `LOG_LEVEL_[...]` constants.
 * @param etype                 The type of data to write; one of the
 *                                  `LOG_ETYPE_[...]` constants.
 * @param om                    The mbuf to write.
 *
 * @return                      0 on success; nonzero on failure.
 */
int log_append_mbuf_body(struct log *log, uint8_t module, uint8_t level,
                         uint8_t etype, struct os_mbuf *om);

#if MYNEWT_VAL(LOG_CONSOLE)
struct log *log_console_get(void);
void log_console_init(void);
#endif

/**
 * @brief Writes the raw contents of a flat buffer to the specified log.
 *
 * NOTE: The flat buffer must have an initial padding of length
 * `LOG_ENTRY_HDR_SIZE`.  This padding is *not* reflected in the specified
 * length.  So, to log the string "abc", you should pass the following
 * arguments to this function:
 *
 *     data: <padding>abc   (total of `LOG_ENTRY_HDR_SIZE`+3 bytes.)
 *     len: 3
 *
 * @param log                   The log to write to.
 * @param module                The log module of the entry to write.
 * @param level                 The severity of the log entry to write.
 * @param data                  The flat buffer to write.
 * @param len                   The number of byte in the *message body*.
 *
 * @return                      0 on success; nonzero on failure.
 */
static inline int
log_append(struct log *log, uint8_t module, uint8_t level, void *data,
           uint16_t len)
{
    return log_append_typed(log, module, level, LOG_ETYPE_STRING, data, len);
}

/**
 * @brief Logs the contents of the provided mbuf, only freeing the mbuf on
 * failure.
 *
 * Logs the contents of the provided mbuf, only freeing the mbuf on failure.
 * On success, the mbuf remains allocated, but its structure may have been
 * modified by pullup operations.  The updated mbuf address is passed back to
 * the caller via a write to the supplied mbuf pointer-to-pointer.
 *
 * NOTE: The mbuf must have an initial padding of length
 * `LOG_ENTRY_HDR_SIZE`.  So, to log the string "abc", you should pass an mbuf
 * with the following characteristics:
 *
 *     om_data: <padding>abc
 *     om_len: `LOG_ENTRY_HDR_SIZE` + 3
 *
 * @param log                   The log to write to.
 * @param module                The module ID of the entry to write.
 * @param level                 The severity of the entry to write; one of the
 *                                  `LOG_LEVEL_[...]` constants.
 * @param om_ptr                Indirectly points to the mbuf to write.  This
 *                                  function updates the mbuf address if it
 *                                  changes.
 *
 * @return                      0 on success; nonzero on failure.
 */
static inline int
log_append_mbuf_no_free(struct log *log, uint8_t module, uint8_t level,
                        struct os_mbuf **om)
{
    return log_append_mbuf_typed_no_free(log, module, level, LOG_ETYPE_STRING,
                                         om);
}

/**
 * @brief Logs the contents of the provided mbuf.
 *
 * Logs the contents of the provided mbuf.  This function always frees the mbuf
 * regardless of the outcome.
 *
 * NOTE: The mbuf must have an initial padding of length
 * `LOG_ENTRY_HDR_SIZE`.  So, to log the string "abc", you should pass an mbuf
 * with the following characteristics:
 *
 *     om_data: <padding>abc
 *     om_len: `LOG_ENTRY_HDR_SIZE` + 3
 *
 * @param log                   The log to write to.
 * @param module                The module ID of the entry to write.
 * @param level                 The severity of the entry to write; one of the
 *                                  `LOG_LEVEL_[...]` constants.
 * @param om                    The mbuf to write.
 *
 * @return                      0 on success; nonzero on failure.
 */
static inline int
log_append_mbuf(struct log *log, uint8_t module, uint8_t level,
                struct os_mbuf *om)
{
    return log_append_mbuf_typed(log, module, level, LOG_ETYPE_STRING, om);
}

void log_printf(struct log *log, uint8_t module, uint8_t level,
        const char *msg, ...);
int log_read(struct log *log, void *dptr, void *buf, uint16_t off,
        uint16_t len);

/**
 * @brief Reads a single log entry header.
 *
 * @param log                   The log to read from.
 * @param dptr                  Medium-specific data describing the area to
 *                                  read from; typically obtained by a call to
 *                                  `log_walk`.
 * @param hdr                   The destination header to read into.
 *
 * @return                      0 on success; nonzero on failure.
 */
int log_read_hdr(struct log *log, void *dptr, struct log_entry_hdr *hdr);

/**
 * @brief Reads data from the body of a log entry into a flat buffer.
 *
 * @param log                   The log to read from.
 * @param dptr                  Medium-specific data describing the area to
 *                                  read from; typically obtained by a call to
 *                                  `log_walk`.
 * @param buf                   The destination buffer to read into.
 * @param off                   The offset within the log entry at which to
 *                                  start the read.
 * @param len                   The number of bytes to read.
 *
 * @return                      The number of bytes actually read on success;
 *                              -1 on failure.
 */
int log_read_body(struct log *log, void *dptr, void *buf, uint16_t off,
                  uint16_t len);
int log_read_mbuf(struct log *log, void *dptr, struct os_mbuf *om, uint16_t off,
                  uint16_t len);
/**
 * @brief Reads data from the body of a log entry into an mbuf.
 *
 * @param log                   The log to read from.
 * @param dptr                  Medium-specific data describing the area to
 *                                  read from; typically obtained by a call to
 *                                  `log_walk`.
 * @param om                    The destination mbuf to read into.
 * @param off                   The offset within the log entry at which to
 *                                  start the read.
 * @param len                   The number of bytes to read.
 *
 * @return                      The number of bytes actually read on success;
 *                              -1 on failure.
 */
int log_read_mbuf_body(struct log *log, void *dptr, struct os_mbuf *om,
                       uint16_t off, uint16_t len);
int log_walk(struct log *log, log_walk_func_t walk_func,
        struct log_offset *log_offset);

/**
 * @brief Applies a callback to each message in the specified log.
 *
 * Similar to `log_walk`, except it passes the message header and body
 * separately to the callback.
 *
 * @param log                   The log to iterate.
 * @param walk_body_func        The function to apply to each log entry.
 * @param log_offset            Specifies the range of entries to process.
 *                                  Entries not matching these criteria are
 *                                  skipped during the walk.
 *
 * @return                      0 if the walk completed successfully;
 *                              nonzero on error or if the walk was aborted.
 */
int log_walk_body(struct log *log, log_walk_body_func_t walk_body_func,
        struct log_offset *log_offset);
int log_flush(struct log *log);

#if MYNEWT_VAL(LOG_MODULE_LEVELS)
/**
 * @brief Retrieves the globally configured minimum log level for the specified
 * module ID.
 *
 * Writes with a level less than the module's minimum level are discarded.
 *
 * @param module                The module whose level should be retrieved.
 *
 * @return                      The configured minimum level, or 0
 *                                  (LOG_LEVEL_DEBUG) if unconfigured.
 */
uint8_t log_level_get(uint8_t module);

/**
 * @brief Sets the globally configured minimum log level for the specified
 * module ID.
 *
 * Writes with a level less than the module's minimum level are discarded.
 *
 * @param module                The module to configure.
 * @param level                 The minimum level to assign to the module
 *                                  (0-15, inclusive).
 */
int log_level_set(uint8_t module, uint8_t level);
#else
static inline uint8_t
log_level_get(uint8_t module)
{
    /* All levels enabled. */
    return 0;
}

static inline int
log_level_set(uint8_t module, uint8_t level)
{
    return SYS_ENOTSUP;
}
#endif

/**
 * @brief Set log level for a logger.
 *
 * @param log                   The log to set level to.
 * @param level                 New log level
 */
void log_set_level(struct log *log, uint8_t level);

/**
 * @brief Get log level for a logger.
 *
 * @param log                   The log to set level to.
 *
 * @return                      current value of log level.
 */
uint8_t log_get_level(const struct log *log);

/**
 * @brief Set maximum length of an entry in the log. If set to
 *        0, no check will be made for maximum write length.
 *        Note that this is maximum log body length; the log
 *        entry header is not included in the check.
 *
 * @param log                   Log to set max entry length
 * @param level                 New max entry length
 */
void log_set_max_entry_len(struct log *log, uint16_t max_entry_len);

#if MYNEWT_VAL(LOG_STORAGE_INFO)
/**
 * Return information about log storage
 *
 * This return information about size and usage of storage on top of which log
 * instance is created.
 *
 * @param log   The log to query.
 * @param info  The destination to write information to.
 *
 * @return 0 on success, error code otherwise
 *
 */
int log_storage_info(struct log *log, struct log_storage_info *info);
#endif
#if MYNEWT_VAL(LOG_STORAGE_WATERMARK)
/**
 * Set watermark on log
 *
 * This sets watermark on log item with given index. This information is used
 * to calculate size of entries which were logged after watermark item, i.e.
 * unread items. The watermark is stored persistently for each log.
 *
 * @param log    The log to set watermark on.
 * @param index  The index of a watermarked item.
 *
 * @return 0 on success, error code otherwise.
 */
int log_set_watermark(struct log *log, uint32_t index);
#endif

/* Handler exports */
#if MYNEWT_VAL(LOG_CONSOLE)
extern const struct log_handler log_console_handler;
#endif
extern const struct log_handler log_cbmem_handler;
#if MYNEWT_VAL(LOG_FCB)
extern const struct log_handler log_fcb_handler;
extern const struct log_handler log_fcb_slot1_handler;
#endif

/* Private */
#if MYNEWT_VAL(LOG_NEWTMGR)
int log_nmgr_register_group(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __SYS_LOG_FULL_H__ */
