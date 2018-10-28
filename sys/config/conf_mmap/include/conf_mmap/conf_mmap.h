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

#ifndef __CONF_MMAP_H
#define __CONF_MMAP_H

#include <config/config.h>
#include <config/config_store.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Read-only config source from a memory mapped data region.
 *
 * Data is expected to be stored without keys. Config source registration
 * provides key names to values when config source is registered.
 * Like with other config values, data has to be a printable string.
 *
 * E.g. memory layout @ 0x10000
 *  0x10000: device_serial_number, 16 bytes,
 *  0x10010: device_model, 6 bytes,
 *  0x10006: device_hw_version, 8 bytes
 *
 * Config name to value mapping would be provided like this:
 * static const struct conf_mmap_kv my_static_kvs[] = {
 *   [0] = { "id/serial", 0, 16 },
 *   [1] = { "id/model", 16, 6 },
 *   [2] = { "hw/ver", 22, 8 }
 * };
 *
 * Then declare the conf_mmap structure:
 * static struct conf_mmap my_static_conf = {
 *   .cm_base = (uintptr_t)0x10000,
 *   .cm_kv_cnt = sizeof(my_static_kvs) / sizeof(my_static_kvs[0]),
 *   .cm_kv = my_static_kvs,
 * };
 *
 * and then register this, eg. before registering other config sources
 *
 * conf_mmap_src(&my_static_conf);
 */

/**
 * Config key value mapping declaration.
 */
struct conf_mmap_kv {
    const char *cmk_key;  /* key (string) */
    uint16_t cmk_off;     /* offset of value from conf_mmap.cm_base */
    uint16_t cmk_maxlen;  /* maximum length of value */
};

struct conf_mmap {
    struct conf_store cm_store;
    uintptr_t cm_base;    /* base address */
    int cm_kv_cnt;        /* number of key/value array elements */
    const struct conf_mmap_kv *cm_kv; /* key/value array */
};

/**
 * Add memory mapped read-only data as a config source.
 *
 * @param cm Information about config data
 *
 * @return 0 on success, non-zero on failure.
 */
int conf_mmap_src(struct conf_mmap *cm);

#ifdef __cplusplus
}
#endif

#endif
