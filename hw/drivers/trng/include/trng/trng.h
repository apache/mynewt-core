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

#ifndef __TRNG_H__
#define __TRNG_H__

#include <inttypes.h>
#include <stddef.h>
#include "os/mynewt.h"

#ifdef __cplusplus
extern "C" {
#endif

struct trng_dev;

typedef uint32_t (* trng_get_u32_func_t)(struct trng_dev *trng);
typedef size_t (* trng_read_func_t)(struct trng_dev *trng, void *ptr,
                                    size_t size);

struct trng_interface {
    trng_get_u32_func_t get_u32;
    trng_read_func_t read;
};

struct trng_dev {
    struct os_dev dev;
    struct trng_interface interface;
};

/**
 * Get 32-bit random value from TRNG
 *
 * This function will block until data is available in TRNG.
 *
 * @param trng  OS device
 *
 * @return  random value
 */
uint32_t trng_get_u32(struct trng_dev *trng);

/**
 * Fill buffer with random values from TRNG
 *
 * This function will read no more than \p size bytes from TRNG into target
 * buffer (up to amount of data available in TRNG for immediate read).
 *
 * @param trng  OS device
 * @param ptr   target buffer pointer
 * @param size  target buffer size (in bytes)
 *
 * @return  number of bytes read from TRNG
 */
size_t trng_read(struct trng_dev *trng, void *ptr, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* __TRNG_H__ */
