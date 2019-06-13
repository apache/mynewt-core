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

#include <stdint.h>
#include <trng/trng.h>
#include <mcu/mcu.h>

#define DA1469X_TRNG_FIFO_SIZE  (32 * sizeof(uint32_t))
#define DA1469X_TRNG_FIFO_ADDR  (0x30050000UL)

static size_t
da1469x_trng_read(struct trng_dev *trng, void *ptr, size_t size)
{
    uint8_t words_to_generate;
    uint8_t remaining_octets;
    uint8_t *ptr_u8 = ptr;
    uint32_t *ptr_u32 = ptr;
    uint32_t tmp;
    int i;

    if (size > DA1469X_TRNG_FIFO_SIZE) {
        size = DA1469X_TRNG_FIFO_SIZE;
    }

    words_to_generate = size / sizeof(uint32_t);
    remaining_octets = size % sizeof(uint32_t);

    CRG_TOP->CLK_AMBA_REG |= (1U << CRG_TOP_CLK_AMBA_REG_TRNG_CLK_ENABLE_Pos);
    TRNG->TRNG_CTRL_REG = TRNG_TRNG_CTRL_REG_TRNG_ENABLE_Msk;

    if (remaining_octets) {
        while (TRNG->TRNG_FIFOLVL_REG < words_to_generate + 1);
    } else {
        while (TRNG->TRNG_FIFOLVL_REG < words_to_generate);
    }

    for (i = 0; i < words_to_generate; i++) {
        ptr_u32[i] = *(uint32_t *)DA1469X_TRNG_FIFO_ADDR;
    }

    if (remaining_octets) {
        tmp = *(uint32_t *)DA1469X_TRNG_FIFO_ADDR;

        for (i = 0; i < remaining_octets; i++) {
            ptr_u8[size - remaining_octets + i] = tmp;
            tmp = tmp >> 8;
        }
    }

    TRNG->TRNG_CTRL_REG = 0;
    CRG_TOP->CLK_AMBA_REG &= ~(1U << CRG_TOP_CLK_AMBA_REG_TRNG_CLK_ENABLE_Pos);

    return size;
}

static uint32_t
da1469x_trng_get_u32(struct trng_dev *trng)
{
    uint32_t ret;

    CRG_TOP->CLK_AMBA_REG |= (1U << CRG_TOP_CLK_AMBA_REG_TRNG_CLK_ENABLE_Pos);
    TRNG->TRNG_CTRL_REG = TRNG_TRNG_CTRL_REG_TRNG_ENABLE_Msk;
    while (TRNG->TRNG_FIFOLVL_REG < 1);

    ret = *(uint32_t *)DA1469X_TRNG_FIFO_ADDR;

    TRNG->TRNG_CTRL_REG = 0;
    CRG_TOP->CLK_AMBA_REG &= ~(1U << CRG_TOP_CLK_AMBA_REG_TRNG_CLK_ENABLE_Pos);

    return ret;
}

int
da1469x_trng_init(struct os_dev *dev, void *arg)
{
    struct trng_dev *trng;

    trng = (struct trng_dev *)dev;
    assert(trng);

    /* This just makes sure that open and close functions
     * are not set as we don't need them
     */
    OS_DEV_SETHANDLERS(dev, NULL, NULL);

    trng->interface.get_u32 = da1469x_trng_get_u32;
    trng->interface.read = da1469x_trng_read;

    return 0;
}
