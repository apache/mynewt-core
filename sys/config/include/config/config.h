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

struct conf_handler {
    SLIST_ENTRY(conf_handler) ch_list;
    char *ch_name;
    char *(*ch_get)(int argc, char **argv, char *val, int val_len_max);
    int (*ch_set)(int argc, char **argv, char *val);
    int (*ch_commit)();
};

int conf_init(void);
int conf_register(struct conf_handler *);
int conf_load(void);

int conf_set_value(char *name, char *val_str);
char *conf_get_value(char *name, char *buf, int buf_len);
int conf_commit(char *name);

int conf_value_from_str(char *val_str, enum conf_type type, void *vp,
  int maxlen);
char *conf_str_from_value(enum conf_type type, void *vp, char *buf,
  int buf_len);
#define CONF_VALUE_SET(str, type, val)                                  \
    conf_value_from_str((str), (type), &(val), sizeof(val))

/*
 * Persisting config.
 */
typedef void (*load_cb)(char *name, char *val, void *cb_arg);

struct conf_store;
struct conf_store_itf {
    int (*csi_load)(struct conf_store *ci, load_cb cb, void *cb_arg);
    int (*csi_save)(struct conf_store *ci, char *name, char *value);
};

struct conf_store {
    SLIST_ENTRY(conf_store) cs_next;
    const struct conf_store_itf *cs_itf;
};

void conf_src_register(struct conf_store *cs);

#endif /* __SYS_CONFIG_H_ */
