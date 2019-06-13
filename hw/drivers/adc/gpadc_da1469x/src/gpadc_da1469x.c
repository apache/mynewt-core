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

#include <console/console.h>

#include <adc/adc.h>

#include <mcu/mcu.h>
#include <mcu/da1469x_pd.h>

#include "gpadc_da1469x/gpadc_da1469x.h"

static struct da1469x_gpadc_dev *da1469x_gpadc_dev;

static void
da1469x_gpadc_resolve_pins(uint32_t ctrl, int *pin0p, int *pin1p)
{
    int pin0 = -1;
    int pin1 = -1;
    uint32_t src;

    /*
     * What is the source?
     */
    src = ctrl & GPADC_GP_ADC_CTRL_REG_GP_ADC_SEL_Msk;
    src = src >> GPADC_GP_ADC_CTRL_REG_GP_ADC_SEL_Pos;

    if (ctrl & GPADC_GP_ADC_CTRL_REG_GP_ADC_SE_Msk) {
        /* single-ended */
        switch (src) {
        case 0:
            pin0 = MCU_GPIO_PORT1(9);
            break;
        case 1:
            pin0 = MCU_GPIO_PORT0(25);
            break;
        case 2:
            pin0 = MCU_GPIO_PORT0(8);
            break;
        case 3:
            pin0 = MCU_GPIO_PORT0(9);
            break;
        case 4:
            /* VDDD */
            break;
        case 5:
            /* V33 (GP_ADC_ATTN3X scaler automatically selected) */
            break;
        case 6:
            /* V33 (GP_ADC_ATTN3X scaler automatically selected) */
            break;
        case 7:
            /* 7: DCDC (GP_ADC_ATTN3X scaler automatically selected) */
            break;
        case 8:
            /* VBAT (5V to 1.2V scaler selected) */
            break;
        case 9:
            /* 9: VSSA */
            break;
        case 16:
            pin0 = MCU_GPIO_PORT1(13);
            break;
        case 17:
            pin0 = MCU_GPIO_PORT1(12);
            break;
        case 18:
            pin0 = MCU_GPIO_PORT1(18);
            break;
        case 19:
            pin0 = MCU_GPIO_PORT1(19);
            break;
        case 20:
            /* diff temp sensor */
            break;
        default:
            break;
        }
    } else {
        /* differential */
        switch (src) {
        case 0:
            pin0 = MCU_GPIO_PORT1(9);
            pin1 = MCU_GPIO_PORT0(25);
            break;
        case 1:
            pin0 = MCU_GPIO_PORT0(8);
            pin1 = MCU_GPIO_PORT0(9);
            break;
        default:
            break;
        }
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
da1469x_gpadc_configure_channel(struct adc_dev *adev, uint8_t cnum,
                                void *cfg)
{
    uint32_t ctrl;
    uint32_t ctrl2;
    uint16_t refmv;

    ctrl = GPADC->GP_ADC_CTRL_REG;
    ctrl2 = GPADC->GP_ADC_CTRL2_REG;

    if (ctrl & GPADC_GP_ADC_CTRL_REG_GP_ADC_SE_Msk) {
        /*
         * Figure out source.
         */
        ctrl = ctrl & GPADC_GP_ADC_CTRL_REG_GP_ADC_SEL_Msk;
        ctrl = ctrl >> GPADC_GP_ADC_CTRL_REG_GP_ADC_SEL_Pos;
        if (ctrl == 8) {
            /* Vbat has special scale */
            refmv = 5000;
        } else {
            refmv = 1200; /* 0 - 1.2 V */
        }
        if (ctrl2 & GPADC_GP_ADC_CTRL2_REG_GP_ADC_ATTN3X_Msk ||
            (ctrl >= 5 && ctrl <= 7)) {
            /*
             * Range is 0 - 3.6 V. Sources 5-7 enable attn3x automatically.
             */
            refmv *= 3;
        }
    } else {
        if (ctrl2 & GPADC_GP_ADC_CTRL2_REG_GP_ADC_ATTN3X_Msk) {
            refmv = 7200; /* -3.6 - 3.6 V */
        } else {
            refmv = 2400; /* -1.2 - 1.2 V */
        }
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
da1469x_gpadc_read(uint32_t *val)
{
    uint32_t reg;

    /*
     * Disable continuous mode (if set), and wait for ADC to stop.
     */
    if (GPADC->GP_ADC_CTRL_REG & GPADC_GP_ADC_CTRL_REG_GP_ADC_CONT_Msk) {
        GPADC->GP_ADC_CTRL_REG &= ~GPADC_GP_ADC_CTRL_REG_GP_ADC_CONT_Msk;
    }

    do {
        reg = GPADC->GP_ADC_CTRL_REG;
    } while (reg & GPADC_GP_ADC_CTRL_REG_GP_ADC_START_Msk);

    /* Clear interrupts */
    GPADC->GP_ADC_CLEAR_INT_REG = 1;

    /* Start the conversion, disable DMA */
    GPADC->GP_ADC_CTRL2_REG &= ~GPADC_GP_ADC_CTRL2_REG_GP_ADC_DMA_EN_Msk;
    reg |= GPADC_GP_ADC_CTRL_REG_GP_ADC_START_Msk;
    GPADC->GP_ADC_CTRL_REG = reg;

    /* Wait for the conversion to finish */
    do {
        reg = GPADC->GP_ADC_CTRL_REG;
    } while ((reg & GPADC_GP_ADC_CTRL_REG_GP_ADC_INT_Msk) == 0);

    *val = GPADC->GP_ADC_RESULT_REG;

    return 0;
}

static int
da1469x_gpadc_read_scaled(uint32_t *val)
{
    if (da1469x_gpadc_read(val)) {
        return -1;
    }
    *val = *val >> 6;
    return 0;
}

static int
da1469x_gpadc_read_channel(struct adc_dev *adev, uint8_t cnum, int *result)
{
    uint32_t val;

    da1469x_gpadc_read(&val);

    /*
     * Read results. # of valid bits depends on settings.
     */
    if (GPADC->GP_ADC_CTRL_REG & GPADC_GP_ADC_CTRL_REG_GP_ADC_SE_Msk) {
        *result = val;
    } else {
        /*
         * Differential mode.
         * 0x8000 is 0V. Move that point to be 0.
         */
        *result = val - 0x8000;
    }
    return 0;
}

static void
da1469x_gpadc_dma_buf(struct da1469x_gpadc_dev *dev)
{
    struct da1469x_dma_regs *dr;

    dr = dev->dgd_dma[0];

    dr->DMA_B_START_REG = (uint32_t)dev->dgd_buf[0];
    dr->DMA_INT_REG = dev->dgd_buf_len - 1;
    dr->DMA_LEN_REG = dev->dgd_buf_len - 1;
    dr->DMA_CTRL_REG |= DMA_DMA0_CTRL_REG_DMA_ON_Msk;
}

/**
 * Trigger taking a sample.
 */
static int
da1469x_gpadc_sample(struct adc_dev *adev)
{
    struct da1469x_gpadc_dev *dev;
    int sr;

    dev = (struct da1469x_gpadc_dev *)adev;

    /*
     * Disable continous mode (if set), and wait for ADC to stop.
     */
    GPADC->GP_ADC_CTRL_REG &= ~GPADC_GP_ADC_CTRL_REG_GP_ADC_CONT_Msk;
    while (GPADC->GP_ADC_CTRL_REG & GPADC_GP_ADC_CTRL_REG_GP_ADC_START_Msk);

    if (dev->dgd_buf[0]) {
        OS_ENTER_CRITICAL(sr);
        da1469x_gpadc_dma_buf(dev);
        OS_EXIT_CRITICAL(sr);
    }

    /* Start the conversion.  */
    GPADC->GP_ADC_CTRL2_REG |= GPADC_GP_ADC_CTRL2_REG_GP_ADC_DMA_EN_Msk;
    GPADC->GP_ADC_CTRL_REG |= (GPADC_GP_ADC_CTRL_REG_GP_ADC_START_Msk |
                               GPADC_GP_ADC_CTRL_REG_GP_ADC_CONT_Msk);

    return 0;
}

/**
 * Set buffer to read data into.  Implementation of setbuffer handler.
 * Sets both the primary and secondary buffers for DMA.
 */
static int
da1469x_gpadc_set_buffer(struct adc_dev *adev, void *buf1, void *buf2,
                         int buf_len)
{
    struct da1469x_gpadc_dev *dev;
    int sr;

    dev = (struct da1469x_gpadc_dev *)adev;

    OS_ENTER_CRITICAL(sr);
    dev->dgd_buf[0] = buf1;
    dev->dgd_buf[1] = buf2;
    dev->dgd_buf_len = (buf_len / sizeof(uint16_t));
    OS_EXIT_CRITICAL(sr);

    return 0;
}

static int
da1469x_gpadc_release_buffer(struct adc_dev *adev, void *buf, int buf_len)
{
    int sr;
    int rc = 0;
    struct da1469x_gpadc_dev *dev;

    dev = (struct da1469x_gpadc_dev *)adev;

    OS_ENTER_CRITICAL(sr);
    if (dev->dgd_buf[0] == NULL) {
        /*
         * If data RX was stalled, restart it.
         */
        dev->dgd_buf[0] = buf;
        da1469x_gpadc_dma_buf(dev);
        GPADC->GP_ADC_CTRL_REG |= (GPADC_GP_ADC_CTRL_REG_GP_ADC_CONT_Msk |
                                   GPADC_GP_ADC_CTRL_REG_GP_ADC_START_Msk);
    } else if (dev->dgd_buf[1] == NULL) {
        /*
         * If we can fit another buffer, queue it.
         */
        dev->dgd_buf[1] = buf;
    } else {
        rc = OS_EBUSY;
    }
    OS_EXIT_CRITICAL(sr);
    return rc;
}

static int
da1469x_gpadc_read_buffer(struct adc_dev *adev, void *buf, int buf_len, int off,
                          int *result)
{
    uint16_t val;

    memcpy(&val, (uint16_t *)buf + off, sizeof(val));
    if (GPADC->GP_ADC_CTRL_REG & GPADC_GP_ADC_CTRL_REG_GP_ADC_SE_Msk) {
        /*
         * Single-ended.
         */
        *result = val;
    } else {
        /*
         * Differential mode.
         * 0x8000 is 0V. Move that point to be 0.
         */
        *result = val - 0x8000;
    }
    return 0;
}

static int
da1469x_gpadc_size_buffer(struct adc_dev *dev, int chans, int samples)
{
    return sizeof(uint16_t) * chans * samples;
}

static int
da1469x_gpadc_dmairq(void *arg)
{
    struct da1469x_gpadc_dev *dev = (struct da1469x_gpadc_dev *)arg;
    struct adc_dev *adev;
    uint16_t *buf;

    /* swap inactive buf to active slot (if it exists) */
    buf = dev->dgd_buf[0];
    dev->dgd_buf[0] = dev->dgd_buf[1];
    dev->dgd_buf[1] = NULL;

    /*
     * We got DMA interrupt, so it should not be running anymore.
     */
    while (dev->dgd_dma[0]->DMA_CTRL_REG & DMA_DMA0_CTRL_REG_DMA_ON_Msk);

    if (dev->dgd_buf[0]) {
        da1469x_gpadc_dma_buf(dev);
    } else {
        GPADC->GP_ADC_CTRL_REG &= ~GPADC_GP_ADC_CTRL_REG_GP_ADC_CONT_Msk;
    }

    adev = &dev->dgd_adc;
    adev->ad_event_handler_func(adev, NULL, ADC_EVENT_RESULT, buf,
                                dev->dgd_buf_len);

    return 0;
}

static const struct adc_driver_funcs da1469x_gpadc_funcs = {
    .af_configure_channel = da1469x_gpadc_configure_channel,
    .af_sample = da1469x_gpadc_sample,
    .af_read_channel = da1469x_gpadc_read_channel,
    .af_set_buffer = da1469x_gpadc_set_buffer,
    .af_release_buffer = da1469x_gpadc_release_buffer,
    .af_read_buffer = da1469x_gpadc_read_buffer,
    .af_size_buffer = da1469x_gpadc_size_buffer
};

/*
 * Run through calibration sequence as described in datasheet.
 */
static int
da1469x_gpadc_calibrate(struct da1469x_gpadc_dev *dev)
{
    uint32_t adc_off_p;
    uint32_t adc_off_n;
    uint32_t orig_ctrl;
    int i;

    orig_ctrl = GPADC->GP_ADC_CTRL_REG;

    /*
     * Only attempt this few times. If calibration fails, still go ahead.
     */
    for (i = 0; i < 5; i++) {
        /*
         * Step 1. Set up registers.
         */
        GPADC->GP_ADC_OFFP_REG = 0x200;
        GPADC->GP_ADC_OFFN_REG = 0x200;

        GPADC->GP_ADC_CTRL_REG =
          (orig_ctrl & ~GPADC_GP_ADC_CTRL_REG_GP_ADC_SIGN_Msk) |
           GPADC_GP_ADC_CTRL_REG_GP_ADC_MUTE_Msk;

        /*
         * Step 2. Run conversion.
         */
        if (da1469x_gpadc_read_scaled(&adc_off_p)) {
            return -1;
        }

        /*
         * Step 3.
         */
        adc_off_p -= 0x200;

        /*
         * Step 4; reconfigure register.
         */
        GPADC->GP_ADC_CTRL_REG |= GPADC_GP_ADC_CTRL_REG_GP_ADC_SIGN_Msk;

        /*
         * Step 5. Run conversion.
         */
        if (da1469x_gpadc_read_scaled(&adc_off_n)) {
            return -1;
        }

        /*
         * Step 6.
         */
        adc_off_n -= 0x200;

        /*
         * Step 7.
         */
        if (GPADC->GP_ADC_CTRL_REG & GPADC_GP_ADC_CTRL_REG_GP_ADC_SE_Msk) {
            GPADC->GP_ADC_OFFP_REG = 0x200 - (adc_off_p * 2);
            GPADC->GP_ADC_OFFN_REG = 0x200 - (adc_off_n * 2);
        } else {
            GPADC->GP_ADC_OFFP_REG = 0x200 - adc_off_p;
            GPADC->GP_ADC_OFFN_REG = 0x200 - adc_off_n;
        }

        /*
         * Step 8. Verify results.
         */
        GPADC->GP_ADC_CTRL_REG &= ~GPADC_GP_ADC_CTRL_REG_GP_ADC_SIGN_Msk;
        if (da1469x_gpadc_read_scaled(&adc_off_p)) {
            return -1;
        }
        adc_off_p = abs(0x200 - adc_off_p);
        if (adc_off_p < 0x8) {
            break;
        }
    }
    assert(i != 5);
    GPADC->GP_ADC_CTRL_REG = orig_ctrl;
    return 0;
}

static int
da1469x_gpadc_hwinit(struct da1469x_gpadc_dev *dev,
                     struct da1469x_gpadc_dev_cfg *dgdc)
{
    struct da1469x_gpadc_init_cfg *dgic;
    uint32_t reg;
    int pin0;
    int pin1;

    dgic = dev->dgd_init_cfg;

    CRG_TOP->LDO_VDDD_HIGH_CTRL_REG |=
      CRG_TOP_LDO_VDDD_HIGH_CTRL_REG_LDO_VDDD_HIGH_ENABLE_Msk;

    /*
     * ADC logic part clocked with the ADC_CLK (16 MHz or 96 MHz)
     * selected with CLK_PER_REG[ADC_CLK_SEL].
     */
    reg = CRG_PER->CLK_PER_REG;
    reg &= ~CRG_PER_CLK_PER_REG_GPADC_CLK_SEL_Msk;
    reg |= dgic->dgic_adc_clk_div;
    CRG_PER->CLK_PER_REG = reg;

    da1469x_gpadc_resolve_pins(dgdc->dgdc_gpadc_ctrl, &pin0, &pin1);
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
    reg = dgdc->dgdc_gpadc_ctrl &
      (GPADC_GP_ADC_CTRL_REG_GP_ADC_CLK_SEL_Msk |
        GPADC_GP_ADC_CTRL_REG_GP_ADC_SE_Msk |
        GPADC_GP_ADC_CTRL_REG_GP_ADC_MUTE_Msk |
        GPADC_GP_ADC_CTRL_REG_GP_ADC_SEL_Msk |
        GPADC_GP_ADC_CTRL_REG_GP_ADC_SIGN_Msk |
        GPADC_GP_ADC_CTRL_REG_GP_ADC_CHOP_Msk |
        GPADC_GP_ADC_CTRL_REG_GP_ADC_LDO_ZERO_Msk |
        GPADC_GP_ADC_CTRL_REG_GP_ADC_DIFF_TEMP_SEL_Msk |
        GPADC_GP_ADC_CTRL_REG_GP_ADC_DIFF_TEMP_EN_Msk);
    GPADC->GP_ADC_CTRL_REG = reg | GPADC_GP_ADC_CTRL_REG_GP_ADC_EN_Msk;

    reg = dgdc->dgdc_gpadc_ctrl2 &
      (GPADC_GP_ADC_CTRL2_REG_GP_ADC_ATTN3X_Msk |
        GPADC_GP_ADC_CTRL2_REG_GP_ADC_IDYN_Msk |
        GPADC_GP_ADC_CTRL2_REG_GP_ADC_I20U_Msk |
        GPADC_GP_ADC_CTRL2_REG_GP_ADC_CONV_NRS_Msk |
        GPADC_GP_ADC_CTRL2_REG_GP_ADC_SMPL_TIME_Msk |
        GPADC_GP_ADC_CTRL2_REG_GP_ADC_STORE_DEL_Msk);
    GPADC->GP_ADC_CTRL2_REG = reg;

    reg = dgdc->dgdc_gpadc_ctrl3 &
      (GPADC_GP_ADC_CTRL3_REG_GP_ADC_EN_DEL_Msk |
        GPADC_GP_ADC_CTRL3_REG_GP_ADC_INTERVAL_Msk);
    GPADC->GP_ADC_CTRL3_REG = reg;

    if (dgdc->dgdc_gpadc_set_offp) {
        reg = dgdc->dgdc_gpadc_offp & GPADC_GP_ADC_OFFP_REG_GP_ADC_OFFP_Msk;
        GPADC->GP_ADC_OFFP_REG = reg;
    }
    if (dgdc->dgdc_gpadc_set_offn) {
        reg = dgdc->dgdc_gpadc_offn & GPADC_GP_ADC_OFFN_REG_GP_ADC_OFFN_Msk;
        GPADC->GP_ADC_OFFN_REG = reg;
    }

    if (dgdc->dgdc_gpadc_autocalibrate) {
        return da1469x_gpadc_calibrate(dev);
    }

    return 0;
}

static int
da1469x_gpadc_open(struct os_dev *odev, uint32_t wait, void *arg)
{
    struct da1469x_gpadc_dev *dev;
    struct da1469x_dma_config cfg = {
        .src_inc = false,
        .dst_inc = true,
        .bus_width = MCU_DMA_BUS_WIDTH_2B,
        .burst_mode = MCU_DMA_BURST_MODE_DISABLED,
    };
    int rc;
    int dma_cidx;

    dev = (struct da1469x_gpadc_dev *)odev;

    rc = os_mutex_pend(&dev->dgd_adc.ad_lock, wait);
    if (rc != OS_OK && rc != OS_NOT_STARTED) {
        return rc;
    }
    if (++dev->dgd_adc.ad_ref_cnt == 1) {
        da1469x_pd_acquire(MCU_PD_DOMAIN_PER);

        /* initialize */
        if (da1469x_gpadc_hwinit(dev, arg)) {
            rc = OS_EINVAL;
        }
        dma_cidx = dev->dgd_init_cfg->dgic_dma_cidx;
        cfg.priority = dev->dgd_init_cfg->dgic_dma_prio;
        if (da1469x_dma_acquire_periph(dma_cidx, MCU_DMA_PERIPH_GPADC,
                                       dev->dgd_dma)) {
            rc = OS_ERROR;
        }
        da1469x_dma_configure(dev->dgd_dma[0], &cfg, da1469x_gpadc_dmairq, dev);
        dev->dgd_dma[0]->DMA_A_START_REG = (uint32_t)&GPADC->GP_ADC_RESULT_REG;
    }
    if (rc) {
        da1469x_pd_release(MCU_PD_DOMAIN_PER);
        --dev->dgd_adc.ad_ref_cnt;
    }
    os_mutex_release(&dev->dgd_adc.ad_lock);
    return rc;
}

static int
da1469x_gpadc_close(struct os_dev *odev)
{
    struct da1469x_gpadc_dev *dev;
    int rc;
    int pin0, pin1;

    dev = (struct da1469x_gpadc_dev *)odev;

    rc = os_mutex_pend(&dev->dgd_adc.ad_lock, OS_TIMEOUT_NEVER);
    if (rc != OS_OK && rc != OS_NOT_STARTED) {
        return rc;
    }
    if (--dev->dgd_adc.ad_ref_cnt == 0) {
        da1469x_dma_release_channel(dev->dgd_dma[0]);
        /*
         * uninit
         * XXXX what state should the pins be left in?
         */
        da1469x_gpadc_resolve_pins(GPADC->GP_ADC_CTRL_REG, &pin0, &pin1);
        if (pin0 >= 0) {
            mcu_gpio_set_pin_function(pin0, MCU_GPIO_MODE_INPUT_PULLDOWN, 0);
        }
        if (pin1 >= 0) {
            mcu_gpio_set_pin_function(pin0, MCU_GPIO_MODE_INPUT_PULLDOWN, 0);
        }
        GPADC->GP_ADC_CTRL_REG = 0;
        CRG_TOP->LDO_VDDD_HIGH_CTRL_REG &=
          ~CRG_TOP_LDO_VDDD_HIGH_CTRL_REG_LDO_VDDD_HIGH_ENABLE_Msk;

        da1469x_pd_release(MCU_PD_DOMAIN_PER);
    }
    os_mutex_release(&dev->dgd_adc.ad_lock);
    return rc;
}

int
da1469x_gpadc_init(struct os_dev *odev, void *arg)
{
    struct da1469x_gpadc_dev *dev;

    dev = (struct da1469x_gpadc_dev *)odev;
    da1469x_gpadc_dev = dev;

    os_mutex_init(&dev->dgd_adc.ad_lock);

    dev->dgd_adc.ad_chans = &dev->dgd_adc_chan;
    dev->dgd_adc.ad_chan_count = 1;

    OS_DEV_SETHANDLERS(odev, da1469x_gpadc_open, da1469x_gpadc_close);

    dev->dgd_init_cfg = arg;

    dev->dgd_adc.ad_funcs = &da1469x_gpadc_funcs;

    return 0;
}

#if MYNEWT_VAL(GPADC_BATTERY)

static struct da1469x_gpadc_dev_cfg os_bsp_gpadc_battery_cfg = {
    .dgdc_gpadc_ctrl = (1U << GPADC_GP_ADC_CTRL_REG_GP_ADC_SE_Pos) |
                       (8U << GPADC_GP_ADC_CTRL_REG_GP_ADC_SEL_Pos),
    .dgdc_gpadc_ctrl2 = 0,
    .dgdc_gpadc_ctrl3 = 0,
    .dgdc_gpadc_set_offp = 0,
    .dgdc_gpadc_set_offn = 0,
    .dgdc_gpadc_offp = 0,
    .dgdc_gpadc_offn = 0,
};

struct os_dev *
da1469x_open_battery_adc(const char *dev_name, uint32_t wait)
{
    struct os_dev *adc = os_dev_open(dev_name, wait, &os_bsp_gpadc_battery_cfg);
    if (adc) {
        /* call adc_chan_config to setup correct multiplier so read returns
         * value in mV */
        adc_chan_config((struct adc_dev *)adc, 0, NULL);
    }
    return adc;
}

#endif
