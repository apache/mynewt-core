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
#include <mcu/da1469x_clock.h>

#include "enc_flash/enc_flash.h"
#include "enc_flash/enc_flash_int.h"
#include "ef_da1469x/ef_da1469x.h"
#include <mcu/da1469x_dma.h>
#include <mcu/da1469x_otp.h>

#define EDEV_TO_DA1469X(dev)   (struct eflash_da1469x_dev *)dev
#define DA1469X_AES_KEYSIZE 256

static void
do_dma_key_tx(const struct hal_flash *h_dev, uint32_t slot)
{
    DMA_Type *dma_regs = DMA;

    /* enable OTP clock and set in read mode */
    da1469x_clock_amba_enable(CRG_TOP_CLK_AMBA_REG_OTP_ENABLE_Msk);
    da1469x_otp_set_mode(OTPC_MODE_READ);

    /* securely DMA hardware key from secret storage to QSPI decrypt engine */
    dma_regs->DMA_REQ_MUX_REG |= 0xf000;
    dma_regs->DMA7_LEN_REG = 8;
    /* program source and destination addresses */
    dma_regs->DMA7_A_START_REG = MCU_OTPM_BASE + OTP_SEGMENT_USER_DATA_KEYS +
                                 (32 * (slot));
    dma_regs->DMA7_B_START_REG = (uint32_t)&AES_HASH->CRYPTO_KEYS_START;
    dma_regs->DMA7_CTRL_REG = DMA_DMA7_CTRL_REG_AINC_Msk |
                              DMA_DMA7_CTRL_REG_BINC_Msk |
                              (MCU_DMA_BUS_WIDTH_4B << DMA_DMA7_CTRL_REG_BW_Pos) |
                              DMA_DMA7_CTRL_REG_DMA_ON_Msk;
    while (dma_regs->DMA7_IDX_REG != 8);

    /* set OTP to standby and turn off clock */
    da1469x_otp_set_mode(OTPC_MODE_STBY);
    da1469x_clock_amba_disable(CRG_TOP_CLK_AMBA_REG_OTP_ENABLE_Msk);
}

static uint32_t
get_key_size_mask(int keysize)
{
    uint32_t val = 0;

    switch (keysize) {
    case 256:
        /*
         * XXX: Datasheet for da1469x Rev 2.0 (CFR0011-120-00)
         * indicates that a value of 2 and 3 correspond to
         * 256 bit AES Key, However, only 2 works.
         */
        val = 2;
        break;
    case 192:
        val = 1;
        break;
    default:
        /* 128 bits */
        val = 0;
        break;
   }
   return (val << AES_HASH_CRYPTO_CTRL_REG_CRYPTO_AES_KEY_SZ_Pos);
}

void
do_encrypt(const struct hal_flash *h_dev, uint32_t *ctr, const uint8_t *src, uint8_t *tgt)
{
    /* Select AES CTR, set CRYPTO_ALG_MD bits to 10 */
    uint32_t algo_sel = (2 << AES_HASH_CRYPTO_CTRL_REG_CRYPTO_ALG_MD_Pos);
    uint32_t ks_mask = get_key_size_mask(DA1469X_AES_KEYSIZE);

    /* XXX: for now assume we are only user of crypto block */
    da1469x_clock_amba_enable(CRG_TOP_CLK_AMBA_REG_AES_CLK_ENABLE_Msk);

    /*
     * Set CRYPTO_CTRL_REG to:
     *  Enable CRYPTO_OUT_MD: Write back to memory only the final block of
     * resulting data
     *  Set Key Size to 256 bits
     *  Set algo mode to CTR.
     */
    AES_HASH->CRYPTO_CTRL_REG = AES_HASH_CRYPTO_CTRL_REG_CRYPTO_OUT_MD_Msk |
                                AES_HASH_CRYPTO_CTRL_REG_CRYPTO_AES_KEXP_Msk |
                                ks_mask |
                                algo_sel;

    AES_HASH->CRYPTO_LEN_REG = ENC_FLASH_BLK;
    AES_HASH->CRYPTO_FETCH_ADDR_REG = (uint32_t)src;
    AES_HASH->CRYPTO_DEST_ADDR_REG = (uint32_t)tgt;

    /* Set nonce and ctr */
    AES_HASH->CRYPTO_MREG0_REG = ctr[0];
    AES_HASH->CRYPTO_MREG1_REG = ctr[1];
    AES_HASH->CRYPTO_MREG2_REG = ctr[2];
    AES_HASH->CRYPTO_MREG3_REG = ctr[3];

    assert((AES_HASH->CRYPTO_STATUS_REG & 0x01) == 1);

    /* securely transfer the key from OTP area */
    do_dma_key_tx(h_dev, MYNEWT_VAL(USER_AES_SLOT));
    /* Start encryption */
    AES_HASH->CRYPTO_START_REG = 1;

    /* wait till done */
    while ((AES_HASH->CRYPTO_STATUS_REG & 0x01) == 0);

    da1469x_clock_amba_disable(CRG_TOP_CLK_AMBA_REG_AES_CLK_ENABLE_Msk);
}

void
enc_flash_crypt_arch(struct enc_flash_dev *edev, uint32_t blk_addr,
                     const uint8_t *src, uint8_t *tgt, int off, int cnt)
{
    struct eflash_da1469x_dev *dev = EDEV_TO_DA1469X(edev);
    const struct hal_flash *h_dev = edev->efd_hwdev;
    uint32_t ctr[4] = {0};
    uint8_t blk[ENC_FLASH_BLK] = {0};

    ctr[0] = (uint32_t) ((blk_addr - h_dev->hf_base_addr) / ENC_FLASH_BLK);

    memcpy(blk + off, src, cnt);
    os_sem_pend(&dev->ef_sem, OS_TIMEOUT_NEVER);
    do_encrypt(h_dev, ctr, blk, blk);
    os_sem_release(&dev->ef_sem);
    memcpy(tgt, blk + off, cnt);
}

/* Key is securely DMA transferred from OTP user data key slot */
void
enc_flash_setkey_arch(struct enc_flash_dev *h_dev, uint8_t *key)
{
    return;
}

int
enc_flash_init_arch(struct enc_flash_dev *edev)
{
    struct eflash_da1469x_dev *dev = EDEV_TO_DA1469X(edev);

    os_sem_init(&dev->ef_sem, 1);
    return 0;
}
