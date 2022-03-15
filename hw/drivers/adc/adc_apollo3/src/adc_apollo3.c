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
#include "os/mynewt.h"
#include <hal/hal_bsp.h>
#include <adc/adc.h>
#include <mcu/cmsis_nvic.h>

/* Ambiq Apollo3 header */
#include "am_mcu_apollo.h"

#include "adc_apollo3/adc_apollo3.h"

/* Each slot can have a channel setting described by am_hal_adc_slot_chan_e */
static struct adc_chan_config g_apollo3_adc_chans[AM_HAL_ADC_MAX_SLOTS];
void *g_apollo3_adc_handle;

uint32_t g_apollo3_timer_int_lut[APOLLO3_ADC_TIMER_AB_CNT][APOLLO3_ADC_CLOCK_CNT] = {
    {AM_HAL_CTIMER_INT_TIMERA0, AM_HAL_CTIMER_INT_TIMERA1, AM_HAL_CTIMER_INT_TIMERA2, AM_HAL_CTIMER_INT_TIMERA3},
    {AM_HAL_CTIMER_INT_TIMERB0, AM_HAL_CTIMER_INT_TIMERB1, AM_HAL_CTIMER_INT_TIMERB2, AM_HAL_CTIMER_INT_TIMERB3}
};

/* ADC DMA complete flag. */
volatile bool                   g_adc_dma_complete;

/* ADC DMA error flag. */
volatile bool                   g_adc_dma_error;

static void
init_adc_timer(struct apollo3_adc_clk_cfg *cfg)
{
    uint32_t ctimer;
    uint32_t timer_int = 0;

    /* 
    * Timer 3A is a special case timer that can trigger for the ADC
    * (Apollo3 Blue MCU Datasheet v1.0.1 Section 19.4.2). Support for other clocks to be added later
    */
    assert(cfg->clk_num == APOLLO3_ADC_CLOCK_3);

    /* Use cfg to craft timer args */
    switch (cfg->timer_ab) {
        case APOLLO3_ADC_TIMER_A:
            ctimer = AM_HAL_CTIMER_TIMERA;
            break;
        case APOLLO3_ADC_TIMER_B:
            ctimer = AM_HAL_CTIMER_TIMERB;
            break;
        case APOLLO3_ADC_TIMER_BOTH:
            ctimer = AM_HAL_CTIMER_BOTH;
            break;
        default:
            ctimer = 0;
            break;
    }
    assert(ctimer != 0);

    /* Start a timer to trigger the ADC periodically (1 second). */
    am_hal_ctimer_config_single(cfg->clk_num, ctimer,
                                AM_HAL_CTIMER_HFRC_12MHZ    |
                                AM_HAL_CTIMER_FN_REPEAT     |
                                AM_HAL_CTIMER_INT_ENABLE);
    
    if (cfg->timer_ab == APOLLO3_ADC_TIMER_A || cfg->timer_ab == APOLLO3_ADC_TIMER_BOTH) {
        timer_int |= g_apollo3_timer_int_lut[0][cfg->clk_num];
    }
    if (cfg->timer_ab == APOLLO3_ADC_TIMER_B || cfg->timer_ab == APOLLO3_ADC_TIMER_BOTH) {
        timer_int |= g_apollo3_timer_int_lut[1][cfg->clk_num];
    }
    
    am_hal_ctimer_int_enable(timer_int);

    am_hal_ctimer_period_set(cfg->clk_num, ctimer, cfg->clk_period, cfg->clk_on_time);

    if (cfg->clk_num == APOLLO3_ADC_CLOCK_3) {
        /* Enable the timer A3 to trigger the ADC directly */
        am_hal_ctimer_adc_trigger_enable();
    }

    /* Start the timer. */
    am_hal_ctimer_start(cfg->clk_num, ctimer);
}

/**
 * Open the Apollo3 ADC device
 *
 * This function locks the device for access from other tasks.
 *
 * @param odev The OS device to open
 * @param wait The time in MS to wait.  If 0 specified, returns immediately
 *             if resource unavailable.  If OS_WAIT_FOREVER specified, blocks
 *             until resource is available.
 * @param arg  Argument provided by higher layer to open, in this case
 *             it can be a adc_cfg, to override the default
 *             configuration.
 *
 * @return 0 on success, non-zero on failure.
 */
static int
apollo3_adc_open(struct os_dev *odev, uint32_t wait, void *arg)
{
    int rc = 0;
    int unlock = 0;
    struct adc_dev *dev = (struct adc_dev *) odev;
    struct adc_cfg *adc_config = (struct adc_cfg *) arg;

    if (!adc_config) {
        adc_config = dev->ad_dev.od_init_arg;
    }

    /* Configuration must be set before opening adc or it must be passed in */
    assert(adc_config != NULL);

    if (os_started()) {
        rc = os_mutex_pend(&dev->ad_lock, wait);
        if (rc != OS_OK) {
            goto err;
        }
        unlock = 1;
    }

    /* Initialize the ADC and get the handle. */
    am_hal_adc_initialize(0, &g_apollo3_adc_handle);

    /* Power on the ADC. */
    am_hal_adc_power_control(g_apollo3_adc_handle, AM_HAL_SYSCTRL_WAKE, false);

    /* Set up the ADC configuration parameters.*/
    am_hal_adc_configure(g_apollo3_adc_handle, &(adc_config->adc_cfg));

    /* ad_chan_count holds number of slots, each slot can configure a channel */
    for (int slot = 0; slot < dev->ad_chan_count; slot++) {
        /* Set up an ADC slot */
        am_hal_adc_configure_slot(g_apollo3_adc_handle, slot, &(adc_config->adc_slot_cfg));
    }

    /* Configure the ADC to use DMA for the sample transfer. */
    am_hal_adc_configure_dma(g_apollo3_adc_handle, &(adc_config->adc_dma_cfg));
    g_adc_dma_complete = false;
    g_adc_dma_error = false;

    /* Wake up for each adc interrupt by default */
    am_hal_adc_interrupt_enable(g_apollo3_adc_handle, AM_HAL_ADC_INT_DERR | AM_HAL_ADC_INT_DCMP );

    /* Enable the ADC. */
    am_hal_adc_enable(g_apollo3_adc_handle);

    /* Start timer for ADC measurements */
    init_adc_timer(&(adc_config->clk_cfg));

    /* Enable adc irq */
    NVIC_EnableIRQ(ADC_IRQn);
    am_hal_interrupt_master_enable();

    /* Trigger manually the first time */
    am_hal_adc_sw_trigger(g_apollo3_adc_handle);

err:
    if (unlock) {
        os_mutex_release(&dev->ad_lock);
    }

    return rc;
}

/**
 * Close the Apollo3 ADC device.
 *
 * This function unlocks the device.
 *
 * @param odev The device to close.
 */
static int
apollo3_adc_close(struct os_dev *odev)
{
    struct adc_dev *dev;
    int rc = 0;
    int unlock = 0;

    dev = (struct adc_dev *) odev;

    if (os_started()) {
        rc = os_mutex_pend(&dev->ad_lock, OS_TIMEOUT_NEVER);
        if (rc != OS_OK) {
            goto err;
        }
        unlock = 1;
    }
    
    /* Initialize the ADC and get the handle. */
    am_hal_adc_deinitialize(&g_apollo3_adc_handle);

    /* Power on the ADC. */
    am_hal_adc_power_control(g_apollo3_adc_handle, AM_HAL_SYSCTRL_NORMALSLEEP, false);

    /* Wake up for each adc interrupt by default */
    am_hal_adc_interrupt_disable(g_apollo3_adc_handle, AM_HAL_ADC_INT_DERR | AM_HAL_ADC_INT_DCMP);

    /* Enable the ADC. */
    am_hal_adc_disable(g_apollo3_adc_handle);

err:
    if (unlock) {
        os_mutex_release(&dev->ad_lock);
    }

    return rc;
}

/**
 * Configure an ADC channel on the Nordic ADC.
 *
 * @param dev The ADC device to configure
 * @param cnum The channel on the ADC device to configure
 * @param cfgdata An opaque pointer to channel config, expected to be
 *                a adc_cfg
 *
 * @return 0 on success, non-zero on failure.
 */
static int
apollo3_adc_configure_channel(struct adc_dev *dev, uint8_t cnum, void *cfgdata)
{
    struct adc_cfg *adc_config = (struct adc_cfg *) cfgdata;

    /* Update device's args */
    dev->ad_dev.od_init_arg = adc_config;
    
    if (cnum >= AM_HAL_ADC_MAX_SLOTS) {
        return OS_EINVAL;
    }

    /* Set up the ADC configuration parameters.*/
    am_hal_adc_configure(g_apollo3_adc_handle, &(adc_config->adc_cfg));

    /* Set up an ADC slot */
    am_hal_adc_configure_slot(g_apollo3_adc_handle, cnum, &(adc_config->adc_slot_cfg));

    /* Configure the ADC to use DMA for the sample transfer. */
    am_hal_adc_configure_dma(g_apollo3_adc_handle, &(adc_config->adc_dma_cfg));
    g_adc_dma_complete = false;
    g_adc_dma_error = false;

    /* Store these values in channel definitions, for conversions to
     * milivolts.
     */
    dev->ad_chans[cnum].c_cnum = cnum;
    dev->ad_chans[cnum].c_res = adc_config->adc_slot_cfg.ePrecisionMode;
    dev->ad_chans[cnum].c_refmv = adc_config->adc_cfg.eReference;
    dev->ad_chans[cnum].c_configured = 1;

    return 0;
}

/**
 * Set buffer to read data into. Implementation of setbuffer handler.
 * Apollo3 cfg takes one buffer
 */
static int
apollo3_adc_set_buffer(struct adc_dev *dev, void *buf1, void *buf2, int buf_len)
{
    am_hal_adc_dma_config_t cfg;
    assert(dev);
    assert(buf1);

    if (buf_len <= 0) {
        return OS_EINVAL;
    }

    cfg = ((struct adc_cfg *)(dev->ad_dev.od_init_arg))->adc_dma_cfg;
    cfg.bDynamicPriority = true;
    cfg.ePriority = AM_HAL_ADC_PRIOR_SERVICE_IMMED;
    cfg.bDMAEnable = true;
    cfg.ui32TargetAddress = (uint32_t)buf1;
    cfg.ui32SampleCount = buf_len / sizeof(am_hal_adc_sample_t);

    if (AM_HAL_STATUS_SUCCESS != am_hal_adc_configure_dma(g_apollo3_adc_handle, &cfg)) {
        return OS_EINVAL;
    }
    g_adc_dma_complete = false;
    g_adc_dma_error = false;

    return 0;
}

static int
apollo3_adc_release_buffer(struct adc_dev *dev, void *buf, int buf_len)
{
    am_hal_adc_dma_config_t cfg;
    assert(dev);
    assert(buf);

    if (buf_len <= 0) {
        return OS_EINVAL;
    }

    cfg = ((struct adc_cfg *)(dev->ad_dev.od_init_arg))->adc_dma_cfg;
    cfg.bDMAEnable = false;
    cfg.ui32TargetAddress = (uint32_t)buf;
    cfg.ui32SampleCount = buf_len / sizeof(am_hal_adc_sample_t);

    if (AM_HAL_STATUS_SUCCESS != am_hal_adc_configure_dma(g_apollo3_adc_handle, &cfg)) {
        return OS_EINVAL;
    }
    g_adc_dma_complete = false;
    g_adc_dma_error = false;

    return 0;
}

/**
 * Trigger an ADC sample.
 */
static int
apollo3_adc_sample(struct adc_dev *dev)
{
    if (AM_HAL_STATUS_SUCCESS != am_hal_adc_sw_trigger(g_apollo3_adc_handle)) {
        return OS_EINVAL;
    }

    return 0;
}

/**
 * Blocking read of an ADC channel, returns result as an integer.
 */
static int
apollo3_adc_read_channel(struct adc_dev *dev, uint8_t cnum, int *result)
{
    int rc;
    int unlock = 0;
    struct adc_cfg * cfg= dev->ad_dev.od_init_arg;
    am_hal_adc_sample_t sample[cfg->adc_dma_cfg.ui32SampleCount];

    if (os_started()) {
        rc = os_mutex_pend(&dev->ad_lock, OS_TIMEOUT_NEVER);
        if (rc != OS_OK) {
            goto err;
        }
        unlock = 1;
    }

    memset(sample, 0, sizeof(am_hal_adc_sample_t) * cfg->adc_dma_cfg.ui32SampleCount);

    am_hal_adc_sw_trigger(g_apollo3_adc_handle);

    /* Blocking read */
    while (1) {
        assert(g_adc_dma_error != true);
        if (g_adc_dma_complete) {
            if (AM_HAL_STATUS_SUCCESS != am_hal_adc_samples_read(g_apollo3_adc_handle, true, (uint32_t *)cfg->adc_dma_cfg.ui32TargetAddress, &(cfg->adc_dma_cfg.ui32SampleCount), sample)) {
                rc = OS_EINVAL;
                goto err;
            }

            am_hal_adc_configure_dma(g_apollo3_adc_handle, &(cfg->adc_dma_cfg));
            g_adc_dma_complete = false;
            g_adc_dma_error = false;

            am_hal_adc_interrupt_clear(g_apollo3_adc_handle, 0xFFFFFFFF);
            break;
        }
    }

    *result = (int) sample[0].ui32Sample;
    rc = 0;

err:
    if (unlock) {
        os_mutex_release(&dev->ad_lock);
    }
    return rc;
}

static int
apollo3_adc_read_buffer(struct adc_dev *dev, void *buf, int buf_len, int off, int *result)
{
    am_hal_adc_sample_t val;
    int data_off;

    data_off = off * sizeof(am_hal_adc_sample_t);
    assert(data_off < buf_len);

    val = *(am_hal_adc_sample_t *) ((uint8_t *) buf + data_off);
    *result = (int)val.ui32Sample;

    return 0;
}

static int
apollo3_adc_size_buffer(struct adc_dev *dev, int chans, int samples)
{
    return sizeof(am_hal_adc_sample_t) * chans * samples;
}

void apollo3_irq_handler(void) 
{
    uint32_t adc_int_mask;

    /* Read the interrupt status. */
    am_hal_adc_interrupt_status(g_apollo3_adc_handle, &adc_int_mask, false);

    /* Clear the ADC interrupt. */
    am_hal_adc_interrupt_clear(g_apollo3_adc_handle, adc_int_mask);

    /* If we got a DMA complete, set the flag. */
    if (adc_int_mask & AM_HAL_ADC_INT_DCMP) {
        g_adc_dma_complete = true;
    }

    /* If we got a DMA error, set the flag. */
    if (adc_int_mask & AM_HAL_ADC_INT_DERR) {
        g_adc_dma_error = true;
    }
}

#if MYNEWT_VAL(OS_SYSVIEW)
static void
sysview_irq_handler(void)
{
    os_trace_isr_enter();
    apollo3_irq_handler();
    os_trace_isr_exit();
}
#endif

/**
 * ADC device driver functions
 */
static const struct adc_driver_funcs apollo3_adc_funcs = {
        .af_configure_channel = apollo3_adc_configure_channel,
        .af_sample = apollo3_adc_sample,
        .af_read_channel = apollo3_adc_read_channel,
        .af_set_buffer = apollo3_adc_set_buffer,
        .af_release_buffer = apollo3_adc_release_buffer,
        .af_read_buffer = apollo3_adc_read_buffer,
        .af_size_buffer = apollo3_adc_size_buffer,
};

/**
 * Callback to initialize an adc_dev structure from the os device
 * initialization callback.  This sets up an apollo3 adc device, so
 * that subsequent lookups to this device allow us to manipulate it.
 */
int
apollo3_adc_dev_init(struct os_dev *odev, void *arg)
{
    struct adc_dev *dev;
    dev = (struct adc_dev *) odev;

    os_mutex_init(&dev->ad_lock);

    dev->ad_chans = (void *) g_apollo3_adc_chans;
    dev->ad_chan_count = AM_HAL_ADC_MAX_SLOTS;
    dev->ad_dev.od_init_arg = (struct adc_cfg *) arg;

    OS_DEV_SETHANDLERS(odev, apollo3_adc_open, apollo3_adc_close);
    dev->ad_funcs = &apollo3_adc_funcs;

#if MYNEWT_VAL(OS_SYSVIEW)
    NVIC_SetVector(ADC_IRQn, (uint32_t) sysview_irq_handler);
#else
    NVIC_SetVector(ADC_IRQn, (uint32_t) apollo3_irq_handler);
#endif

    return 0;
}
