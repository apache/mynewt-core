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

#ifndef __GPADC_DA1469x__
#define __GPADC_DA1469x__

#include <adc/adc.h>
#include <mcu/da1469x_dma.h>

#ifdef __cplusplus
extern "C" {
#endif

struct da1469x_gpadc_dev {
    struct adc_dev dgd_adc;
    uint16_t *dgd_buf[2];
    int dgd_buf_len;
    struct da1469x_dma_regs *dgd_dma[2]; /* periph DMA always comes in pairs */
    struct da1469x_gpadc_init_cfg *dgd_init_cfg;
    struct adc_chan_config dgd_adc_chan;
};

struct da1469x_gpadc_init_cfg {
    uint8_t dgic_adc_clk_div:1;
    uint8_t dgic_dma_prio:3;
    int8_t dgic_dma_cidx;
};

struct da1469x_gpadc_dev_cfg {
    uint32_t dgdc_gpadc_ctrl;           /* GP_ADC_CTRL_REG */
    uint32_t dgdc_gpadc_ctrl2;          /* GP_ADC_CTRL2_REG */
    uint32_t dgdc_gpadc_ctrl3;          /* GP_ADC_CTRL3_REG */
    uint8_t dgdc_gpadc_set_offp:1;      /* Set GP_ADC_OFFP explicitly */
    uint8_t dgdc_gpadc_set_offn:1;      /* Set GP_ADC_OFFN explicitly */
    uint8_t dgdc_gpadc_autocalibrate:1; /* Set offp/offn automatically */
    uint32_t dgdc_gpadc_offp;           /* GP_ADC_OFFP (when _set_offp set) */
    uint32_t dgdc_gpadc_offn;           /* GP_ADC_OFFN (when _set_offn set) */
};

struct da1469x_gpadc_chan_cfg {
};

/**
 * Device initialization routine called by OS.
 *
 * @param dev Pointer to device being initialized.
 * @param arg NA
 *
 * @return 0 on success, non-zero when fail.
 */
int da1469x_gpadc_init(struct os_dev *, void *);

#if MYNEWT_VAL(GPADC_BATTERY)

#define BATTERY_ADC_DEV_NAME    "gpadc"

struct os_dev *da1469x_open_battery_adc(const char *dev_name, uint32_t wait);
#endif

#ifdef __cplusplus
}
#endif


#endif /* __GPADC_DA1469x__ */

