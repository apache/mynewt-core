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

#include <string.h>
#include "mcu/cmsis_nvic.h"
#include <os/mynewt.h>
#include "crypto/crypto.h"
#include "crypto_nrf52/crypto_nrf52.h"

static struct os_mutex gmtx;

struct aes_128_data {
    uint8_t key[AES_128_KEY_LEN];
    uint8_t plain[AES_BLOCK_LEN];
    uint8_t cipher[AES_BLOCK_LEN];
};

static bool
nrf52_crypto_ecb_start(const struct aes_128_data *crypto_data)
{
    int ctr = 0x100000;
    bool rc = true;

    NRF_ECB->ECBDATAPTR = (uint32_t)crypto_data;
    NRF_ECB->TASKS_STARTECB = 1;

    while (ctr-- > 0) {
        if (NRF_ECB->EVENTS_ENDECB) {
            break;
        }
        if (NRF_ECB->EVENTS_ERRORECB) {
            NRF_ECB->TASKS_STOPECB = 1;
            rc = false;
            break;
        }
    }
    if (ctr == 0) {
        NRF_ECB->TASKS_STOPECB = 1;
    }
    NRF_ECB->EVENTS_ENDECB = 0;
    NRF_ECB->EVENTS_ERRORECB = 0;

    return rc;
}

static uint32_t
nrf52_crypto_encrypt_ecb(struct crypto_dev *crypto, const uint8_t *key,
        const uint8_t *inbuf, uint8_t *outbuf, size_t len)
{
    struct aes_128_data crypto_data;
    uint32_t i;
    size_t remain;

    os_mutex_pend(&gmtx, OS_TIMEOUT_NEVER);

    memcpy(crypto_data.key, key, AES_128_KEY_LEN);

    i = 0;
    remain = len;
    while (remain) {
        if (len > AES_BLOCK_LEN) {
            len = AES_BLOCK_LEN;
        }

        memcpy(crypto_data.plain, &inbuf[i], len);

        /* XXX might sleep for a tick and try again? */
        if (nrf52_crypto_ecb_start(&crypto_data) == false) {
            break;
        }

        memcpy(&outbuf[i], crypto_data.cipher, len);

        i += len;
        remain -= len;
        len = remain;
    }

    os_mutex_release(&gmtx);

    return i;
}

static bool
nrf52_crypto_has_support(struct crypto_dev *crypto, uint8_t op, uint16_t algo,
        uint16_t mode, uint16_t keylen)
{
    (void)crypto;

    if (op == CRYPTO_OP_ENCRYPT && algo == CRYPTO_ALGO_AES &&
            mode == CRYPTO_MODE_ECB && keylen == 128) {
        return true;
    }

    return false;
}

static uint32_t
nrf52_crypto_encrypt(struct crypto_dev *crypto, uint16_t algo, uint16_t mode,
        const uint8_t *key, uint16_t keylen, uint8_t *iv, const uint8_t *inbuf,
        uint8_t *outbuf, uint32_t len)
{
    (void)iv;

    if (!nrf52_crypto_has_support(crypto, CRYPTO_OP_ENCRYPT, algo, mode, keylen)) {
        return 0;
    }

    return nrf52_crypto_encrypt_ecb(crypto, key, inbuf, outbuf, len);
}

static int
nrf52_crypto_dev_open(struct os_dev *dev, uint32_t wait, void *arg)
{
    struct crypto_dev *crypto;

    crypto = (struct crypto_dev *)dev;
    assert(crypto);

    if (!(dev->od_flags & OS_DEV_F_STATUS_OPEN)) {
        /* XXX do something here? */
    }

    return OS_OK;
}

int
nrf52_crypto_dev_init(struct os_dev *dev, void *arg)
{
    struct crypto_dev *crypto;

    crypto = (struct crypto_dev *)dev;
    assert(crypto);

    OS_DEV_SETHANDLERS(dev, nrf52_crypto_dev_open, NULL);

    assert(os_mutex_init(&gmtx) == 0);

    crypto->interface.encrypt = nrf52_crypto_encrypt;
    crypto->interface.decrypt = NULL;
    crypto->interface.has_support = nrf52_crypto_has_support;

    return 0;
}
