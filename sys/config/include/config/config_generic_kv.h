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
#ifndef __SYS_CONFIG_GENERIC_KV_H_
#define __SYS_CONFIG_GENERIC_KV_H_

#include <stddef.h>
#include "config/config.h"
#include "fcb/fcb.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Load value for given key from FCB key-value storage area
 *
 * This loads latest value set for given key from FCB key-value storage area.
 * The FCB kv store can be any FCB provided by application, however it's up to
 * application to make sure such FCB was not used for other purposes. Trying to
 * load value from FCB which was used for other purposes may yield undefined
 * results.
 *
 * The returned value is always null-terminated.
 *
 * @param fcb    FCB with kv store
 * @param name   Key name to load
 * @param value  Buffer to store value
 * @param len    Length of buffer
 *
 * @return OS_OK on success, error code otherwise
 */
int conf_fcb_kv_load(struct fcb *fcb, const char *name, char *value, size_t len);

/**
 * Store value for given key to FCB key-value storage area
 *
 * This stores new value for given key to FCB key-value storage area.
 * The FCB kv store can be any FCB provided by application.
 *
 * @param fcb    FCB with kv store
 * @param name   Key name to store
 * @param value  Value to store
 *
 * @return OS_OK on success, error code otherwise
 */
int conf_fcb_kv_save(struct fcb *fcb, const char *name, const char *value);

#ifdef __cplusplus
}
#endif

#endif /* __SYS_CONFIG_GENERIC_KV_H_ */
