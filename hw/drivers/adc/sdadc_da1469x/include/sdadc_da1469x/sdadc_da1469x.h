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

#ifndef __SDADC_DA1469x__
#define __SDADC_DA1469x__

#include <adc/adc.h>
#include <mcu/da1469x_dma.h>

#ifdef __cplusplus
extern "C" {
#endif

struct da1469x_sdadc_dev {
    struct adc_dev dsd_adc;
    uint16_t *dsd_buf[2];
    int dsd_buf_len;
    struct da1469x_dma_regs *dsd_dma[2]; /* periph DMA always comes in pairs */
    struct da1469x_sdadc_init_cfg *dsd_init_cfg;
    struct adc_chan_config dsd_adc_chan;
};

struct da1469x_sdadc_init_cfg {
    uint8_t dsic_dma_prio:3;
    int8_t dsic_dma_cidx;
};

struct da1469x_sdadc_dev_cfg {
    uint32_t dsdc_sdadc_ctrl;
    uint8_t dsdc_sdadc_set_gain_corr:1;
    uint8_t dsdc_sdadc_set_offs_corr:1;
    uint32_t dsdc_sdadc_gain_corr;
    uint32_t dsdc_sdadc_offs_corr;
};

struct da1469x_sdadc_chan_cfg {
    uint16_t dscc_refmv;        /* ref VDD in mV, when using external */
};

/**
 * Device initialization routine called by OS.
 *
 * @param dev Pointer to device being initialized.
 * @param arg NA
 *
 * @return 0 on success, non-zero when fail.
 */
int da1469x_sdadc_init(struct os_dev *, void *);

#if MYNEWT_VAL(SDADC_BATTERY)

#define BATTERY_ADC_DEV_NAME    "sdadc"

struct os_dev *da1469x_open_battery_adc(const char *dev_name, uint32_t wait);
#endif

#ifdef __cplusplus
}
#endif


#endif /* __SDADC_DA1469x__ */

