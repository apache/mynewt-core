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
#include <assert.h>
#include <os/os.h>
#include <errno.h>
#include <pwm/pwm.h>
#include <string.h>
#include <bsp/cmsis_nvic.h>

/* Nordic headers */
#include <nrf.h>
#include <nrf_pwm.h>
#include <nrf_drv_pwm.h>
#include <nrf_drv_clock.h>

#include <app_util_platform.h>
#include <nrf_drv_ppi.h>
#include <nrf_drv_timer.h>

/* Mynewt Nordic driver */
#include "pwm_nrf52/pwm_nrf52.h"

/**
 * Weak symbols, these are defined in Nordic drivers but not exported.
 * Needed for NVIC_SetVector().
 */
#if (PWM0_ENABLED == 1)
extern void PWM0_IRQHandler(void);
#endif
#if (PWM1_ENABLED == 1)
extern void PWM1_IRQHandler(void);
#endif
#if (PWM2_ENABLED == 1)
extern void PWM2_IRQHandler(void);
#endif

typedef void (*user_handler_t) ();

struct nrf52_pwm_dev_global {
    bool in_use;
    bool playing;
    nrf_drv_pwm_t drv_instance;
    nrf_drv_pwm_config_t config;
    nrf_pwm_sequence_t seq;
    nrf_drv_pwm_handler_t internal_handler;
    user_handler_t user_handler;
};

static struct nrf52_pwm_dev_global instances[] =
{
#if (PWM0_ENABLED == 1)
    [0].in_use = false,
    [0].playing = false,
    [0].drv_instance = NRF_DRV_PWM_INSTANCE(0),
    [0].config = NRF_DRV_PWM_DEFAULT_CONFIG(0),
    [0].internal_handler = NULL,
    [0].user_handler = NULL
#endif
#if (PWM1_ENABLED == 1)
    ,
    [1].in_use = false,
    [1].playing = false,
    [1].drv_instance = NRF_DRV_PWM_INSTANCE(1),
    [1].config = NRF_DRV_PWM_DEFAULT_CONFIG(1),
    [1].internal_handler = NULL,
    [1].user_handler = NULL
#endif
#if (PWM2_ENABLED == 1)
    ,
    [2].in_use = false,
    [2].playing = false,
    [2].drv_instance = NRF_DRV_PWM_INSTANCE(2),
    [2].config = NRF_DRV_PWM_DEFAULT_CONFIG(2),
    [2].internal_handler = NULL,
    [2].user_handler = NULL
#endif
};

#if (PWM0_ENABLED == 1)
nrf_pwm_values_individual_t values_pwm0;
#endif
#if (PWM1_ENABLED == 1)
nrf_pwm_values_individual_t values_pwm1;
#endif
#if (PWM2_ENABLED == 1)
nrf_pwm_values_individual_t values_pwm2;
#endif

static nrf_pwm_sequence_t const sequences[] =
{
#if (PWM0_ENABLED == 1)
    [0].values.p_individual = &values_pwm0,
    [0].length = 4,
    [0].repeats = 0,
    [0].end_delay = 0
#endif
#if (PWM1_ENABLED == 1)
    ,
    [1].values.p_individual = &values_pwm1,
    [1].length = 4,
    [1].repeats = 0,
    [1].end_delay = 0
#endif
#if (PWM2_ENABLED == 1)
    ,
    [2].values.p_individual = &values_pwm2,
    [2].length = 4,
    [2].repeats = 0,
    [2].end_delay = 0
#endif
};

#if (PWM0_ENABLED == 1)
static void handler(nrf_drv_pwm_evt_type_t event_type)
{
    if (event_type == NRF_DRV_PWM_EVT_FINISHED)
    {
        instances[0].user_handler();
    }
}
#endif

/**
 * Initialize a driver instance.
 */
static int
init_instance(int inst_id, nrf_drv_pwm_config_t* init_conf)
{
    if(!instances[inst_id].in_use) {
        return (EINVAL);
    }
    nrf_drv_pwm_config_t *config;

    config = &instances[inst_id].config;
    if (!init_conf) {
        config->output_pins[0] = NRF_DRV_PWM_PIN_NOT_USED;
        config->output_pins[1] = NRF_DRV_PWM_PIN_NOT_USED;
        config->output_pins[2] = NRF_DRV_PWM_PIN_NOT_USED;
        config->output_pins[3] = NRF_DRV_PWM_PIN_NOT_USED;
        config->irq_priority = APP_IRQ_PRIORITY_LOW;
        config->base_clock   = NRF_PWM_CLK_1MHz;
        config->count_mode   = NRF_PWM_MODE_UP;
        config->top_value    = 10000;
        config->load_mode    = NRF_PWM_LOAD_INDIVIDUAL;
        config->step_mode    = NRF_PWM_STEP_AUTO;
    } else {
        memcpy(config, init_conf, sizeof(nrf_drv_pwm_config_t));
    }

    return (0);
}

/**
 * Cleanup a driver instance.
 */
static void
cleanup_instance(int inst_id)
{
    if (instances[inst_id].playing) {
        memset((uint16_t *) sequences[inst_id].values.p_individual,
               0x00,
               4 * sizeof(uint16_t));
        instances[inst_id].playing = false;
    }
    instances[inst_id].in_use = false;
}

/**
 * Open the NRF52 PWM device
 *
 * This function locks the device for access from other tasks.
 *
 * @param odev The OS device to open
 * @param wait The time in MS to wait.  If 0 specified, returns immediately
 *             if resource unavailable.  If OS_WAIT_FOREVER specified, blocks
 *             until resource is available.
 * @param arg  Argument provided by higher layer to open, in this case
 *             it can be a nrf_drv_pwm_config_t, to override the default
 *             configuration.
 *
 * @return 0 on success, non-zero on failure.
 */
static int
nrf52_pwm_open(struct os_dev *odev, uint32_t wait, void *arg)
{
    struct pwm_dev *dev;
    int stat = 0;
    int inst_id;
    dev = (struct pwm_dev *) odev;
    inst_id = dev->pwm_instance_id;

    if (os_started()) {
        stat = os_mutex_pend(&dev->pwm_lock, wait);
        if (stat != OS_OK) {
            return (stat);
        }
    }

    if (odev->od_flags & OS_DEV_F_STATUS_OPEN) {
        os_mutex_release(&dev->pwm_lock);
        stat = OS_EBUSY;
        return (stat);
    }

    stat = init_instance(inst_id, arg);
    if (stat) {
        return (stat);
    }

    return (0);
}

/**
 * Close the NRF52 PWM device.
 *
 * This function unlocks the device.
 *
 * @param odev The device to close.
 */
static int
nrf52_pwm_close(struct os_dev *odev)
{
    struct pwm_dev *dev;
    int inst_id;

    dev = (struct pwm_dev *) odev;
    inst_id = dev->pwm_instance_id;

    nrf_drv_pwm_uninit(&instances[inst_id].drv_instance);

    cleanup_instance(inst_id);

    if (os_started()) {
        os_mutex_release(&dev->pwm_lock);
    }

    return (0);
}

/**
 * Play using current configuration.
 */
static void
play_current_config(int inst_id)
{
    instances[inst_id].playing = true;
    nrf_drv_pwm_simple_playback(&instances[inst_id].drv_instance,
                                &sequences[inst_id],
                                1,
                                NRF_DRV_PWM_FLAG_LOOP);
}

/**
 * Configure a channel on the PWM device.
 *
 * @param dev The device to configure.
 * @param cnum The channel number to configure.
 * @param cfg  A pwm_chan_cfg data structure.
 *
 * @return 0 on success, non-zero error code on failure.
 */
static int
nrf52_pwm_configure_channel(struct pwm_dev *dev,
                            uint8_t cnum,
                            struct pwm_chan_cfg *cfg)
{
    int inst_id = dev->pwm_instance_id;
    nrf_drv_pwm_config_t *config = &instances[inst_id].config;

    config->output_pins[cnum] = cfg->pin;
    config->output_pins[cnum] |= (cfg->inverted) ?
        NRF_DRV_PWM_PIN_INVERTED : config->output_pins[cnum];

    instances[inst_id].user_handler = (user_handler_t) cfg->data;
    instances[inst_id].internal_handler =
        (cfg->data) ? handler : NULL;

    if (instances[inst_id].playing) {
        nrf_drv_pwm_uninit(&instances[inst_id].drv_instance);
        nrf_drv_pwm_init(&instances[inst_id].drv_instance,
                         &instances[inst_id].config,
                         instances[inst_id].internal_handler);

        play_current_config(inst_id);
    }

    return (0);
}

/**
 * Enable the PWM with specified duty cycle.
 *
 * This duty cycle is a fractional duty cycle where 0 == off, 65535=on,
 * and any value in between is on for fraction clocks and off
 * for 65535-fraction clocks.
 *
 * @param dev The device to configure.
 * @param cnum The channel number. The channel should be in use.
 * @param fraction The fraction value.
 *
 * @return 0 on success, negative on error.
 */
static int
nrf52_pwm_enable_duty_cycle(struct pwm_dev *dev, uint8_t cnum, uint16_t fraction)
{
    int stat;
    int inst_id = dev->pwm_instance_id;
    nrf_drv_pwm_config_t *config;
    bool inverted;

    config = &instances[inst_id].config;
    assert (config->output_pins[cnum] != NRF_DRV_PWM_PIN_NOT_USED);
    inverted = ((config->output_pins[cnum] & NRF_DRV_PWM_PIN_INVERTED) != 0);

    ((uint16_t *) sequences[inst_id].values.p_individual)[cnum] =
            (inverted) ? fraction : config->top_value - fraction;

    if (!instances[inst_id].playing) {
        stat = nrf_drv_pwm_init(&instances[inst_id].drv_instance,
                                &instances[inst_id].config,
                                instances[inst_id].internal_handler);
        if (stat != NRF_SUCCESS) {
            return (stat);
        }
        play_current_config(inst_id);
    }
    return (0);
}

/**
 * Disable the PWM channel, it will be marked as unconfigured.
 *
 * @param dev The device to configure.
 * @param cnum The channel number.
 *
 * @return 0 success, negative on error.
 */
static int
nrf52_pwm_disable(struct pwm_dev *dev, uint8_t cnum)
{
    int inst_id = dev->pwm_instance_id;
    instances[inst_id].config.output_pins[cnum] = NRF_DRV_PWM_PIN_NOT_USED;

    nrf_drv_pwm_uninit(&instances[inst_id].drv_instance);
    nrf_drv_pwm_init(&instances[inst_id].drv_instance,
                     &instances[inst_id].config,
                     instances[inst_id].internal_handler);


    /* check if there is any channel in use */
    if (instances[inst_id].playing) {
        play_current_config(inst_id);
    }
    return (0);
}

/**
 * This frequency must be between 1/2 the clock frequency and
 * the clock divided by the resolution. NOTE: This may affect
 * other PWM channels.
 *
 * @param dev The device to configure.
 * @param freq_hz The frequency value in Hz.
 *
 * @return A value is in Hz on success, negative on error.
 */
static int
nrf52_pwm_set_frequency(struct pwm_dev *dev, uint32_t freq_hz)
{
    int inst_id = dev->pwm_instance_id;
    nrf_pwm_clk_t *frq = &instances[inst_id].config.base_clock;
    uint16_t *top_value = &instances[inst_id].config.top_value;
    uint32_t base_freq_val;

    freq_hz = (freq_hz > 7999999) ? 7999999 : freq_hz;
    freq_hz = (freq_hz < 2) ? 2 : freq_hz;

    if (freq_hz > 244) {
        *frq = NRF_PWM_CLK_16MHz;
        base_freq_val = 16000000;
    } else if (freq_hz > 122) {
        *frq = NRF_PWM_CLK_8MHz;
        base_freq_val = 8000000;
    } else if (freq_hz > 61) {
        *frq = NRF_PWM_CLK_4MHz;
        base_freq_val = 4000000;
    } else if (freq_hz > 30) {
        *frq = NRF_PWM_CLK_2MHz;
        base_freq_val = 2000000;
    } else if (freq_hz > 15) {
        *frq = NRF_PWM_CLK_1MHz;
        base_freq_val = 1000000;
    } else if (freq_hz > 7) {
        *frq = NRF_PWM_CLK_500kHz;
        base_freq_val = 500000;
    } else if (freq_hz > 3) {
        *frq = NRF_PWM_CLK_250kHz;
        base_freq_val = 250000;
    } else if (freq_hz > 1) {
        *frq = NRF_PWM_CLK_125kHz;
        base_freq_val = 125000;
    }

    *top_value = base_freq_val / freq_hz;

    if (instances[inst_id].playing) {
        nrf_drv_pwm_uninit(&instances[inst_id].drv_instance);
        nrf_drv_pwm_init(&instances[inst_id].drv_instance,
                         &instances[inst_id].config,
                         instances[inst_id].internal_handler);

        play_current_config(inst_id);
    }

    return base_freq_val;
}

/**
 * Get the underlying clock driving the PWM device.
 *
 * @param dev
 *
 * @return value is in Hz on success, negative on error.
 */
static int
nrf52_pwm_get_clock_freq(struct pwm_dev *dev)
{
    int inst_id = dev->pwm_instance_id;
    assert(instances[inst_id].in_use);

    switch (instances[inst_id].config.base_clock) {
    case NRF_PWM_CLK_16MHz:
        return (16000000);
    case NRF_PWM_CLK_8MHz:
        return (8000000);
    case NRF_PWM_CLK_4MHz:
        return (4000000);
    case NRF_PWM_CLK_2MHz:
        return (2000000);
    case NRF_PWM_CLK_1MHz:
        return (1000000);
    case NRF_PWM_CLK_500kHz:
        return (500000);
    case NRF_PWM_CLK_250kHz:
        return (250000);
    case NRF_PWM_CLK_125kHz:
        return (125000);
    }
    return (-EINVAL);
}

/**
 * Get the resolution of the PWM in bits.
 *
 * @param dev The device to query.
 *
 * @return The value in bits on success, negative on error.
 */
static int
nrf52_pwm_get_resolution_bits(struct pwm_dev *dev)
{
    int inst_id = dev->pwm_instance_id;
    assert(instances[inst_id].in_use);

    uint16_t top_val = instances[inst_id].config.top_value;
    if (top_val >= 32768)
    {
        return (15);
    } else if (top_val >= 16384) {
        return (14);
    } else if (top_val >= 8192) {
        return (13);
    } else if (top_val >= 4096) {
        return (12);
    } else if (top_val >= 2048) {
        return (11);
    } else if (top_val >= 1024) {
        return (10);
    } else if (top_val >= 512) {
        return (9);
    } else if (top_val >= 256) {
        return (8);
    } else if (top_val >= 128) {
        return (7);
    } else if (top_val >= 64) {
        return (6);
    } else if (top_val >= 32) {
        return (5);
    } else if (top_val >= 16) {
        return (4);
    } else if (top_val >= 8) {
        return (3);
    } else if (top_val >= 4) {
        return (2);
    } else if (top_val >= 2) {
        return (1);
    }

    return (-EINVAL);
}

/**
 * Callback to initialize an adc_dev structure from the os device
 * initialization callback.  This sets up a nrf52_pwm_device(), so
 * that subsequent lookups to this device allow us to manipulate it.
 */
int
nrf52_pwm_dev_init(struct os_dev *odev, void *arg)
{
    struct pwm_dev *dev;
    struct pwm_driver_funcs *pwm_funcs;

    dev = (struct pwm_dev *) odev;

    dev->pwm_instance_id = 0;
    while (dev->pwm_instance_id < PWM_COUNT) {
        if (!instances[dev->pwm_instance_id].in_use) {
            instances[dev->pwm_instance_id].in_use = true;
            break;
        }
        dev->pwm_instance_id++;
    }

    dev->pwm_chan_count = NRF_PWM_CHANNEL_COUNT;
    os_mutex_init(&dev->pwm_lock);

    OS_DEV_SETHANDLERS(odev, nrf52_pwm_open, nrf52_pwm_close);

    pwm_funcs = &dev->pwm_funcs;
    pwm_funcs->pwm_configure_channel = nrf52_pwm_configure_channel;
    pwm_funcs->pwm_enable_duty_cycle = nrf52_pwm_enable_duty_cycle;
    pwm_funcs->pwm_set_frequency = nrf52_pwm_set_frequency;
    pwm_funcs->pwm_get_clock_freq = nrf52_pwm_get_clock_freq;
    pwm_funcs->pwm_get_resolution_bits = nrf52_pwm_get_resolution_bits;
    pwm_funcs->pwm_disable = nrf52_pwm_disable;

    switch (dev->pwm_instance_id) {
#if (PWM0_ENABLED == 1)
    case 0:
        NVIC_SetVector(PWM0_IRQn, (uint32_t) PWM0_IRQHandler);
        break;
#endif

#if (PWM1_ENABLED == 1)
    case 1:
        NVIC_SetVector(PWM1_IRQn, (uint32_t) PWM1_IRQHandler);
        break;
#endif

#if (PWM2_ENABLED == 1)
    case 2:
        NVIC_SetVector(PWM2_IRQn, (uint32_t) PWM2_IRQHandler);
        break;
#endif
    }
    return (0);
}
