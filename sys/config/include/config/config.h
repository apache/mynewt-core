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
#ifndef __SYS_CONFIG_H_
#define __SYS_CONFIG_H_

/**
 * @addtogroup SysConfig Configuration of Apache Mynewt System
 * @{
 */

#include <os/queue.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @cond INTERNAL_HIDDEN */

#define CONF_MAX_DIR_DEPTH	8	/* max depth of config tree */
#define CONF_MAX_NAME_LEN	(8 * CONF_MAX_DIR_DEPTH)
#define CONF_MAX_VAL_LEN	256
#define CONF_NAME_SEPARATOR	"/"

#define CONF_NMGR_OP		0

/** @endcond */

/**
 * Type of configuration value.
 */
typedef enum conf_type {
    CONF_NONE = 0,
    CONF_DIR,
    /** 8-bit signed integer */
    CONF_INT8,
    /** 16-bit signed integer */
    CONF_INT16,
    /** 32-bit signed integer */
    CONF_INT32,
    /** 64-bit signed integer */
    CONF_INT64,
    /** String */
    CONF_STRING,
    /** Bytes */
    CONF_BYTES,
    /** Floating point */
    CONF_FLOAT,
    /** Double precision */
    CONF_DOUBLE,
    /** Boolean */
    CONF_BOOL,
} __attribute__((__packed__)) conf_type_t;

/**
 * Parameter to commit handler describing where data is going to.
 */
enum conf_export_tgt {
    /** Value is to be persisted */
    CONF_EXPORT_PERSIST,
    /** Value is to be display */
    CONF_EXPORT_SHOW
};

typedef enum conf_export_tgt conf_export_tgt_t;

/**
 * Handler for getting configuration items, this handler is called
 * per-configuration section.  Configuration sections are delimited
 * by '/', for example:
 *
 *  - section/name/value
 *
 * Would be passed as:
 *
 *  - argc = 3
 *  - argv[0] = section
 *  - argv[1] = name
 *  - argv[2] = value
 *
 * The handler returns the value into val, null terminated, up to
 * val_len_max.
 *
 * @param argc          The number of sections in the configuration variable
 * @param argv          The array of configuration sections
 * @param val           A pointer to the buffer to return the configuration
 *                      value into.
 * @param val_len_max   The maximum length of the val buffer to copy into.
 *
 * @return A pointer to val or NULL if error.
 */
typedef char *(*conf_get_handler_t)(int argc, char **argv, char *val, int val_len_max);

/**
 * Set the configuration variable pointed to by argc and argv.  See
 * description of ch_get_handler_t for format of these variables.  This sets the
 * configuration variable to the shadow value, but does not apply the configuration
 * change.  In order to apply the change, call the ch_commit() handler.
 *
 * @param argc   The number of sections in the configuration variable.
 * @param argv   The array of configuration sections
 * @param val    The value to configure that variable to
 *
 * @return 0 on success, non-zero error code on failure.
 */
typedef int (*conf_set_handler_t)(int argc, char **argv, char *val);

/**
 * Commit shadow configuration state to the active configuration.
 *
 * @return 0 on success, non-zero error code on failure.
 */
typedef int (*conf_commit_handler_t)(void);

/**
 * Called per-configuration variable being exported.
 *
 * @param name The name of the variable to export
 * @param val  The value of the variable to export
 */
typedef void (*conf_export_func_t)(char *name, char *val);

/**
 * Export all of the configuration variables, calling the export_func
 * per variable being exported.
 *
 * @param export_func  The export function to call.
 * @param tgt          The target of the export, either for persistence or display.
 *
 * @return 0 on success, non-zero error code on failure.
 */
typedef int (*conf_export_handler_t)(conf_export_func_t export_func,
        conf_export_tgt_t tgt);

/**
 * Configuration handler, used to register a config item/subtree.
 */
struct conf_handler {
    SLIST_ENTRY(conf_handler) ch_list;
    /**
     * The name of the conifguration item/subtree
     */
    char *ch_name;
    /** Get configuration value */
    conf_get_handler_t ch_get;
    /** Set configuration value */
    conf_set_handler_t ch_set;
    /** Commit configuration value */
    conf_commit_handler_t ch_commit;
    /** Export configuration value */
    conf_export_handler_t ch_export;
};

void conf_init(void);
void conf_store_init(void);

/**
 * Register a handler for configurations items.
 *
 * @param cf Structure containing registration info.
 *
 * @return 0 on success, non-zero on failure.
 */
int conf_register(struct conf_handler *cf);

/**
 * Load configuration from registered persistence sources. Handlers for
 * configuration subtrees registered earlier will be called for encountered
 * values.
 *
 * @return 0 on success, non-zero on failure.
 */
int conf_load(void);

/**
 * Save currently running configuration. All configuration which is different
 * from currently persisted values will be saved.
 *
 * @return 0 on success, non-zero on failure.
 */
int conf_save(void);

/**
 * Save currently running configuration for configuration subtree.
 *
 * @param name Name of the configuration subtree.
 *
 * @return 0 on success, non-zero on failure.
 */
int conf_save_tree(char *name);

/**
 * Write a single configuration value to persisted storage (if it has
 * changed value).
 *
 * @param name Name/key of the configuration item.
 * @param var Value of the configuration item.
 *
 * @return 0 on success, non-zero on failure.
 */
int conf_save_one(const char *name, char *var);

/*
  XXXX for later
  int conf_save_lib(char *name);
  int conf_save_var(char *name, char *var);
*/

/**
 * Set configuration item identified by @p name to be value @p val_str.
 * This finds the configuration handler for this subtree and calls it's
 * set handler.
 *
 * @param name Name/key of the configuration item.
 * @param val_str Value of the configuration item.
 *
 * @return 0 on success, non-zero on failure.
 */
int conf_set_value(char *name, char *val_str);

/**
 * Get value of configuration item identified by @p name.
 * This calls the configuration handler ch_get for the subtree.
 *
 * Configuration handler can copy the string to @p buf, the maximum
 * number of bytes it will copy is limited by @p buf_len.
 *
 * Return value will be pointer to beginning of the value. Note that
 * this might, or might not be the same as buf.
 *
 * @param name Name/key of the configuration item.
 * @param val_str Value of the configuration item.
 *
 * @return pointer to value on success, NULL on failure.
 */
char *conf_get_value(char *name, char *buf, int buf_len);

/**
 * Call commit for all configuration handler. This should apply all
 * configuration which has been set, but not applied yet.
 *
 * @param name Name of the configuration subtree, or NULL to commit everything.
 *
 * @return 0 on success, non-zero on failure.
 */
int conf_commit(char *name);

/**
 * Convenience routine for converting value passed as a string to native
 * data type.
 *
 * @param val_str Value of the configuration item as string.
 * @param type Type of the value to convert to.
 * @param vp Pointer to variable to fill with the decoded value.
 * @param vp Size of that variable.
 *
 * @return 0 on success, non-zero on failure.
 */
int conf_value_from_str(char *val_str, enum conf_type type, void *vp,
  int maxlen);

/**
 * Convenience routine for converting byte array passed as a base64
 * encoded string.
 *
 * @param val_str Value of the configuration item as string.
 * @param vp Pointer to variable to fill with the decoded value.
 * @param len Size of that variable. On return the number of bytes in the array.
 *
 * @return 0 on success, non-zero on failure.
 */
int conf_bytes_from_str(char *val_str, void *vp, int *len);

/**
 * Convenience routine for converting native data type to a string.
 *
 * @param type Type of the value to convert from.
 * @param vp Pointer to variable to convert.
 * @param buf Buffer where string value will be stored.
 * @param buf_len Size of the buffer.
 *
 * @return 0 on success, non-zero on failure.
 */
char *conf_str_from_value(enum conf_type type, void *vp, char *buf,
  int buf_len);

/** Return the length of a configuration string from buffer length. */
#define CONF_STR_FROM_BYTES_LEN(len) (((len) * 4 / 3) + 4)

/**
 * Convenience routine for converting byte array into a base64
 * encoded string.
 *
 * @param vp Pointer to variable to convert.
 * @param vp_len Number of bytes to convert.
 * @param buf Buffer where string value will be stored.
 * @param buf_len Size of the buffer.
 *
 * @return 0 on success, non-zero on failure.
 */
char *conf_str_from_bytes(void *vp, int vp_len, char *buf, int buf_len);

/**
 * Convert a string into a value of type
 */
#define CONF_VALUE_SET(str, type, val)                                  \
    conf_value_from_str((str), (type), &(val), sizeof(val))

/*
 * Config storage
 */
struct conf_store_itf;
struct conf_store {
    SLIST_ENTRY(conf_store) cs_next;
    const struct conf_store_itf *cs_itf;
};

#ifdef __cplusplus
}
#endif

/**
 * @} SysConfig
 */


#endif /* __SYS_CONFIG_H_ */
