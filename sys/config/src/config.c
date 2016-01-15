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

#include <string.h>

#include "config/config.h"

#ifdef SHELL_PRESENT
#include <shell/shell.h>
#include <console/console.h>
#endif

static struct conf_node g_conf_root;

static struct conf_entry *
conf_node_match(struct conf_node *cn, const char *name)
{
    int i;

    for (i = 0; i < cn->cn_cnt; i++) {
        if (!strcmp(name, cn->cn_array[i].c_name)) {
            return &cn->cn_array[i];
        }
    }
    return NULL;
}

/*
 * Register config node to a specific spot in the tree.
 */
int
conf_register(struct conf_node *parent, struct conf_node *child)
{
    struct conf_node *cn;
    int i;

    if (!parent) {
        parent = &g_conf_root;
    }

    for (i = 0; i < child->cn_cnt; i++) {
        SLIST_FOREACH(cn, &parent->cn_children, cn_next) {
            if (conf_node_match(cn, child->cn_array[i].c_name)) {
                return -1;
            }
        }
    }
    SLIST_INSERT_HEAD(&parent->cn_children, child, cn_next);
    return 0;
}

/*
 * Lookup conf_entry based on name.
 */
struct conf_entry *
conf_lookup(int argc, char **argv)
{
    int i;
    struct conf_node *parent = &g_conf_root;
    struct conf_entry *ret = NULL;
    struct conf_node *cn;

    for (i = 0; i < argc; i++) {
        ret = NULL;
        SLIST_FOREACH(cn, &parent->cn_children, cn_next) {
            ret = conf_node_match(cn, argv[i]);
            if (ret) {
                break;
            }
        }
        parent = cn;
        if (!parent) {
            return NULL;
        }
    }
    return ret;
}

/*
 * Separate string into argv array.
 */
int
conf_parse_name(char *name, int *name_argc, char *name_argv[])
{
    char *tok;
    char *tok_ptr;
    int i;

    tok_ptr = NULL;
    tok = strtok_r(name, CONF_NAME_SEPARATOR, &tok_ptr);

    i = 0;
    while (tok) {
        name_argv[i++] = tok;
        tok = strtok_r(NULL, CONF_NAME_SEPARATOR, &tok_ptr);
    }
    *name_argc = i;

    return 0;
}
