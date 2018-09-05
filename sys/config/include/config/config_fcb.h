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
#ifndef __SYS_CONFIG_FCB_H_
#define __SYS_CONFIG_FCB_H_

#include "config/config.h"
#include "config/config_store.h"

#ifdef __cplusplus
extern "C" {
#endif

struct conf_fcb {
    struct conf_store cf_store;
    struct fcb cf_fcb;
};

/**
 * Add FCB as a sourced of persisted configation
 *
 * @param cf Information regarding FCB area to add.
 *
 * @return 0 on success, non-zero on failure.
 */
int conf_fcb_src(struct conf_fcb *cf);

/**
 * Set FCB as the destination for persisting configation
 *
 * @param cf Information regarding FCB area to add. This FCB area should have
 *           been added using conf_fcb_src() previously.
 *
 * @return 0 on success, non-zero on failure.
 */
int conf_fcb_dst(struct conf_fcb *cf);

/**
 * Do a custom compression cycle. Caller provides a callback function which
 * returns whether value should be copied or not. Note that compression
 * automatically filters out old configuration values.
 *
 * @param cf FCB source to compress.
 * @param copy_or_not Function which gets called with key/value pair.
 *                    Returns 0 if copy should happen, 1 if key/value pair
 *                    should be skipped.
 */
void conf_fcb_compress(struct conf_fcb *cf,
                       int (*copy_or_not)(const char *name, const char *val,
                                          void *con_arg),
                       void *con_arg);

#ifdef __cplusplus
}
#endif

#endif /* __SYS_CONFIG_FCB_H_ */
