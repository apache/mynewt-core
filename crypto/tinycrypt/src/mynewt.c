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

#if MYNEWT_VAL(TINYCRYPT_UECC_RNG_USE_TRNG)

#include "tinycrypt/ecc.h"
#include "trng/trng.h"

static struct trng_dev *g_trng;

static int
uecc_rng_trng(uint8_t *dst, unsigned int size)
{
    size_t num;

    while (size) {
        num = trng_read(g_trng, dst, size);
        dst += num;
        size -= num;
    }

    return 1;
}

void
mynewt_tinycrypt_pkg_init(void)
{
    g_trng = (struct trng_dev *)os_dev_open(MYNEWT_VAL(TINYCRYPT_UECC_RNG_TRNG_DEV_NAME),
                                            OS_WAIT_FOREVER, NULL);
    assert(g_trng);
    uECC_set_rng(uecc_rng_trng);
}

#endif
