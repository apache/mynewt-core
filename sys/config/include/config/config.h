/**
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

#include <os/queue.h>
#include <stdint.h>

#define CONF_MAX_DIR_DEPTH	8	/* max depth of config tree */
#define CONF_MAX_NAME_LEN	(8 * CONF_MAX_DIR_DEPTH)
#define CONF_MAX_VAL_LEN	256
#define CONF_NAME_SEPARATOR	"/"

#define CONF_NMGR_OP		0

enum conf_type {
    CONF_NONE = 0,
    CONF_DIR,
    CONF_INT8,
    CONF_INT16,
    CONF_INT32,
    CONF_INT64,
    CONF_STRING,
    CONF_BYTES,
    CONF_FLOAT,
    CONF_DOUBLE
} __attribute__((__packed__));

enum conf_export_tgt {
    CONF_EXPORT_PERSIST,        /* Value is to be persisted. */
    CONF_EXPORT_SHOW            /* Value is to be displayed. */
};

struct conf_handler {
    SLIST_ENTRY(conf_handler) ch_list;
    char *ch_name;
    char *(*ch_get)(int argc, char **argv, char *val, int val_len_max);
    int (*ch_set)(int argc, char **argv, char *val);
    int (*ch_commit)(void);
    int (*ch_export)(void (*export_func)(char *name, char *val),
      enum conf_export_tgt tgt);
};

int conf_init(void);
int conf_register(struct conf_handler *);
int conf_load(void);

int conf_save(void);
int conf_save_one(const char *name, char *var);

/*
  XXXX for later
  int conf_save_lib(char *name);
  int conf_save_var(char *name, char *var);
*/

int conf_set_value(char *name, char *val_str);
char *conf_get_value(char *name, char *buf, int buf_len);
int conf_commit(char *name);

int conf_value_from_str(char *val_str, enum conf_type type, void *vp,
  int maxlen);
int conf_bytes_from_str(char *val_str, void *vp, int *len);
char *conf_str_from_value(enum conf_type type, void *vp, char *buf,
  int buf_len);
#define CONF_STR_FROM_BYTES_LEN(len) (((len) * 4 / 3) + 4)
char *conf_str_from_bytes(void *vp, int vp_len, char *buf, int buf_len);

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

#endif /* __SYS_CONFIG_H_ */
