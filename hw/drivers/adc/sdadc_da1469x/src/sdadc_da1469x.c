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
#include <os/mynewt.h>

#include <adc/adc.h>

#include <mcu/mcu.h>
#include <mcu/da1469x_pd.h>

#include "sdadc_da1469x/sdadc_da1469x.h"

static struct da1469x_sdadc_dev *da1469x_sdadc_dev;

static const int da1469x_sdadc_src2pin[] = {
    MCU_GPIO_PORT1(9),  /* 0 */
    MCU_GPIO_PORT0(25), /* 1 */
    MCU_GPIO_PORT0(8),  /* 2 */
    MCU_GPIO_PORT0(9),  /* 3 */
    MCU_GPIO_PORT1(14), /* 4 */
    MCU_GPIO_PORT1(20), /* 5 */
    MCU_GPIO_PORT1(21), /* 6 */
    MCU_GPIO_PORT1(22), /* 7 */
};
#define DA1469X_SDADC_SRC2PIN_SZ                                        \
    (sizeof(da1469x_sdadc_src2pin) / sizeof(da1469x_sdadc_src2pin[0]))

static void
da1469x_sdadc_resolve_pins(uint32_t ctrl, int *pin0p, int *pin1p)
{
    int pin0 = -1;
    int pin1 = -1;
    uint32_t srcn;
    uint32_t srcp;

    /*
     * What is the source?
     */
    srcn = ctrl & SDADC_SDADC_CTRL_REG_SDADC_INN_SEL_Msk;
    srcn = srcn >> SDADC_SDADC_CTRL_REG_SDADC_INN_SEL_Pos;
    srcp = ctrl & SDADC_SDADC_CTRL_REG_SDADC_INP_SEL_Msk;
    srcp = srcp >> SDADC_SDADC_CTRL_REG_SDADC_INP_SEL_Pos;

    if (srcp < DA1469X_SDADC_SRC2PIN_SZ) {
        pin0 = da1469x_sdadc_src2pin[srcp];
    }
    if ((ctrl & SDADC_SDADC_CTRL_REG_SDADC_SE_Msk) == 0 &&
        srcn < DA1469X_SDADC_SRC2PIN_SZ) {
        pin1 = da1469x_sdadc_src2pin[srcn];
    }
    *pin0p = pin0;
    *pin1p = pin1;
}

/**
 * Configure an ADC channel for the general purpose ADC.
 *
 * @param dev The ADC device to configure
 * @param cnum The channel on the ADC device to configure
 * @param cfgdata An opaque pointer to channel config
 *
 * @return 0 on success, non-zero on failure.
 */
static int
da1469x_sdadc_configure_channel(struct adc_dev *adev, uint8_t cnum,
                                void *cfg)
{
    uint32_t ctrl;
    uint16_t refmv;

    ctrl = SDADC->SDADC_CTRL_REG;

    if ((ctrl & SDADC_SDADC_CTRL_REG_SDADC_VREF_SEL_Msk) == 0) {
        /*
         * Internal reference voltage is 1.2 V.
         */
        if (ctrl & SDADC_SDADC_CTRL_REG_SDADC_SE_Msk) {
            refmv = 1200; /* 0 - 1.2 V */
        } else {
            refmv = 2400; /* -1.2 - 1.2 V */
        }
    } else {
        /*
         * External refence voltage
         */
        refmv = ((struct da1469x_sdadc_chan_cfg *)cfg)->dscc_refmv;
    }
    if ((ctrl & SDADC_SDADC_CTRL_REG_SDADC_SE_Msk) &&
        ((ctrl & SDADC_SDADC_CTRL_REG_SDADC_INP_SEL_Msk) ==
         (8 << SDADC_SDADC_CTRL_REG_SDADC_INP_SEL_Pos))) {
        /*
         * If single-ended, and measuring Vbat, then result is 4x attenuated
         */
        refmv *= 4;
    }
    adev->ad_chans[0].c_res = 16;
    adev->ad_chans[0].c_refmv = refmv;
    adev->ad_chans[0].c_configured = 1;

    return 0;
}

/**
 * Blocking read of an ADC channel, returns result as an integer.
 */
static int
da1469x_sdadc_read_channel(struct adc_dev *adev, uint8_t cnum, int *result)
{
    uint32_t reg;

    /*
     * Disable continuous mode (if set), and wait for ADC to stop.
     */
    SDADC->SDADC_CTRL_REG &= ~SDADC_SDADC_CTRL_REG_SDADC_CONT_Msk;

    do {
        reg = SDADC->SDADC_CTRL_REG;
    } while (reg & SDADC_SDADC_CTRL_REG_SDADC_START_Msk);

    /* Clear interrupts */
    SDADC->SDADC_CLEAR_INT_REG = 1;

    /* Disable DMA */
    SDADC->SDADC_CTRL_REG &= ~SDADC_SDADC_CTRL_REG_SDADC_DMA_EN_Msk;

    /* Start the conversion */
    reg |= SDADC_SDADC_CTRL_REG_SDADC_START_Msk;
    SDADC->SDADC_CTRL_REG = reg;

    /* Wait for the conversion to finish */
    do {
        reg = SDADC->SDADC_CTRL_REG;
    } while ((reg & SDADC_SDADC_CTRL_REG_SDADC_INT_Msk) == 0);

    /*
     * Read results. # of valid bits depends on settings.
     *
     * Note that there is no centering of results like with GPADC.
     */
    *result = SDADC->SDADC_RESULT_REG;
    return 0;
}

static void
da1469x_sdadc_dma_buf(struct da1469x_sdadc_dev *dev)
{
    struct da1469x_dma_regs *dr;

    dr = dev->dsd_dma[0];

    dr->DMA_B_START_REG = (uint32_t)dev->dsd_buf[0];
    dr->DMA_INT_REG = dev->dsd_buf_len - 1;
    dr->DMA_LEN_REG = dev->dsd_buf_len - 1;
    dr->DMA_CTRL_REG |= DMA_DMA0_CTRL_REG_DMA_ON_Msk;
}

/**
 * Trigger taking a sample.
 */
static int
da1469x_sdadc_sample(struct adc_dev *adev)
{
    struct da1469x_sdadc_dev *dev;
    int sr;

    dev = (struct da1469x_sdadc_dev *)adev;

    /*
     * Disable continuous mode (if set), and wait for ADC to stop.
     */
    SDADC->SDADC_CTRL_REG &= ~SDADC_SDADC_CTRL_REG_SDADC_CONT_Msk;
    while (SDADC->SDADC_CTRL_REG & SDADC_SDADC_CTRL_REG_SDADC_START_Msk);

    if (dev->dsd_buf[0]) {
        OS_ENTER_CRITICAL(sr);
        da1469x_sdadc_dma_buf(dev);
        OS_EXIT_CRITICAL(sr);
    }

    /* Start the conversion. */
    SDADC->SDADC_CTRL_REG |= (SDADC_SDADC_CTRL_REG_SDADC_CONT_Msk |
                              SDADC_SDADC_CTRL_REG_SDADC_DMA_EN_Msk |
                              SDADC_SDADC_CTRL_REG_SDADC_START_Msk);

    return 0;
}

/**
 * Set buffer to read data into.  Implementation of setbuffer handler.
 * Sets both the primary and secondary buffers for DMA.
 */
static int
da1469x_sdadc_set_buffer(struct adc_dev *adev, void *buf1, void *buf2,
                         int buf_len)
{
    struct da1469x_sdadc_dev *dev;
    int sr;

    dev = (struct da1469x_sdadc_dev *)adev;

    OS_ENTER_CRITICAL(sr);
    dev->dsd_buf[0] = buf1;
    dev->dsd_buf[1] = buf2;
    dev->dsd_buf_len = (buf_len / sizeof(uint16_t));
    OS_EXIT_CRITICAL(sr);

    return 0;
}

static int
da1469x_sdadc_release_buffer(struct adc_dev *adev, void *buf, int buf_len)
{
    int sr;
    int rc = 0;
    struct da1469x_sdadc_dev *dev;

    dev = (struct da1469x_sdadc_dev *)adev;

    OS_ENTER_CRITICAL(sr);
    if (dev->dsd_buf[0] == NULL) {
        /*
         * If data RX was stalled, restart it.
         */
        dev->dsd_buf[0] = buf;
        da1469x_sdadc_dma_buf(dev);
        SDADC->SDADC_CTRL_REG |= (SDADC_SDADC_CTRL_REG_SDADC_CONT_Msk |
                                  SDADC_SDADC_CTRL_REG_SDADC_START_Msk);
    } else if (dev->dsd_buf[1] == NULL) {
        /*
         * If we can fit another buffer, queue it.
         */
        dev->dsd_buf[1] = buf;
    } else {
        rc = OS_EBUSY;
    }
    OS_EXIT_CRITICAL(sr);
    return rc;
}

static int
da1469x_sdadc_read_buffer(struct adc_dev *adev, void *buf, int buf_len, int off,
                          int *result)
{
    uint16_t val;

    memcpy(&val, (uint16_t *)buf + off, sizeof(val));
    *result = val;
    return 0;
}

static int
da1469x_sdadc_size_buffer(struct adc_dev *dev, int chans, int samples)
{
    return sizeof(uint16_t) * chans * samples;
}

static int
da1469x_sdadc_dmairq(void *arg)
{
    struct da1469x_sdadc_dev *dev = (struct da1469x_sdadc_dev *)arg;
    struct adc_dev *adev;
    uint16_t *buf;

    buf = dev->dsd_buf[0];

    /* swap inactive buf to active slot (if it exists) */
    dev->dsd_buf[0] = dev->dsd_buf[1];
    dev->dsd_buf[1] = NULL;

    /*
     * We got DMA interrupt, so it should not be running anymore.
     */
    while (dev->dsd_dma[0]->DMA_CTRL_REG & DMA_DMA0_CTRL_REG_DMA_ON_Msk);

    if (dev->dsd_buf[0]) {
        da1469x_sdadc_dma_buf(dev);
    } else {
        SDADC->SDADC_CTRL_REG &= ~SDADC_SDADC_CTRL_REG_SDADC_CONT_Msk;
    }

    adev = &dev->dsd_adc;
    adev->ad_event_handler_func(adev, NULL, ADC_EVENT_RESULT, buf,
                                dev->dsd_buf_len);
    return 0;
}

static const struct adc_driver_funcs da1469x_sdadc_funcs = {
    .af_configure_channel = da1469x_sdadc_configure_channel,
    .af_sample = da1469x_sdadc_sample,
    .af_read_channel = da1469x_sdadc_read_channel,
    .af_set_buffer = da1469x_sdadc_set_buffer,
    .af_release_buffer = da1469x_sdadc_release_buffer,
    .af_read_buffer = da1469x_sdadc_read_buffer,
    .af_size_buffer = da1469x_sdadc_size_buffer
};

static int
da1469x_sdadc_hwinit(struct da1469x_sdadc_dev *dev,
                     struct da1469x_sdadc_dev_cfg *dsdc)
{
    uint32_t reg;
    int pin0;
    int pin1;

    da1469x_sdadc_resolve_pins(dsdc->dsdc_sdadc_ctrl, &pin0, &pin1);
    if (pin0 >= 0) {
        mcu_gpio_set_pin_function(pin0, MCU_GPIO_MODE_INPUT, MCU_GPIO_FUNC_ADC);
    }
    if (pin1 >= 0) {
        mcu_gpio_set_pin_function(pin1, MCU_GPIO_MODE_INPUT, MCU_GPIO_FUNC_ADC);
    }

    /*
     * Some of this could be in channel config. Given that there can be
     * only one active channel at a time, passing all the config here
     * is ok too.
     */
    reg = dsdc->dsdc_sdadc_ctrl &
      (SDADC_SDADC_CTRL_REG_SDADC_INP_SEL_Msk |
        SDADC_SDADC_CTRL_REG_SDADC_INN_SEL_Msk |
        SDADC_SDADC_CTRL_REG_SDADC_SE_Msk |
        SDADC_SDADC_CTRL_REG_SDADC_OSR_Msk |
        SDADC_SDADC_CTRL_REG_SDADC_VREF_SEL_Msk);
    SDADC->SDADC_CTRL_REG = reg | SDADC_SDADC_CTRL_REG_SDADC_EN_Msk;

    if (dsdc->dsdc_sdadc_set_gain_corr) {
        reg = dsdc->dsdc_sdadc_gain_corr &
              SDADC_SDADC_GAIN_CORR_REG_SDADC_GAIN_CORR_Msk;
        SDADC->SDADC_GAIN_CORR_REG = reg;
    }
    if (dsdc->dsdc_sdadc_set_offs_corr) {
        reg = dsdc->dsdc_sdadc_offs_corr &
              SDADC_SDADC_OFFS_CORR_REG_SDADC_OFFS_CORR_Msk;
        SDADC->SDADC_OFFS_CORR_REG = reg;
    }

    return 0;
}

static int
da1469x_sdadc_open(struct os_dev *odev, uint32_t wait, void *arg)
{
    struct da1469x_sdadc_dev *dev;
    struct da1469x_dma_config cfg = {
        .src_inc = false,
        .dst_inc = true,
        .bus_width = MCU_DMA_BUS_WIDTH_2B,
        .burst_mode = MCU_DMA_BURST_MODE_DISABLED,
    };
    int rc;
    int dma_cidx;

    dev = (struct da1469x_sdadc_dev *)odev;

    rc = os_mutex_pend(&dev->dsd_adc.ad_lock, wait);
    if (rc != OS_OK && rc != OS_NOT_STARTED) {
        return rc;
    }
    if (++dev->dsd_adc.ad_ref_cnt == 1) {
        da1469x_pd_acquire(MCU_PD_DOMAIN_COM);

        /* initialize */
        if (da1469x_sdadc_hwinit(dev, arg)) {
            rc = OS_EINVAL;
        }
        dma_cidx = dev->dsd_init_cfg->dsic_dma_cidx;
        cfg.priority = dev->dsd_init_cfg->dsic_dma_prio;
        if (da1469x_dma_acquire_periph(dma_cidx, MCU_DMA_PERIPH_SDADC,
            dev->dsd_dma)) {
            rc = OS_ERROR;
        }
        da1469x_dma_configure(dev->dsd_dma[0], &cfg, da1469x_sdadc_dmairq, dev);
        dev->dsd_dma[0]->DMA_A_START_REG = (uint32_t)&SDADC->SDADC_RESULT_REG;
    }
    if (rc) {
        da1469x_pd_release(MCU_PD_DOMAIN_COM);
        --dev->dsd_adc.ad_ref_cnt;
    }
    os_mutex_release(&dev->dsd_adc.ad_lock);
    return rc;
}

static int
da1469x_sdadc_close(struct os_dev *odev)
{
    struct da1469x_sdadc_dev *dev;
    int rc;
    int pin0, pin1;

    dev = (struct da1469x_sdadc_dev *)odev;

    rc = os_mutex_pend(&dev->dsd_adc.ad_lock, OS_TIMEOUT_NEVER);
    if (rc != OS_OK && rc != OS_NOT_STARTED) {
        return rc;
    }
    if (--dev->dsd_adc.ad_ref_cnt == 0) {
        da1469x_dma_release_channel(dev->dsd_dma[0]);
        /*
         * uninit
         * XXXX what state should the pins be left in?
         */
        da1469x_sdadc_resolve_pins(SDADC->SDADC_CTRL_REG, &pin0, &pin1);
        if (pin0 >= 0) {
            mcu_gpio_set_pin_function(pin0, MCU_GPIO_MODE_INPUT_PULLDOWN, 0);
        }
        if (pin1 >= 0) {
            mcu_gpio_set_pin_function(pin0, MCU_GPIO_MODE_INPUT_PULLDOWN, 0);
        }
        SDADC->SDADC_CTRL_REG = 0;
        SDADC->SDADC_CLEAR_INT_REG = 1;

        da1469x_pd_release(MCU_PD_DOMAIN_COM);
    }
    os_mutex_release(&dev->dsd_adc.ad_lock);
    return rc;
}

int
da1469x_sdadc_init(struct os_dev *odev, void *arg)
{
    struct da1469x_sdadc_dev *dev;

    dev = (struct da1469x_sdadc_dev *)odev;
    da1469x_sdadc_dev = dev;

    os_mutex_init(&dev->dsd_adc.ad_lock);

    dev->dsd_adc.ad_chans = &dev->dsd_adc_chan;
    dev->dsd_adc.ad_chan_count = 1;

    OS_DEV_SETHANDLERS(odev, da1469x_sdadc_open, da1469x_sdadc_close);

    dev->dsd_init_cfg = arg;

    dev->dsd_adc.ad_funcs = &da1469x_sdadc_funcs;

    return 0;
}

#if MYNEWT_VAL(SDADC_BATTERY)

static struct da1469x_sdadc_dev_cfg os_bsp_adc_battery_cfg = {
    .dsdc_sdadc_ctrl = (1U << SDADC_SDADC_CTRL_REG_SDADC_SE_Pos) |
                       (8U << SDADC_SDADC_CTRL_REG_SDADC_INP_SEL_Pos),
    .dsdc_sdadc_gain_corr = 0,
    .dsdc_sdadc_offs_corr = 0,
    .dsdc_sdadc_set_gain_corr = 0,
    .dsdc_sdadc_set_offs_corr = 0,
};

struct os_dev *
da1469x_open_battery_adc(const char *dev_name, uint32_t wait)
{
    struct os_dev *adc = os_dev_open(dev_name, wait, &os_bsp_adc_battery_cfg);
    if (adc) {
        /* call adc_chan_config to setup correct multiplier so read returns
         * value in mV */
        adc_chan_config((struct adc_dev *)adc, 0, NULL);
    }
    return adc;
}

#endif
