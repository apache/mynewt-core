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
#include "syscfg/syscfg.h"
#include <mcu/mcu.h>
#include "da1469x_priv.h"
#include "mcu/da1469x_clock.h"
#include <mcu/da1469x_otp.h>

int
da1469x_otp_read(uint32_t offset, void *dst, uint32_t num_bytes)
{
    uint32_t *src_addr = (uint32_t *)(MCU_OTPM_BASE + offset);
    uint32_t *dst_addr = dst;

    if (offset >= MCU_OTPM_SIZE || (offset + num_bytes) > MCU_OTPM_SIZE) {
       return OTP_ERR_INVALID_ADDRESS;
    }

    if (num_bytes & 0x3) {
       return OTP_ERR_INVALID_SIZE_ALIGNMENT;
    }

    /* Enable OTP clock and set mode to standby */
    da1469x_clock_amba_enable(CRG_TOP_CLK_AMBA_REG_OTP_ENABLE_Msk);

    da1469x_otp_set_mode(OTPC_MODE_READ);

    for (; num_bytes; dst_addr++, src_addr++, num_bytes -= 4) {
        *dst_addr = *src_addr;
    }

    da1469x_otp_set_mode(OTPC_MODE_DSTBY);

    /* Disable OTP clock */
    da1469x_clock_amba_disable(CRG_TOP_CLK_AMBA_REG_OTP_ENABLE_Msk);
    return 0;
}

int
da1469x_otp_write(uint32_t offset, const void *src, uint32_t num_bytes)
{
    uint32_t *dst_addr = (uint32_t *)(MCU_OTPM_BASE + offset);
    const uint32_t *src_addr = src;
    int ret = 0;

    if (offset >= MCU_OTPM_SIZE || (offset + num_bytes) > MCU_OTPM_SIZE) {
       return OTP_ERR_INVALID_ADDRESS;
    }

    if (num_bytes & 0x3) {
       return OTP_ERR_INVALID_SIZE_ALIGNMENT;
    }

    /* Enable OTP clock and set mode to standby */
    da1469x_clock_amba_enable(CRG_TOP_CLK_AMBA_REG_OTP_ENABLE_Msk);

    do {
        da1469x_otp_set_mode(OTPC_MODE_PROG);

        /* wait for programming to go idle and data buffer to be empty */
        while (!(OTPC->OTPC_STAT_REG & OTPC_OTPC_STAT_REG_OTPC_STAT_PRDY_Msk));
        while (!(OTPC->OTPC_STAT_REG &
                 OTPC_OTPC_STAT_REG_OTPC_STAT_PBUF_EMPTY_Msk));

        /* fill data buffer with a word and trigger via the PADDR reg */
        OTPC->OTPC_PWORD_REG = *src_addr;
        OTPC->OTPC_PADDR_REG = ((uint32_t)dst_addr >> 2) &
                               OTPC_OTPC_PADDR_REG_OTPC_PADDR_Msk;

        while (!(OTPC->OTPC_STAT_REG & OTPC_OTPC_STAT_REG_OTPC_STAT_PRDY_Msk));

        /* set mode to verify */
        da1469x_otp_set_mode(OTPC_MODE_PVFY);

        /* read data and compare to source */
        if (*dst_addr != *src_addr) {
            ret = OTP_ERR_PROGRAM_VERIFY_FAILED;
            goto fail_write;
        }

        da1469x_otp_set_mode(OTPC_MODE_STBY);
        num_bytes -= 4;
        src_addr++;
        dst_addr++;
    } while (num_bytes);

fail_write:

    /* Disable OTP clock */
    da1469x_clock_amba_disable(CRG_TOP_CLK_AMBA_REG_OTP_ENABLE_Msk);

    return ret;
}

void
da1469x_otp_init(void)
{
    /* Enable OTP clock and set mode to standby */
    da1469x_clock_amba_enable(CRG_TOP_CLK_AMBA_REG_OTP_ENABLE_Msk);

    da1469x_otp_set_mode(OTPC_MODE_STBY);

    /* set clk timing */
    OTPC->OTPC_TIM1_REG = 0x0999101f;  /* 32 MHz default */
    OTPC->OTPC_TIM2_REG = 0xa4040409;

    /* Disable OTP clock */
    da1469x_clock_amba_disable(CRG_TOP_CLK_AMBA_REG_OTP_ENABLE_Msk);
}
