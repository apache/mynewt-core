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
#include <nrfx.h>
#include <nrfx_adc.h>

#include "adc_nrf51/adc_nrf51.h"

/**
 * Weak symbol, this is defined in Nordic drivers but not exported.
 * Needed for NVIC_SetVector().
 */
extern void ADC_IRQHandler(void);

#define NRF_ADC_CHANNEL_COUNT   (1)

struct nrf51_adc_stats {
    uint16_t adc_events;
    uint16_t adc_events_failed;
};
static struct nrf51_adc_stats nrf51_adc_stats;

static struct adc_dev *global_adc_dev;

static nrfx_adc_config_t *global_adc_config;
static struct nrf51_adc_dev_cfg *init_adc_config;

static struct adc_chan_config nrf51_adc_chans[NRF_ADC_CHANNEL_COUNT];
static nrfx_adc_channel_t *nrf_adc_chan;

static void
nrf51_adc_event_handler(const nrfx_adc_evt_t *event)
{
    nrfx_adc_done_evt_t *done_ev;
    int rc;

    if (global_adc_dev == NULL) {
        ++nrf51_adc_stats.adc_events_failed;
        return;
    }

    ++nrf51_adc_stats.adc_events;

    /* Right now only data reads supported, assert for unknown event
     * type.
     */
    assert(event->type == NRFX_ADC_EVT_DONE);

    done_ev = (nrfx_adc_done_evt_t * const) &event->data.done;

    rc = global_adc_dev->ad_event_handler_func(global_adc_dev,
            global_adc_dev->ad_event_handler_arg,
            ADC_EVENT_RESULT, done_ev->p_buffer,
            done_ev->size * sizeof(nrf_adc_value_t));
    if (rc != 0) {
        ++nrf51_adc_stats.adc_events_failed;
    }
}

/**
 * Open the NRF51 ADC device
 *
 * This function locks the device for access from other tasks.
 *
 * @param odev The OS device to open
 * @param wait The time in MS to wait.  If 0 specified, returns immediately
 *             if resource unavailable.  If OS_WAIT_FOREVER specified, blocks
 *             until resource is available.
 * @param arg  Argument provided by higher layer to open, in this case
 *             it must be a nrfx_adc_config_t
 *             configuration.
 *
 * @return 0 on success, non-zero on failure.
 */
static int
nrf51_adc_open(struct os_dev *odev, uint32_t wait, void *arg)
{
    struct adc_dev *dev;
    nrfx_adc_config_t *cfg;
    int rc;

    if (arg == NULL) {
        rc = OS_EINVAL;
        goto err;
    }

    dev = (struct adc_dev *) odev;

    if (os_started()) {
        rc = os_mutex_pend(&dev->ad_lock, wait);
        if (rc != OS_OK) {
            goto err;
        }
    }

    /* Initialize the device */
    cfg = (nrfx_adc_config_t *)arg;
    rc = nrfx_adc_init(cfg, nrf51_adc_event_handler);
    if (rc != NRFX_SUCCESS) {
        goto err;
    }

    global_adc_dev = dev;
    global_adc_config = arg;

    return (0);
err:
    return (rc);
}


/**
 * Close the NRF51 ADC device.
 *
 * This function unlocks the device.
 *
 * @param odev The device to close.
 */
static int
nrf51_adc_close(struct os_dev *odev)
{
    struct adc_dev *dev;

    dev = (struct adc_dev *) odev;

    nrfx_adc_uninit();

    global_adc_dev = NULL;
    global_adc_config = NULL;

    if (os_started()) {
        os_mutex_release(&dev->ad_lock);
    }

    return (0);
}

/**
 * Configure an ADC channel on the Nordic ADC.
 *
 * @param dev The ADC device to configure
 * @param cnum The channel on the ADC device to configure
 * @param cfgdata An opaque pointer to channel config, expected to be
 *                a nrfx_adc_channel_config_t
 *
 * @return 0 on success, non-zero on failure.
 */
static int
nrf51_adc_configure_channel(struct adc_dev *dev, uint8_t cnum,
                            void *cfgdata)
{
    int rc;
    nrfx_adc_channel_config_t *cc_cfg;
    int reference;
    uint16_t refmv;
    uint8_t res;

    if (global_adc_config == NULL) {
        rc = OS_ERROR;
        goto err;
    }

    //store nrf_adc_chan for nrf51_adc_read_channel
    nrf_adc_chan = cfgdata;

    nrfx_adc_channel_enable(nrf_adc_chan);

    // they play shenanigans storing the reference bit fields so we have to as well
    cc_cfg = &nrf_adc_chan->config.config;
    reference = cc_cfg->reference | (cc_cfg->external_reference << ADC_CONFIG_EXTREFSEL_Pos);

    /* Set the resolution and reference voltage for this channel to
    * enable conversion functions.
    */
    switch (cc_cfg->resolution) {
        case NRF_ADC_CONFIG_RES_8BIT:
            res = 8;
            break;
        case NRF_ADC_CONFIG_RES_9BIT:
            res = 9;
            break;
        case NRF_ADC_CONFIG_RES_10BIT:
            res = 10;
            break;
        default:
            assert(0);
    }

    switch (reference) {
        case NRF_ADC_CONFIG_REF_VBG:
            refmv = 1200; /* 1.2V for NRF51 */
            break;
        case NRF_ADC_CONFIG_REF_EXT_REF0:
            refmv = init_adc_config->nadc_refmv0;
            break;
        case NRF_ADC_CONFIG_REF_EXT_REF1:
            refmv = init_adc_config->nadc_refmv1;
            break;
        case NRF_ADC_CONFIG_REF_SUPPLY_ONE_HALF:
            refmv = init_adc_config->nadc_refmv_vdd / 2;
            break;
        case NRF_ADC_CONFIG_REF_SUPPLY_ONE_THIRD:
            refmv = init_adc_config->nadc_refmv_vdd / 3;
            break;
        default:
            assert(0);
    }

    /* Adjust reference voltage for gain. */
    switch (cc_cfg->input) {
        case NRF_ADC_CONFIG_SCALING_INPUT_FULL_SCALE:
            break;
        case NRF_ADC_CONFIG_SCALING_INPUT_ONE_THIRD:
            refmv *= 3;
            break;
        case NRF_ADC_CONFIG_SCALING_INPUT_TWO_THIRDS:
            refmv = (refmv * 3) / 2;
            break;
        case NRF_ADC_CONFIG_SCALING_SUPPLY_ONE_THIRD:
            refmv = refmv * 3;
            break;
        case NRF_ADC_CONFIG_SCALING_SUPPLY_TWO_THIRDS:
            refmv = (refmv * 3) / 2;
            break;
        default:
            break;
    }

    /* Store these values in channel definitions, for conversions to
     * milivolts.
     */
    dev->ad_chans[cnum].c_res = res;
    dev->ad_chans[cnum].c_refmv = refmv;
    dev->ad_chans[cnum].c_configured = 1;
    dev->ad_chans[cnum].c_cnum = cnum;

    return (0);
err:
    return (rc);
}

/**
 * Set buffer to read data into.  Implementation of setbuffer handler.
 * Sets both the primary and secondary buffers for DMA.
 */
static int
nrf51_adc_set_buffer(struct adc_dev *dev, void *buf1, void *buf2,
                     int buf_len)
{
    int rc;

    if (dev == NULL || buf1 == NULL) {
        rc = OS_EINVAL;
        goto err;
    }

    /* Convert overall buffer length, into a total number of samples which
     * Nordic APIs expect.
     */
    buf_len /= sizeof(nrf_adc_value_t);

    rc = nrfx_adc_buffer_convert((nrf_adc_value_t *) buf1, buf_len);
    if (rc != NRFX_SUCCESS) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

//WHY DOES THIS EXIST! why doesnt it work without it!
static int
nrf51_adc_release_buffer(struct adc_dev *dev, void *buf, int buf_len)
{
    int rc;

    buf_len /= sizeof(nrf_adc_value_t);

    rc = nrfx_adc_buffer_convert((nrf_adc_value_t *) buf, buf_len);
    if (rc != NRFX_SUCCESS) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

/**
 * Trigger an ADC sample.
 */
static int
nrf51_adc_sample(struct adc_dev *dev)
{
    nrfx_adc_sample();

    return (0);
}

/**
 * Blocking read of an ADC channel, returns result as an integer.
 * It is technically not necessary to have actually adc_chan_config (or even nrf51_adc_dev_init)
 * However there is no way to pass the nrfx_adc_channel_t argument through this function
 */
static int
nrf51_adc_read_channel(struct adc_dev *dev, uint8_t cnum, int *result)
{
    nrf_adc_value_t adc_value;
    int rc;

    rc = nrfx_adc_sample_convert(nrf_adc_chan, &adc_value);
    if (rc != NRFX_SUCCESS) {
        rc = OS_EBUSY;
        goto err;
    }

    *result = (int) adc_value;

    return 0;
err:
    return rc;
}

static int
nrf51_adc_read_buffer(struct adc_dev *dev, void *buf, int buf_len, int off,
                      int *result)
{
    nrf_adc_value_t val;
    int data_off;

    data_off = off * sizeof(nrf_adc_value_t);
    assert(data_off < buf_len);

    val = *(nrf_adc_value_t *) ((uint8_t *) buf + data_off);
    *result = val;

    return (0);
}

static int
nrf51_adc_size_buffer(struct adc_dev *dev, int chans, int samples)
{
    return (sizeof(nrf_adc_value_t) * chans * samples);
}


/**
 * Callback to initialize an adc_dev structure from the os device
 * initialization callback.  This sets up a nrf51_adc_device(), so that
 * subsequent lookups to this device allow us to manipulate it.
 */
int
nrf51_adc_dev_init(struct os_dev *odev, void *arg)
{
    struct adc_dev *dev;
    struct adc_driver_funcs *af;

    dev = (struct adc_dev *) odev;

    os_mutex_init(&dev->ad_lock);

    dev->ad_chans = (void *) nrf51_adc_chans;
    dev->ad_chan_count = NRF_ADC_CHANNEL_COUNT;

    OS_DEV_SETHANDLERS(odev, nrf51_adc_open, nrf51_adc_close);

    assert(init_adc_config == NULL || init_adc_config == arg);
    init_adc_config = arg;

    af = &dev->ad_funcs;

    af->af_configure_channel = nrf51_adc_configure_channel;
    af->af_sample = nrf51_adc_sample;
    af->af_read_channel = nrf51_adc_read_channel;
    af->af_set_buffer = nrf51_adc_set_buffer;
    af->af_release_buffer = nrf51_adc_release_buffer;
    af->af_read_buffer = nrf51_adc_read_buffer;
    af->af_size_buffer = nrf51_adc_size_buffer;

    NVIC_SetVector(ADC_IRQn, (uint32_t) nrfx_adc_irq_handler);

    return (0);
}


