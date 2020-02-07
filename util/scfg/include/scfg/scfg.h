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

#ifndef H_SCFG_
#define H_SCFG_

#include "config/config.h"

struct scfg_setting {
    /** The name of the setting. */
    const char *name;

    /** Points to the RAM replica of the setting value. */
    void *val;

    /**
     * Only needed for string settings.  Indicates the maximum length this
     * setting's value can by.
     */
    int max_len;

    /** This setting's data data.  One of the CONF_[...] constants. */
    uint8_t type;

    /**
     * Whether this setting contains private data.  If true, the value is
     * hidden in config dump output.
     */
    bool private;
};

struct scfg_group {
    /*** Public */
    /** This array must be terminated with an `{ 0 }` entry. */
    const struct scfg_setting *settings;

    /*** Private */
    struct conf_handler handler;
};

/**
 * Persists a single setting.
 *
 * @param group                 The group that the setting belongs to.
 * @param setting               The setting to save.
 *
 * @return                      0 on success; SYS_E[...] code on failure.
 */
int scfg_save_setting(const struct scfg_group *group,
                      const struct scfg_setting *setting);

/**
 * Persists the setting with the specified name.
 *
 * @param group                 The group that the setting belongs to.
 * @param setting_name          The name of the setting to save.
 *
 * @return                      0 on success; SYS_E[...] code on failure.
 */
int scfg_save_name(const struct scfg_group *group, const char *setting_name);

/**
 * Persists the setting whose value is stored in the specified variable.  The
 * specified value address should be the same one that was specified in the
 * `scfg_setting` definition.
 *
 * @param group                 The group that the setting belongs to.
 * @param val                   The address of the setting's value variable.
 *
 * @return                      0 on success; SYS_E[...] code on failure.
 */
int scfg_save_val(const struct scfg_group *group, const void *val);

/**
 * Registers a group of configuration settings.  The group's public members
 * must be populated before this function is called.
 *
 * @param group                 The group to register
 * @param name                  The name of the settings group.
 *
 * @return                      0 on success; SYS_E[...] code on failure.
 */
int scfg_register(struct scfg_group *group, char *name);

#endif
