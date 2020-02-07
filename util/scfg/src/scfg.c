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

#include "os/mynewt.h"
#include "scfg/scfg.h"

/* "+ 1" to account for null terminator. */
#define SCFG_SETTING_ID_BUF_SIZE    (MYNEWT_VAL(SCFG_SETTING_ID_MAX_LEN) + 1)

#define SCFG_NUM_STR_BUF_SIZE       (sizeof "18446744073709551615")

#define SCFG_FOREACH_SETTING(group, s) \
    for (s = (group)->settings; s->name != NULL; s++)

static void
scfg_setting_id(const char *group_name, const char *setting_name, char *buf)
{
    int setting_len;
    int group_len;
    int off;

    group_len = strlen(group_name);
    setting_len = strlen(setting_name);

    /* <group>/<setting> */
    assert(group_len + 1 + setting_len < SCFG_SETTING_ID_BUF_SIZE);

    off = 0;

    memcpy(&buf[off], group_name, group_len);
    off += group_len;

    buf[off] = '/';
    off++;

    memcpy(&buf[off], setting_name, setting_len);
    off += setting_len;

    buf[off] = '\0';
}

static struct scfg_setting *
scfg_find_setting_by_name(const struct scfg_group *group,
                          const char *setting_name)
{
    const struct scfg_setting *setting;

    SCFG_FOREACH_SETTING(group, setting) {
        if (strcmp(setting->name, setting_name) == 0) {
            /* Cast away const. */
            return (struct scfg_setting *)setting;
        }
    }

    return NULL;
}

static struct scfg_setting *
scfg_find_setting_by_val(const struct scfg_group *group, const void *val)
{
    const struct scfg_setting *setting;

    SCFG_FOREACH_SETTING(group, setting) {
        if (setting->val == val) {
            /* Cast away const. */
            return (struct scfg_setting *)setting;
        }
    }

    return NULL;
}

/**
 * Conf get handler.  Converts a setting's underlying variable to a string.
 */
static char *
scfg_handler_get(int argc, char **argv, char *buf, int max_len, void *arg)
{
    const struct scfg_setting *setting;
    struct scfg_group *group;

    group = arg;

    if (argc < 1) {
        return NULL;
    }

    setting = scfg_find_setting_by_name(group, argv[0]);
    if (setting == NULL) {
        return NULL;
    }

    return conf_str_from_value(setting->type, setting->val, buf, max_len);
}

/**
 * Conf set handler.  Converts from a string-representation and writes the
 * result to the setting's underlying variable.
 */
static int
scfg_handler_set(int argc, char **argv, char *val, void *arg)
{
    const struct scfg_setting *setting;
    struct scfg_group *group;
    int rc;

    group = arg;

    if (argc < 1) {
        return SYS_EINVAL;
    }

    setting = scfg_find_setting_by_name(group, argv[0]);
    if (setting == NULL) {
        return SYS_ENOENT;
    }

    rc = conf_value_from_str(val, setting->type, setting->val,
                             setting->max_len);
    if (rc != 0) {
        return os_error_to_sys(rc);
    }

    return 0;
}

static int
scfg_handler_export(void (*func)(char *name, char *val),
                    enum conf_export_tgt tgt, void *arg)
{
    const struct scfg_setting *setting;
    struct scfg_group *group;
    char id_buf[SCFG_SETTING_ID_BUF_SIZE];
    char val_buf[SCFG_NUM_STR_BUF_SIZE];
    char *val;

    group = arg;

    SCFG_FOREACH_SETTING(group, setting) {
        scfg_setting_id(group->handler.ch_name, setting->name, id_buf);
        if (setting->private) {
            val = "<set>";
        } else {
            val = conf_str_from_value(setting->type, setting->val,
                                      val_buf, sizeof val_buf);
        }
        func(id_buf, val);
    }

    return 0;
}

int
scfg_save_setting(const struct scfg_group *group,
                  const struct scfg_setting *setting)
{
    char id_buf[SCFG_SETTING_ID_BUF_SIZE];
    char val_buf[SCFG_NUM_STR_BUF_SIZE];
    char *val;
    int rc;

    val = conf_str_from_value(setting->type, setting->val,
                              val_buf, sizeof val_buf);
    if (val == NULL) {
        return SYS_EUNKNOWN;
    }

    scfg_setting_id(group->handler.ch_name, setting->name, id_buf);

    rc = conf_save_one(id_buf, val);
    if (rc != 0) {
        return os_error_to_sys(rc);
    }

    return 0;
}

int
scfg_save_name(const struct scfg_group *group, const char *setting_name)
{
    const struct scfg_setting *setting;

    setting = scfg_find_setting_by_name(group, setting_name);
    if (setting == NULL) {
        return SYS_ENOENT;
    }

    return scfg_save_setting(group, setting);
}

int
scfg_save_val(const struct scfg_group *group, const void *val)
{
    const struct scfg_setting *setting;

    setting = scfg_find_setting_by_val(group, val);
    if (setting == NULL) {
        return SYS_ENOENT;
    }

    return scfg_save_setting(group, setting);
}

int
scfg_register(struct scfg_group *group, char *name)
{
    const struct scfg_setting *setting;
    int rc;

    SCFG_FOREACH_SETTING(group, setting) {
        switch (setting->type) {
        case CONF_INT8:
        case CONF_INT16:
        case CONF_INT32:
        case CONF_INT64:
        case CONF_STRING:
        case CONF_BOOL:
            break;

        default:
            /* We don't know how to (de)serialize the other data types. */
            return SYS_EINVAL;
        }
    }

    group->handler = (struct conf_handler) {
        .ch_name = name,
        .ch_get_ext = scfg_handler_get,
        .ch_set_ext = scfg_handler_set,
        .ch_export_ext = scfg_handler_export,
        .ch_arg = group,
        .ch_ext = true,
    };

    rc = conf_register(&group->handler);
    if (rc != 0) {
        return os_error_to_sys(rc);
    }

    return 0;
}
