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

#ifndef _TRNG_SW_H_
#define _TRNG_SW_H_

#include <os/os_dev.h>
#include <trng/trng.h>
#include <tinycrypt/hmac_prng.h>


#ifdef __cplusplus
extern "C" {
#endif

struct trng_sw_dev {
    struct trng_dev tsd_dev;
    struct tc_hmac_prng_struct tsd_prng;
    uint8_t tsd_entr[32]; /* min entropy to reseed */
    uint8_t tsd_entr_len;
};

/**
 * Initial personalization data when initializing the device.
 */
struct trng_sw_dev_cfg {
    void *tsdc_entr;    /* pointer to entropy data */
    int tsdc_len;       /* number of bytes of entropy */
};

/**
 * Initializer routine for ther TRNG software implementation
 *
 * @param dev Pointer to device being initialized
 * @param arg Pointer to struct trng_sw_dev_cfg structure.
 *
 * @return 0 on success. Non-zero on error.
 */
int trng_sw_dev_init(struct os_dev *dev, void *arg);

/**
 * Add more entropy to random number generator. Use things like
 * interrupt/packet arrival timestamps.
 * Note: before you can use this driver, you have to give at least
 * 32 bytes of entropy.
 *
 * @param tsd Pointer to device
 * @param entr Pointer to random data to add to entropy pool.
 * @param entr_len Number of bytes to add.
 *
 * @return 0 on success. Non-zero on error.
 */
int trng_sw_dev_add_entropy(struct trng_sw_dev *tsd, void *entr, int entr_len);

#ifdef __cplusplus
}
#endif

#endif
