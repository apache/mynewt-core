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

/* Nordic headers */
#include <nrf_saadc.h>

#include "adc_nrf52/adc_nrf52.h"

struct nrf52_saadc_stats {
    uint16_t saadc_events;
    uint16_t saadc_events_failed;
};
static struct nrf52_saadc_stats nrf52_saadc_stats;

static struct nrf52_adc_dev_cfg *init_cfg;
static struct adc_dev *global_adc_dev;
static struct adc_chan_config nrf52_adc_chans[SAADC_CH_NUM];

struct nrf52_adc_chan {
    nrf_saadc_input_t pin_p;
    nrf_saadc_input_t pin_n;
    nrf_saadc_channel_config_t nrf_chan;
};

struct nrf52_saadc_dev_global {
    nrf_saadc_value_t *primary_buf;
    nrf_saadc_value_t *secondary_buf;
    uint16_t primary_size;
    uint16_t secondary_size;
    nrf_saadc_resolution_t resolution;
    nrf_saadc_oversample_t oversample;
    struct nrf52_adc_chan channels[SAADC_CH_NUM];
    bool calibrate;
};

static struct nrf52_saadc_dev_global g_drv_instance;

/**
 * Initialize a channel with default/unconfigured values.
 */
static void
channel_unconf(int cnum)
{
    nrf_saadc_channel_config_t default_ch = {
        .gain = NRF_SAADC_GAIN1_6,
        .reference = NRF_SAADC_REFERENCE_INTERNAL,
        .acq_time = NRF_SAADC_ACQTIME_10US,
        .mode = NRF_SAADC_MODE_SINGLE_ENDED,
        .burst = NRF_SAADC_BURST_DISABLED,
        .resistor_p = NRF_SAADC_RESISTOR_DISABLED,
        .resistor_n = NRF_SAADC_RESISTOR_DISABLED,
    };

    memcpy(&g_drv_instance.channels[cnum].nrf_chan,
           &default_ch,
           sizeof(nrf_saadc_channel_config_t));
    g_drv_instance.channels[cnum].pin_p = NRF_SAADC_INPUT_DISABLED;
    g_drv_instance.channels[cnum].pin_n = NRF_SAADC_INPUT_DISABLED;
    global_adc_dev->ad_chans[cnum].c_configured = 0;
}

/**
 * Initialize a driver instance with default/unconfigured values.
 */
static void
init_instance_unconf(void)
{
    int cnum;

    g_drv_instance.primary_buf = NULL;
    g_drv_instance.secondary_buf = NULL;
    g_drv_instance.primary_size = 0;
    g_drv_instance.secondary_size = 0;
    g_drv_instance.resolution = NRF_SAADC_RESOLUTION_14BIT;
    g_drv_instance.oversample = NRF_SAADC_OVERSAMPLE_DISABLED;
    g_drv_instance.calibrate = false;

    for (cnum = 0; cnum < SAADC_CH_NUM; cnum++) {
        channel_unconf(cnum);
    }
}

/**
 * Unconfigures the device registers
 */
static void
clear_device_regs(void)
{
    int cnum;
    nrf_saadc_channel_config_t default_ch = {
        .gain = NRF_SAADC_GAIN1_6,
        .reference = NRF_SAADC_REFERENCE_INTERNAL,
        .acq_time = NRF_SAADC_ACQTIME_10US,
        .mode = NRF_SAADC_MODE_SINGLE_ENDED,
        .burst = NRF_SAADC_BURST_DISABLED,
        .resistor_p = NRF_SAADC_RESISTOR_DISABLED,
        .resistor_n = NRF_SAADC_RESISTOR_DISABLED,
    };

    nrf_saadc_int_disable(NRF_SAADC, NRF_SAADC_INT_ALL);
    nrf_saadc_event_clear(NRF_SAADC, NRF_SAADC_EVENT_END);
    nrf_saadc_event_clear(NRF_SAADC, NRF_SAADC_EVENT_STARTED);

    for (cnum = 0; cnum < SAADC_CH_NUM; cnum++) {
        nrf_saadc_channel_init(NRF_SAADC, cnum, &default_ch);
        nrf_saadc_channel_input_set(NRF_SAADC, cnum, NRF_SAADC_INPUT_DISABLED,
                                    NRF_SAADC_INPUT_DISABLED);
    }
}

/**
 * Open the NRF52 ADC device
 *
 * This function locks the device for access from other tasks.
 *
 * @param odev The OS device to open
 * @param wait The time in MS to wait.  If 0 specified, returns immediately
 *             if resource unavailable.  If OS_WAIT_FOREVER specified, blocks
 *             until resource is available.
 * @param arg  Argument provided by higher layer to open, in this case
 *             it can be a adc_dev_cfg, to override the default
 *             configuration.
 *
 * @return 0 on success, non-zero on failure.
 */
static int
nrf52_adc_open(struct os_dev *odev, uint32_t wait, void *arg)
{
    int rc = 0;
    int unlock = 0;
    struct adc_dev *dev = (struct adc_dev *) odev;
    struct adc_dev_cfg *adc_config = (struct adc_dev_cfg *) arg;

    if (os_started()) {
        rc = os_mutex_pend(&dev->ad_lock, wait);
        if (rc != OS_OK) {
            goto err;
        }
        unlock = 1;
    }

    if (++(dev->ad_ref_cnt) == 1) {
        init_instance_unconf();
        NVIC_SetPriority(SAADC_IRQn, 0);
        NVIC_EnableIRQ(SAADC_IRQn);
        global_adc_dev = dev;

        if (adc_config) {
            switch (adc_config->resolution) {
            case ADC_RESOLUTION_8BIT:
                g_drv_instance.resolution = NRF_SAADC_RESOLUTION_8BIT;
                break;
            case ADC_RESOLUTION_10BIT:
                g_drv_instance.resolution = NRF_SAADC_RESOLUTION_10BIT;
                break;
            case ADC_RESOLUTION_12BIT:
                g_drv_instance.resolution = NRF_SAADC_RESOLUTION_12BIT;
                break;
            case ADC_RESOLUTION_14BIT:
                g_drv_instance.resolution = NRF_SAADC_RESOLUTION_14BIT;
                break;
            default:
                assert(0);
            }

            switch (adc_config->oversample) {
            case ADC_OVERSAMPLE_DISABLED:
                g_drv_instance.oversample = NRF_SAADC_OVERSAMPLE_DISABLED;
                break;
            case ADC_OVERSAMPLE_2X:
                g_drv_instance.oversample = NRF_SAADC_OVERSAMPLE_2X;
                break;
            case ADC_OVERSAMPLE_4X:
                g_drv_instance.oversample = NRF_SAADC_OVERSAMPLE_4X;
                break;
            case ADC_OVERSAMPLE_8X:
                g_drv_instance.oversample = NRF_SAADC_OVERSAMPLE_8X;
                break;
            case ADC_OVERSAMPLE_16X:
                g_drv_instance.oversample = NRF_SAADC_OVERSAMPLE_16X;
                break;
            case ADC_OVERSAMPLE_32X:
                g_drv_instance.oversample = NRF_SAADC_OVERSAMPLE_32X;
                break;
            case ADC_OVERSAMPLE_64X:
                g_drv_instance.oversample = NRF_SAADC_OVERSAMPLE_64X;
                break;
            case ADC_OVERSAMPLE_128X:
                g_drv_instance.oversample = NRF_SAADC_OVERSAMPLE_128X;
                break;
            case ADC_OVERSAMPLE_256X:
                g_drv_instance.oversample = NRF_SAADC_OVERSAMPLE_256X;
                break;
            default:
                assert(0);
            }

            g_drv_instance.calibrate = adc_config->calibrate;
        }
    }
err:
    if (unlock) {
        os_mutex_release(&dev->ad_lock);
    }

    return rc;
}

/**
 * Close the NRF52 ADC device.
 *
 * This function unlocks the device.
 *
 * @param odev The device to close.
 */
static int
nrf52_adc_close(struct os_dev *odev)
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
    if (--(dev->ad_ref_cnt) == 0) {
        NVIC_DisableIRQ(SAADC_IRQn);
        if (nrf_saadc_busy_check(NRF_SAADC)) {
            nrf_saadc_event_clear(NRF_SAADC, NRF_SAADC_EVENT_STOPPED);
            nrf_saadc_task_trigger(NRF_SAADC, NRF_SAADC_TASK_STOP);

            while (!nrf_saadc_event_check(NRF_SAADC, NRF_SAADC_EVENT_STOPPED));

            nrf_saadc_event_clear(NRF_SAADC, NRF_SAADC_EVENT_STOPPED);
            nrf_saadc_event_clear(NRF_SAADC, NRF_SAADC_EVENT_END);
        }

        nrf_saadc_disable(NRF_SAADC);
        init_instance_unconf();
        clear_device_regs();

        global_adc_dev = NULL;
    }

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
 *                a nrf_saadc_channel_config_t
 *
 * @return 0 on success, non-zero on failure.
 */
static int
nrf52_adc_configure_channel(struct adc_dev *dev, uint8_t cnum, void *cfgdata)
{
    struct adc_chan_cfg *cfg = (struct adc_chan_cfg *) cfgdata;
    uint16_t refmv;
    nrf_saadc_resolution_t res;

    if (cnum >= SAADC_CH_NUM) {
        return OS_EINVAL;
    }

    if (nrf_saadc_busy_check(NRF_SAADC)) {
        return OS_EBUSY;
    }

    channel_unconf(cnum);
    dev->ad_chans[cnum].c_configured = 0;
    if (!cfgdata) {
        nrf_saadc_channel_init(NRF_SAADC, cnum,
                               &g_drv_instance.channels[cnum].nrf_chan);
        return 0;
    }

    switch (g_drv_instance.resolution) {
    case NRF_SAADC_RESOLUTION_8BIT:
        res = 8;
        break;
    case NRF_SAADC_RESOLUTION_10BIT:
        res = 10;
        break;
    case NRF_SAADC_RESOLUTION_12BIT:
        res = 12;
        break;
    case NRF_SAADC_RESOLUTION_14BIT:
        res = 14;
        break;
    default:
        assert(0);
    }

    switch (cfg->reference) {
    case ADC_REFERENCE_INTERNAL:
        g_drv_instance.channels[cnum].nrf_chan.reference =
            NRF_SAADC_REFERENCE_INTERNAL;
        refmv = 600; /* 0.6V for NRF52 */
        break;
    case ADC_REFERENCE_VDD_DIV_4:
        g_drv_instance.channels[cnum].nrf_chan.reference =
            NRF_SAADC_REFERENCE_VDD4;
        refmv = init_cfg->nadc_refmv / 4;
        break;
    default:
        assert(0);
    }

    /* Adjust reference voltage for gain. */
    switch (cfg->gain) {
    case ADC_GAIN1_6:
        refmv *= 6;
        break;
    case ADC_GAIN1_5:
        refmv *= 5;
        break;
    case ADC_GAIN1_4:
        refmv *= 4;
        break;
    case ADC_GAIN1_3:
        refmv *= 3;
        break;
    case ADC_GAIN1_2:
        refmv *= 2;
        break;
    case ADC_GAIN2:
        refmv /= 2;
        break;
    case ADC_GAIN4:
        refmv /= 4;
        break;
    default:
        break;
    }

    g_drv_instance.channels[cnum].pin_p = cfg->pin;
    if (cfg->differential) {
        g_drv_instance.channels[cnum].nrf_chan.mode =
            NRF_SAADC_MODE_DIFFERENTIAL;
        g_drv_instance.channels[cnum].pin_n = cfg->pin_negative;
    }

    /* Init Channel's registers */
    nrf_saadc_channel_init(NRF_SAADC, cnum,
                           &g_drv_instance.channels[cnum].nrf_chan);

    /* Store these values in channel definitions, for conversions to
     * milivolts.
     */
    dev->ad_chans[cnum].c_res = res;
    dev->ad_chans[cnum].c_refmv = refmv;
    dev->ad_chans[cnum].c_configured = 1;

    return 0;
}

/**
 * Set buffer to read data into. Implementation of setbuffer handler.
 * Sets both the primary and secondary buffers for DMA.
 */
static int
nrf52_adc_set_buffer(struct adc_dev *dev, void *buf1, void *buf2, int buf_len)
{
    assert(dev);
    assert(buf1);

    if (buf_len <= 0) {
        return OS_EINVAL;
    }

    buf_len /= sizeof(nrf_saadc_value_t);
    g_drv_instance.primary_buf = buf1;
    g_drv_instance.primary_size = buf_len;
    if (buf2) {
        g_drv_instance.secondary_buf = buf2;
        g_drv_instance.secondary_size = buf_len;
    }

    return 0;
}

static int
nrf52_adc_release_buffer(struct adc_dev *dev, void *buf, int buf_len)
{
    assert(dev);
    assert(buf);

    if (buf_len <= 0) {
        return OS_EINVAL;
    }

    buf_len /= sizeof(nrf_saadc_value_t);
    if (!g_drv_instance.primary_buf) {
        g_drv_instance.primary_buf = buf;
        g_drv_instance.primary_size = buf_len;
    } else if (!g_drv_instance.secondary_buf) {
        g_drv_instance.secondary_buf = buf;
        g_drv_instance.secondary_size = buf_len;
    } else {
        return OS_ENOENT;
    }

    return 0;
}

/**
 * Trigger an ADC sample.
 */
static int
nrf52_adc_sample(struct adc_dev *dev)
{
    int cnum;
    int cnum_last = 0;
    int used_chans = 0;
    int size;

    if (nrf_saadc_busy_check(NRF_SAADC)) {
        return OS_EBUSY;
    }

    for (cnum = 0; cnum < SAADC_CH_NUM; cnum++) {
        if (dev->ad_chans[cnum].c_configured) {
            cnum_last = cnum;
            used_chans++;
            nrf_saadc_channel_input_set(NRF_SAADC, cnum,
                                        g_drv_instance.channels[cnum].pin_p,
                                        g_drv_instance.channels[cnum].pin_n);
        }
    }
    assert(used_chans > 0);

    if (g_drv_instance.primary_size < (used_chans * sizeof(nrf_saadc_value_t))) {
        return OS_ENOMEM;
    }

    if ((used_chans == 1) &&
        (g_drv_instance.oversample != NRF_SAADC_OVERSAMPLE_DISABLED)) {
        nrf_saadc_burst_set(NRF_SAADC, cnum_last, NRF_SAADC_BURST_ENABLED);
        nrf_saadc_oversample_set(NRF_SAADC, g_drv_instance.oversample);
    } else {
        nrf_saadc_oversample_set(NRF_SAADC, NRF_SAADC_OVERSAMPLE_DISABLED);
    }

    nrf_saadc_resolution_set(NRF_SAADC, g_drv_instance.resolution);
    size = g_drv_instance.primary_size / sizeof(nrf_saadc_value_t);
    nrf_saadc_buffer_init(NRF_SAADC, g_drv_instance.primary_buf, size);

    nrf_saadc_event_clear(NRF_SAADC, NRF_SAADC_EVENT_END);
    nrf_saadc_event_clear(NRF_SAADC, NRF_SAADC_EVENT_CALIBRATEDONE);
    nrf_saadc_int_enable(NRF_SAADC,
                         NRF_SAADC_INT_END | NRF_SAADC_INT_CALIBRATEDONE);

    /* Start Sampling */
    nrf_saadc_enable(NRF_SAADC);

    if (g_drv_instance.calibrate) {
        nrf_saadc_task_trigger(NRF_SAADC, NRF_SAADC_TASK_CALIBRATEOFFSET);
    } else {
        nrf_saadc_task_trigger(NRF_SAADC, NRF_SAADC_TASK_STOP);
        nrf_saadc_task_trigger(NRF_SAADC, NRF_SAADC_TASK_START);
        nrf_saadc_task_trigger(NRF_SAADC, NRF_SAADC_TASK_SAMPLE);
    }

    return 0;
}

/**
 * Blocking read of an ADC channel, returns result as an integer.
 */
static int
nrf52_adc_read_channel(struct adc_dev *dev, uint8_t cnum, int *result)
{
    int rc;
    int unlock = 0;
    nrf_saadc_value_t adc_value;

    if (nrf_saadc_busy_check(NRF_SAADC)) {
        return OS_EBUSY;
    }

    if (os_started()) {
        rc = os_mutex_pend(&dev->ad_lock, OS_TIMEOUT_NEVER);
        if (rc != OS_OK) {
            goto err;
        }
        unlock = 1;
    }

    /* Channel must be configured */
    if (!dev->ad_chans[cnum].c_configured) {
        rc = OS_EINVAL;
        goto err;
    }

    /* Enable the channel, set pins */
    nrf_saadc_channel_input_set(NRF_SAADC, cnum,
                                g_drv_instance.channels[cnum].pin_p,
                                g_drv_instance.channels[cnum].pin_n);

    if (g_drv_instance.oversample != NRF_SAADC_OVERSAMPLE_DISABLED) {
        nrf_saadc_burst_set(NRF_SAADC, cnum, NRF_SAADC_BURST_ENABLED);
        nrf_saadc_oversample_set(NRF_SAADC, g_drv_instance.oversample);
    } else {
        nrf_saadc_oversample_set(NRF_SAADC, NRF_SAADC_OVERSAMPLE_DISABLED);
    }

    nrf_saadc_resolution_set(NRF_SAADC, g_drv_instance.resolution);
    nrf_saadc_buffer_init(NRF_SAADC, &adc_value, 1);
    nrf_saadc_enable(NRF_SAADC);

    nrf_saadc_task_trigger(NRF_SAADC, NRF_SAADC_TASK_START);
    nrf_saadc_task_trigger(NRF_SAADC, NRF_SAADC_TASK_SAMPLE);
    while (!nrf_saadc_event_check(NRF_SAADC, NRF_SAADC_EVENT_END));

    nrf_saadc_disable(NRF_SAADC);
    nrf_saadc_event_clear(NRF_SAADC, NRF_SAADC_EVENT_STARTED);
    nrf_saadc_event_clear(NRF_SAADC, NRF_SAADC_EVENT_END);

    /* Disable the channel, unset pins */
    nrf_saadc_channel_input_set(NRF_SAADC, cnum,
                                NRF_SAADC_INPUT_DISABLED,
                                NRF_SAADC_INPUT_DISABLED);

    *result = (int) adc_value;
    rc = 0;

err:
    if (unlock) {
        os_mutex_release(&dev->ad_lock);
    }
    return rc;
}

static int
nrf52_adc_read_buffer(struct adc_dev *dev, void *buf, int buf_len, int off,
                      int *result)
{
    nrf_saadc_value_t val;
    int data_off;

    data_off = off * sizeof(nrf_saadc_value_t);
    assert(data_off < buf_len);

    val = *(nrf_saadc_value_t *) ((uint8_t *) buf + data_off);
    *result = val;

    return 0;
}

static int
nrf52_adc_size_buffer(struct adc_dev *dev, int chans, int samples)
{
    return sizeof(nrf_saadc_value_t) * chans * samples;
}

void
nrf52_saadc_irq_handler(void)
{
    void *buf = NULL;
    int bufsize = 0;
    int size;

    if (global_adc_dev == NULL || !global_adc_dev->ad_event_handler_func) {
        ++nrf52_saadc_stats.saadc_events_failed;
        return;
    }
    ++nrf52_saadc_stats.saadc_events;

    if (nrf_saadc_event_check(NRF_SAADC, NRF_SAADC_EVENT_END)) {
        nrf_saadc_event_clear(NRF_SAADC, NRF_SAADC_EVENT_END);

        buf = g_drv_instance.primary_buf;
        bufsize = g_drv_instance.primary_size * sizeof(nrf_saadc_value_t);
        if (g_drv_instance.secondary_buf) {
            g_drv_instance.primary_buf = g_drv_instance.secondary_buf;
            g_drv_instance.primary_size = g_drv_instance.secondary_size;
            g_drv_instance.secondary_buf = NULL;
            g_drv_instance.secondary_size = 0;

            size = g_drv_instance.primary_size / sizeof(nrf_saadc_value_t);
            nrf_saadc_buffer_init(NRF_SAADC, g_drv_instance.primary_buf, size);
        } else {
            nrf_saadc_int_disable(NRF_SAADC, NRF_SAADC_INT_ALL);
            nrf_saadc_task_trigger(NRF_SAADC, NRF_SAADC_TASK_STOP);
            nrf_saadc_disable(NRF_SAADC);
        }

        global_adc_dev->ad_event_handler_func(global_adc_dev,
                                              global_adc_dev->ad_event_handler_arg,
                                              ADC_EVENT_RESULT,
                                              buf, bufsize);
    } else if (nrf_saadc_event_check(NRF_SAADC, NRF_SAADC_EVENT_CALIBRATEDONE)) {
        nrf_saadc_event_clear(NRF_SAADC, NRF_SAADC_EVENT_CALIBRATEDONE);

        global_adc_dev->ad_event_handler_func(global_adc_dev,
                                              global_adc_dev->ad_event_handler_arg,
                                              ADC_EVENT_CALIBRATED,
                                              buf, bufsize);

        nrf_saadc_task_trigger(NRF_SAADC, NRF_SAADC_TASK_STOP);
        nrf_saadc_task_trigger(NRF_SAADC, NRF_SAADC_TASK_START);
        nrf_saadc_task_trigger(NRF_SAADC, NRF_SAADC_TASK_SAMPLE);
    } else {
        ++nrf52_saadc_stats.saadc_events_failed;
    }
}

#if MYNEWT_VAL(OS_SYSVIEW)
static void
sysview_irq_handler(void)
{
    os_trace_isr_enter();
    nrf52_saadc_irq_handler();
    os_trace_isr_exit();
}
#endif

/**
 * ADC device driver functions
 */
static const struct adc_driver_funcs nrf52_adc_funcs = {
        .af_configure_channel = nrf52_adc_configure_channel,
        .af_sample = nrf52_adc_sample,
        .af_read_channel = nrf52_adc_read_channel,
        .af_set_buffer = nrf52_adc_set_buffer,
        .af_release_buffer = nrf52_adc_release_buffer,
        .af_read_buffer = nrf52_adc_read_buffer,
        .af_size_buffer = nrf52_adc_size_buffer,
};

/**
 * Callback to initialize an adc_dev structure from the os device
 * initialization callback.  This sets up a nrf52_adc_device(), so
 * that subsequent lookups to this device allow us to manipulate it.
 */
int
nrf52_adc_dev_init(struct os_dev *odev, void *arg)
{
    struct adc_dev *dev;
    dev = (struct adc_dev *) odev;

    os_mutex_init(&dev->ad_lock);

    dev->ad_chans = (void *) nrf52_adc_chans;
    dev->ad_chan_count = SAADC_CH_NUM;

    init_cfg = (struct nrf52_adc_dev_cfg *) arg;

    OS_DEV_SETHANDLERS(odev, nrf52_adc_open, nrf52_adc_close);
    dev->ad_funcs = &nrf52_adc_funcs;

#if MYNEWT_VAL(OS_SYSVIEW)
    NVIC_SetVector(SAADC_IRQn, (uint32_t) sysview_irq_handler);
#else
    NVIC_SetVector(SAADC_IRQn, (uint32_t) nrf52_saadc_irq_handler);
#endif

    return 0;
}
