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

/**
 * @file
 * @brief modlog - Module-mapped logging.
 *
 * The modlog facility allows log entries to be written to numeric modules
 * identifiers.  In typical usage, startup code maps 8-bit module IDs to one or
 * more log objects, while other parts of the application log events by write
 * entries to modules IDs.  This usage differs from the underlying `sys/log`
 * facility, which requires clients to provide a log object to write to
 * rather than just a module identifier.
 *
 * Benefits provided by the modlog package are:
 *     o Improved modularity - configuration and usage are distinct.
 *     o Ability to write to several logs with a single function call.  If one
 *       module ID is mapped to several logs, a write to that ID causes all
 *       mapped logs to be written.
 *     o Default mappings.  Writes to unmapped module IDs get written to an
 *       optional set of default logs.
 *     o Minimum log level per mapping.  Writes specifying a log level less
 *       than the module's minimum level are discarded.
 *
 * Costs of using modlog rather than the bare `sys/log` facility are:
 *     o Increased RAM usage (`MODLOG_MAX_MAPPINGS` * 12).
 *     o Increased CPU usage - each log write requires a lookup in the set
 *       of configured modlog mappings.
 */

#ifndef H_MODLOG_
#define H_MODLOG_

#include "os/mynewt.h"
#include "log/log.h"

#define MODLOG_MODULE_DFLT      255

/**
 * @brief Modlog mapping descriptor.
 *
 * Describes an individual mapping of module ID to log.
 */
struct modlog_desc {
    /** The log being mapped. */
    struct log *log;

    /** Unique identifier for this mapping. */
    uint8_t handle;

    /** The numeric module ID being mapped. */
    uint8_t module;

    /** Log writes with a level less than this are discarded. */
    uint8_t min_level;
};

/** @typedef modlog_foreach_fn
 * @brief Function applied to each modlog mapping during a `modlog_foreach`
 *        traversal.
 *
 * @param desc                  The modlog mapping to operate on.
 * @param arg                   Optional user argument.
 *
 * @return                      0 if the traversal should continue;
 *                              nonzero to abort the traversal with the
 *                                  specified return code.
 */
typedef int modlog_foreach_fn(const struct modlog_desc *desc, void *arg);

/**
 * @brief Retrieves the modlog mapping with the specified handle.
 *
 * @param handle                The handle of the modlog mapping to retrieve.
 * @param out_desc              On success, the retrieved mapping is written
 *                                  here.  Pass NULL if you do not reqire this
 *                                  information.
 *
 * @return                      0 on success;
 *                              SYS_ENOENT if no mapping with the specified
 *                                  handle exists;
 *                              Other SYS_[...] error code on failure.
 */
int modlog_get(uint8_t handle, struct modlog_desc *out_desc);

/**
 * @brief Registers a new modlog mapping.
 *
 * @param module                The module to map to a log.
 * @param log                   The log to map.
 * @param min_level             Subsequent writes to this mapping with a level
 *                                  less than this value are discarded.
 * @param out_handle            On success, the new mapping's handle gets
 *                                  written here.  Pass NULL if you do not
 *                                  require this information.
 * 
 * @return                      0 on success;
 *                              SYS_ENOMEM on memory exhaustion.
 *                              Other SYS_[...] error on failure.
 */
int modlog_register(uint8_t module, struct log *log, uint8_t min_level,
                    uint8_t *out_handle);

/**
 * @brief Deletes the configured modlog mapping with the specified handle.
 *
 * @param handle                The handle of the mapping to delete.
 *
 * @return                      0 on success;
 *                              SYS_ENOENT if the specified handle is unmapped.
 *                              Other SYS_[...] error on failure.
 */
int modlog_delete(uint8_t handle);

/**
 * @brief Deletes all configured modlog mappings.
 */
void modlog_clear(void);

/**
 * @brief Writes the contents of a flat buffer to the specified log module.
 *
 * NOTE: The flat buffer must have an initial padding of length
 * `LOG_ENTRY_HDR_SIZE`.  This padding is *not* reflected in the specified
 * length.  So, to log the string "abc", you should pass the following
 * arguments to this function:
 *
 *     data: <padding>abc   (total of `LOG_ENTRY_HDR_SIZE`+3 bytes.)
 *     len: 3
 *
 * @param module                The log module to write to.
 * @param level                 The severity of the log entry to write.
 * @param etype                 The type of data being written; one of the
 *                                  `LOG_ETYPE_[...]` constants.
 * @param data                  The flat buffer to write.
 * @param len                   The number of bytes to write.
 *
 * @return                      0 on success; nonzero on failure.
 */
int modlog_append(uint8_t module, uint8_t level, uint8_t etype,
                  void *data, uint16_t len);

/**
 * @brief Writes the contents of an mbuf to the specified log module.
 *
 * NOTE: The mbuf must have an initial padding of length
 * `LOG_ENTRY_HDR_SIZE`.  So, to log the string "abc", you should pass an mbuf
 * with the following characteristics:
 *
 *     om_data: <padding>abc
 *     om_len: `LOG_ENTRY_HDR_SIZE` + 3
 *
 * @param module                The log module to write to.
 * @param level                 The severity of the log entry to write.
 * @param etype                 The type of data being written; one of the
 *                                  `LOG_ETYPE_[...]` constants.
 * @param om                    The mbuf to write.
 *
 * @return                      0 on success; nonzero on failure.
 */
int modlog_append_mbuf(uint8_t module, uint8_t level, uint8_t etype,
                       struct os_mbuf *om);

/**
 * @brief Applies a function to each configured modlog mapping.
 *
 * The callback is permitted to delete the mapping under operation.  No other
 * manipulations of the mapping list are allowed during the traversal.
 *
 * @param fn                    The function to apply to each modlog mapping.
 * @param arg                   Optional argument to pass to the callback.
 *
 * @return                      0 if the full iteration completed;
 *                              nonzero if the callback aborted the iteration.
 */
int modlog_foreach(modlog_foreach_fn *fn, void *arg);

/**
 * @brief Writes a formatted text entry to the specified log module.
 *
 * @param module                The log module to write to.
 * @param level                 The severity of the log entry to write.
 * @param msg                   The "printf" formatted string to write.
 */
void modlog_printf(uint16_t module, uint16_t level, const char *msg, ...);

#if MYNEWT_VAL(LOG_LEVEL) <= LOG_LEVEL_DEBUG || defined __DOXYGEN__
/**
 * @brief Writes a formatted debug text entry to the specified log module.
 *
 * This expands to nothing if the global log level is greater than
 * `LOG_LEVEL_DEBUG`.
 *
 * @param ml_mod_               The log module to write to.
 * @param ml_msg_               The "printf" formatted string to write.
 */
#define MODLOG_DEBUG(ml_mod_, ml_msg_, ...) \
    modlog_printf(ml_mod_, LOG_LEVEL_DEBUG, ml_msg_, ##__VA_ARGS__)
#else
#define MODLOG_DEBUG(ml_mod_, ...) IGNORE(__VA_ARGS__)
#endif

#if MYNEWT_VAL(LOG_LEVEL) <= LOG_LEVEL_INFO || defined __DOXYGEN__
/**
 * @brief Writes a formatted info text entry to the specified log module.
 *
 * This expands to nothing if the global log level is greater than
 * `LOG_LEVEL_INFO`.
 *
 * @param ml_mod_               The log module to write to.
 * @param ml_msg_               The "printf" formatted string to write.
 */
#define MODLOG_INFO(ml_mod_, ml_msg_, ...) \
    modlog_printf(ml_mod_, LOG_LEVEL_INFO, ml_msg_, ##__VA_ARGS__)
#else
#define MODLOG_INFO(ml_mod_, ...) IGNORE(__VA_ARGS__)
#endif

#if MYNEWT_VAL(LOG_LEVEL) <= LOG_LEVEL_WARN || defined __DOXYGEN__
/**
 * @brief Writes a formatted warn text entry to the specified log module.
 *
 * This expands to nothing if the global log level is greater than
 * `LOG_LEVEL_WARN`.
 *
 * @param ml_mod_               The log module to write to.
 * @param ml_msg_               The "printf" formatted string to write.
 */
#define MODLOG_WARN(ml_mod_, ml_msg_, ...) \
    modlog_printf(ml_mod_, LOG_LEVEL_WARN, ml_msg_, ##__VA_ARGS__)
#else
#define MODLOG_WARN(ml_mod_, ...) IGNORE(__VA_ARGS__)
#endif

#if MYNEWT_VAL(LOG_LEVEL) <= LOG_LEVEL_ERROR || defined __DOXYGEN__
/**
 * @brief Writes a formatted error text entry to the specified log module.
 *
 * This expands to nothing if the global log level is greater than
 * `LOG_LEVEL_ERROR`.
 *
 * @param ml_mod_               The log module to write to.
 * @param ml_msg_               The "printf" formatted string to write.
 */
#define MODLOG_ERROR(ml_mod_, ml_msg_, ...) \
    modlog_printf(ml_mod_, LOG_LEVEL_ERROR, ml_msg_, ##__VA_ARGS__)
#else
#define MODLOG_ERROR(ml_mod_, ...) IGNORE(__VA_ARGS__)
#endif

#if MYNEWT_VAL(LOG_LEVEL) <= LOG_LEVEL_CRITICAL || defined __DOXYGEN__
/**
 * @brief Writes a formatted critical text entry to the specified log module.
 *
 * This expands to nothing if the global log level is greater than
 * `LOG_LEVEL_CRITICAL`.
 *
 * @param ml_mod_               The log module to write to.
 * @param ml_msg_               The "printf" formatted string to write.
 */
#define MODLOG_CRITICAL(ml_mod_, ml_msg_, ...) \
    modlog_printf(ml_mod_, LOG_LEVEL_CRITICAL, ml_msg_, ##__VA_ARGS__)
#else
#define MODLOG_CRITICAL(ml_mod_, ...) IGNORE(__VA_ARGS__)
#endif

#endif
