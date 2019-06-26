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
#include <assert.h>
#include <string.h>

#include <tinycrypt/constants.h>

#include <trng/trng.h>
#include <trng_sw/trng_sw.h>

/*
 * SW implementation of a TRNG driver API.
 * Utilizes PRNG implementation from tinycrypt.
 */
static size_t
trng_sw_read(struct trng_dev *dev, void *ptr, size_t size)
{
    struct trng_sw_dev *tsd = (struct trng_sw_dev *)dev;
    int rc;

    rc = tc_hmac_prng_generate(ptr, size, &tsd->tsd_prng);
    assert(rc == TC_CRYPTO_SUCCESS);

    return size;
}

static uint32_t
trng_sw_get_u32(struct trng_dev *dev)
{
    struct trng_sw_dev *tsd = (struct trng_sw_dev *)dev;
    uint32_t val;
    int rc;

    rc = tc_hmac_prng_generate((void *)&val, sizeof(val), &tsd->tsd_prng);
    assert(rc == TC_CRYPTO_SUCCESS);

    return val;
}

static int
trng_sw_dev_open(struct os_dev *dev, uint32_t wait, void *arg)
{
    return 0;
}

int
trng_sw_dev_add_entropy(struct trng_sw_dev *tsd, void *entr, int entr_len)
{
    int blen;
    int rc;

    blen = min(sizeof(tsd->tsd_entr) - tsd->tsd_entr_len, entr_len);
    memcpy(tsd->tsd_entr + tsd->tsd_entr_len, entr, blen);
    tsd->tsd_entr_len += blen;

    if (tsd->tsd_entr_len == sizeof(tsd->tsd_entr)) {
        rc = tc_hmac_prng_reseed(&tsd->tsd_prng, tsd->tsd_entr,
                                 sizeof(tsd->tsd_entr), NULL, 0);
        assert(rc == TC_CRYPTO_SUCCESS);
        tsd->tsd_entr_len = 0;
        if (blen != entr_len) {
            /*
             * If we got more entropy, don't waste it.
             */
            entr = (uint8_t *)entr + blen;
            entr_len -= blen;
            blen = min(sizeof(tsd->tsd_entr), entr_len);
            memcpy(tsd->tsd_entr, (uint8_t *)entr, blen);
            tsd->tsd_entr_len += blen;
        }
    } else {
        rc = 0;
    }
    return 0;
}

int
trng_sw_dev_init(struct os_dev *odev, void *arg)
{
    struct trng_sw_dev *tsd;
    struct trng_sw_dev_cfg *tsdc;
    int rc;

    tsd = (struct trng_sw_dev *)odev;
    tsdc = (struct trng_sw_dev_cfg *)arg;
    assert(odev && tsdc);

    OS_DEV_SETHANDLERS(odev, trng_sw_dev_open, NULL);

    tsd->tsd_dev.interface.get_u32 = trng_sw_get_u32;
    tsd->tsd_dev.interface.read = trng_sw_read;

    rc = tc_hmac_prng_init(&tsd->tsd_prng, tsdc->tsdc_entr, tsdc->tsdc_len);
    assert(rc == TC_CRYPTO_SUCCESS);

    return 0;
}
