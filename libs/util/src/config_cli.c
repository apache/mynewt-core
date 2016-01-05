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

#include <stddef.h>

#include "util/config.h"

#ifdef SHELL_PRESENT
#include <string.h>

#include <shell/shell.h>
#include <console/console.h>

static struct shell_cmd shell_conf_cmd;

static void
shell_conf_display(struct conf_entry *ce)
{
    int32_t val;

    if (ce->c_type == CONF_STRING) {
        console_printf("%s\n", (char *)ce->c_val.array.val);
        return;
    }
    switch (ce->c_type) {
    case CONF_INT8:
    case CONF_INT16:
    case CONF_INT32:
        if (ce->c_type == CONF_INT8) {
            val = *(int8_t *)ce->c_val.single.val;
        } else if (ce->c_type == CONF_INT16) {
            val = *(int16_t *)ce->c_val.single.val;
        } else {
            val = *(int32_t *)ce->c_val.single.val;
        }
        console_printf("%ld (0x%lx)\n", (long)val, (long)val);
        break;
    default:
        console_printf("Can't print type %d\n", ce->c_type);
        return;
    }
}

static int
shell_conf_set(struct conf_entry *ce, char *val_str)
{
    int32_t val;
    char *eptr;

    switch (ce->c_type) {
    case CONF_INT8:
    case CONF_INT16:
    case CONF_INT32:
        val = strtol(val_str, &eptr, 0);
        if (*eptr != '\0') {
            goto err;
        }
        if (ce->c_type == CONF_INT8) {
            if (val < INT8_MIN || val > UINT8_MAX) {
                goto err;
            }
            *(int8_t *)ce->c_val.single.val = val;
        } else if (ce->c_type == CONF_INT16) {
            if (val < INT16_MIN || val > UINT16_MAX) {
                goto err;
            }
            *(int16_t *)ce->c_val.single.val = val;
        } else if (ce->c_type == CONF_INT32) {
            *(int32_t *)ce->c_val.single.val = val;
        }
        break;
    case CONF_STRING:
        val = strlen(val_str);
        if (val + 1 > ce->c_val.array.maxlen) {
            goto err;
        }
        strcpy(ce->c_val.array.val, val_str);
        ce->c_val.array.len = val;
        break;
    default:
        console_printf("Can't parse type %d\n", ce->c_type);
        break;
    }
    return 0;
err:
    return -1;
}

static int
shell_conf_command(int argc, char **argv)
{
    char *name = NULL;
    char *val = NULL;
    char *name_argv[CONF_MAX_DIR_DEPTH];
    int name_argc;
    int rc;
    struct conf_entry *ce;

    switch (argc) {
    case 1:
        break;
    case 2:
        name = argv[1];
        break;
    case 3:
        name = argv[1];
        val = argv[2];
        break;
    default:
        goto err;
    }

    rc = conf_parse_name(name, &name_argc, name_argv);
    if (rc) {
        goto err;
    }

    ce = conf_lookup(name_argc, name_argv);
    if (!ce) {
        console_printf("No such config variable\n");
        goto err;
    }

    if (!val) {
        shell_conf_display(ce);
    } else {
        rc = shell_conf_set(ce, val);
        if (rc) {
            console_printf("Failed to set\n");
            goto err;
        }
    }
    return 0;
err:
    console_printf("Invalid args\n");
    return 0;
}
#endif

int conf_module_init(void)
{
#ifdef SHELL_PRESENT
    shell_cmd_register(&shell_conf_cmd, "config", shell_conf_command);
#endif
    return 0;
}

