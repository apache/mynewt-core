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

#include <os/mynewt.h>

#include <mcu/cortex_m4.h>

#if MYNEWT_VAL(BSP_NRF52)
#include <mcu/nrf52_hal.h>
#elif MYNEWT_VAL(BSP_NRF51)
#include <mcu/nrf51_hal.h>
#else
#error "Unsupported arch"
#endif

#include <enc_flash/enc_flash.h>
#include "ef_nrf5x/ef_nrf5x.h"

#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LE_ENCRYPTION)
#error "At the moment CCM/ECB use cannot coexist"
#endif
#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LL_PRIVACY)
#error "At the moment AAR/ECB use cannot coexist"
#endif

#define EDEV_TO_NRF5X(dev) (struct eflash_nrf5x_dev *)(dev)
#define ENC_FLASH_NONCE "mynewtencfla"

static uint8_t *
nrf5x_get_block(struct eflash_nrf5x_dev *dev, uint32_t addr)
{
    uint8_t *rblk = NULL;
    int ctr = 0x100000;

    memcpy(dev->end_ecb.ene_plain + 12, &addr, sizeof(addr));

    NRF_ECB->ECBDATAPTR = (uint32_t)&dev->end_ecb;
    NRF_ECB->TASKS_STARTECB = 1;
    while (ctr-- > 0) {
        if (NRF_ECB->EVENTS_ENDECB) {
            rblk = dev->end_ecb.ene_cipher;
            goto out;
        }
        if (NRF_ECB->EVENTS_ERRORECB) {
            NRF_ECB->TASKS_STOPECB = 1;
            /* XXX maybe sleep and retry? maybe use SW instead? */
            break;
        }
    }
    if (ctr == 0) {
        NRF_ECB->TASKS_STOPECB = 1;
    }
out:
    NRF_ECB->EVENTS_ENDECB = 0;
    NRF_ECB->EVENTS_ERRORECB = 0;
    return rblk;
}

void
enc_flash_crypt_arch(struct enc_flash_dev *edev, uint32_t blk_addr,
                     const uint8_t *src, uint8_t *tgt, int off, int cnt)
{
    struct eflash_nrf5x_dev *dev = EDEV_TO_NRF5X(edev);
    int sr;
    uint8_t *blk;
    int i;

    __HAL_DISABLE_INTERRUPTS(sr);
    blk = nrf5x_get_block(dev, blk_addr);
    blk += off;
    for (i = 0 ; i < cnt; i++) {
        *tgt++ = *blk++ ^ *src++;
    }
    __HAL_ENABLE_INTERRUPTS(sr);
}

void
enc_flash_setkey_arch(struct enc_flash_dev *edev, uint8_t *key)
{
    struct eflash_nrf5x_dev *dev = EDEV_TO_NRF5X(edev);

    memcpy(dev->end_ecb.ene_key, key, ENC_FLASH_BLK);
}

int
enc_flash_init_arch(const struct enc_flash_dev *edev)
{
    struct eflash_nrf5x_dev *dev = EDEV_TO_NRF5X(edev);

    memcpy(dev->end_ecb.ene_plain, ENC_FLASH_NONCE, 12);

    return 0;
}
