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


#include <hal/hal_bsp.h>
#include <adc/adc.h>
#include <assert.h>
#include <os/os.h>
#include <bsp/cmsis_nvic.h>

struct stm32f4_adc_stats {
    uint16_t adc_events;
    uint16_t adc_events_failed;
};
static struct stm32f4_adc_stats stm32f4_adc_stats;


/**
 * Open the STM32F4 ADC device
 *
 * This function locks the device for access from other tasks.
 *
 * @param odev The OS device to open
 * @param wait The time in MS to wait.  If 0 specified, returns immediately
 *             if resource unavailable.  If OS_WAIT_FOREVER specified, blocks
 *             until resource is available.
 * @param arg  Argument provided by higher layer to open.
 *
 * @return 0 on success, non-zero on failure.
 */
static int
stm32f4_adc_open(struct os_dev *odev, uint32_t wait, void *arg)
{
    struct adc_dev *dev;
    int rc;

    dev = (struct adc_dev *) odev;

    if (os_started()) {
        rc = os_mutex_pend(&dev->ad_lock, wait);
        if (rc != OS_OK) {
            goto err;
        }
    }

    if (odev->od_status & OS_DEV_STATUS_OPEN) {
        os_mutex_release(&dev->ad_lock);
        rc = OS_EBUSY;
        goto err;
    }

    return (0);
err:
    return (rc);
}


/**
 * Close the STM32F4 ADC device.
 *
 * This function unlocks the device.
 *
 * @param odev The device to close.
 */
static int
stm32f4_adc_close(struct os_dev *odev)
{
    struct adc_dev *dev;

    dev = (struct adc_dev *) odev;

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
 *                a nrf_saadc_channel_config_t
 *
 * @return 0 on success, non-zero on failure.
 */
static int
stm32f4_adc_configure_channel(struct adc_dev *dev, uint8_t cnum,
        void *cfgdata)
{
    uint16_t refmv;
    uint8_t res;
    int rc;

    /* XXX: Dummy values prior to implementation. */
    res = 16;
    refmv = 2800;

    /* Store these values in channel definitions, for conversions to
     * milivolts.
     */
    dev->ad_chans[cnum].c_res = res;
    dev->ad_chans[cnum].c_refmv = refmv;
    dev->ad_chans[cnum].c_configured = 1;

    return (0);
err:
    return (rc);
}

/**
 * Set buffer to read data into.  Implementation of setbuffer handler.
 * Sets both the primary and secondary buffers for DMA.
 */
static int
stm32f4_adc_set_buffer(struct adc_dev *dev, void *buf1, void *buf2,
        int buf_len)
{
    int rc;

    rc = OS_EINVAL;
    goto err;

    return (0);
err:
    return (rc);
}

static int
stm32f4_adc_release_buffer(struct adc_dev *dev, void *buf, int buf_len)
{
    int rc;

    rc = OS_EINVAL;
    goto err;

    return (0);
err:
    return (rc);
}

/**
 * Trigger an ADC sample.
 */
static int
stm32f4_adc_sample(struct adc_dev *dev)
{
    return (0);
}

/**
 * Blocking read of an ADC channel, returns result as an integer.
 */
static int
stm32f4_adc_read_channel(struct adc_dev *dev, uint8_t cnum, int *result)
{
    int rc;

    rc = OS_EINVAL;
    goto err;

    return (0);
err:
    return (rc);
}

static int
stm32f4_adc_read_buffer(struct adc_dev *dev, void *buf, int buf_len, int off,
        int *result)
{
    return (0);
}

static int
stm32f4_adc_size_buffer(struct adc_dev *dev, int chans, int samples)
{
    return (0 * chans * samples);
}


/**
 * Callback to initialize an adc_dev structure from the os device
 * initialization callback.  This sets up a nrf52_adc_device(), so
 * that subsequent lookups to this device allow us to manipulate it.
 */
int
stm32f4_adc_dev_init(struct os_dev *odev, void *arg)
{
    struct stm32f4_adc_dev *sad;
    struct stm32f4_adc_dev_cfg *sac;
    struct adc_dev *dev;
    struct adc_driver_funcs *af;

    sad = (struct stm32f4_adc_dev *) odev;
    sac = (struct stm32f4_adc_dev_cfg *) arg;

    assert(sad != NULL);
    assert(sac != NULL);

    dev = (struct adc_dev *) &sad->sad_dev;

    os_mutex_init(&dev->ad_lock);

    /* Have a pointer to the init typedef from the configured
     * value.  This allows non-driver specific items to be configured.
     */
    sad->sad_init = &sac->sac_init;

    dev->ad_chans = (void *) sac->sac_chans;
    dev->ad_chan_count = sac->sac_chan_count;

    OS_DEV_SETHANDLERS(odev, stm32f4_adc_open, stm32f4_adc_close);

    af = &dev->ad_funcs;

    af->af_configure_channel = stm32f4_adc_configure_channel;
    af->af_sample = stm32f4_adc_sample;
    af->af_read_channel = stm32f4_adc_read_channel;
    af->af_set_buffer = stm32f4_adc_set_buffer;
    af->af_release_buffer = stm32f4_adc_release_buffer;
    af->af_read_buffer = stm32f4_adc_read_buffer;
    af->af_size_buffer = stm32f4_adc_size_buffer;

    return (0);
}


