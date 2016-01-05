/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef __UTIL_CONFIG_H_
#define __UTIL_CONFIG_H_

#include <os/queue.h>
#include <stdint.h>

#define CONF_MAX_DIR_DEPTH	8	/* max depth of config tree */
#define CONF_NAME_SEPARATOR	"/"

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

struct conf_entry {
    const char *c_name;
    enum conf_type c_type;
    union {
        struct {	/* INT8, INT16, INT32, INT64, FLOAT, DOUBLE */
            void *val;
        } single;
        struct {	/* STRING, BYTES */
            uint16_t maxlen;
            uint16_t len;
            void *val;
        } array;
    } c_val;
};

struct conf_entry_dir {
    const char *c_name;
    enum conf_type c_type;	/* DIR */
};

struct conf_node {
    SLIST_ENTRY(conf_node) cn_next;
    SLIST_HEAD(, conf_node) cn_children;
    struct conf_entry *cn_array;
    int cn_cnt;
};

int conf_module_init(void);
int conf_register(struct conf_node *parent, struct conf_node *child);
struct conf_entry *conf_lookup(int argc, char **argv);

int conf_parse_name(char *name, int *name_argc, char *name_argv[]);

#endif /* __UTIL_CONFIG_H_ */
