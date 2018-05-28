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
#include <string.h>
#include <errno.h>
#include "os/mynewt.h"
#include <bsp.h>
#include <hal/hal_bsp.h>
#include <pwm/pwm.h>
#include <mcu/cmsis_nvic.h>

/* Nordic headers */
#include <nrfx.h>
#include <nrfx_pwm.h>
#include <nrf_pwm.h>

/* Mynewt Nordic driver */
#include "pwm_nrf52/pwm_nrf52.h"

struct nrf52_pwm_dev_global {
    bool in_use;
    bool playing;
    nrfx_pwm_t drv_instance;
    nrfx_pwm_config_t config;
    nrf_pwm_values_individual_t duty_cycles;
    uint32_t n_cycles;
    nrfx_pwm_flag_t flags;
    nrfx_pwm_handler_t internal_handler;
    user_handler_t cycle_handler;
    user_handler_t seq_end_handler;
    void* cycle_data;
    void* seq_end_data;
};

static struct nrf52_pwm_dev_global instances[] =
{
#if MYNEWT_VAL(PWM_0)
    [0].in_use = false,
    [0].playing = false,
    [0].drv_instance = NRFX_PWM_INSTANCE(0),
    [0].config = NRFX_PWM_DEFAULT_CONFIG,
    [0].flags = NRFX_PWM_FLAG_LOOP,
    [0].duty_cycles = {0},
    [0].n_cycles = 1,
    [0].internal_handler = NULL,
    [0].cycle_handler = NULL,
    [0].cycle_data = NULL,
    [0].seq_end_handler = NULL,
    [0].seq_end_data = NULL
#endif
#if MYNEWT_VAL(PWM_1)
    ,
    [1].in_use = false,
    [1].playing = false,
    [1].drv_instance = NRFX_PWM_INSTANCE(1),
    [1].config = NRFX_PWM_DEFAULT_CONFIG,
    [1].flags = NRFX_PWM_FLAG_LOOP,
    [1].duty_cycles = {0},
    [1].n_cycles = 1,
    [1].internal_handler = NULL,
    [1].cycle_handler = NULL,
    [1].cycle_data = NULL,
    [1].seq_end_handler = NULL,
    [1].seq_end_data = NULL
#endif
#if MYNEWT_VAL(PWM_2)
    ,
    [2].in_use = false,
    [2].playing = false,
    [2].drv_instance = NRFX_PWM_INSTANCE(2),
    [2].config = NRFX_PWM_DEFAULT_CONFIG,
    [2].flags = NRFX_PWM_FLAG_LOOP,
    [2].duty_cycles = {0},
    [2].n_cycles = 1,
    [2].internal_handler = NULL,
    [2].cycle_handler = NULL,
    [2].cycle_data = NULL,
    [2].seq_end_handler = NULL,
    [2].seq_end_data = NULL
#endif
#if MYNEWT_VAL(PWM_3)
    ,
    [3].in_use = false,
    [3].playing = false,
    [3].drv_instance = NRFX_PWM_INSTANCE(3),
    [3].config = NRFX_PWM_DEFAULT_CONFIG,
    [3].flags = NRFX_PWM_FLAG_LOOP,
    [3].duty_cycles = {0},
    [3].n_cycles = 1,
    [3].internal_handler = NULL,
    [3].cycle_handler = NULL,
    [3].cycle_data = NULL,
    [3].seq_end_handler = NULL,
    [3].seq_end_data = NULL
#endif
};

#if MYNEWT_VAL(PWM_0)
static void handler_0(nrfx_pwm_evt_type_t event_type)
{
    switch (event_type)
    {
    case NRFX_PWM_EVT_END_SEQ0 :
    case NRFX_PWM_EVT_END_SEQ1 :
        instances[0].cycle_handler(instances[0].cycle_data);
        break;

    case NRFX_PWM_EVT_FINISHED :
        instances[0].playing = false;
        nrfx_pwm_uninit(&instances[0].drv_instance);
        instances[0].seq_end_handler(instances[0].seq_end_data);
        break;

    default:
        assert(0);
    }
}
#endif

#if MYNEWT_VAL(PWM_1)
static void handler_1(nrfx_pwm_evt_type_t event_type)
{
    switch (event_type)
    {
    case NRFX_PWM_EVT_END_SEQ0 :
    case NRFX_PWM_EVT_END_SEQ1 :
        instances[1].cycle_handler(instances[1].cycle_data);
        break;

    case NRFX_PWM_EVT_FINISHED :
        instances[1].playing = false;
        nrfx_pwm_uninit(&instances[1].drv_instance);
        instances[1].seq_end_handler(instances[1].seq_end_data);
        break;

    default:
        assert(0);
    }
}
#endif

#if MYNEWT_VAL(PWM_2)
static void handler_2(nrfx_pwm_evt_type_t event_type)
{
    switch (event_type)
    {
    case NRFX_PWM_EVT_END_SEQ0 :
    case NRFX_PWM_EVT_END_SEQ1 :
        instances[2].cycle_handler(instances[2].cycle_data);
        break;

    case NRFX_PWM_EVT_FINISHED :
        instances[2].playing = false;
        nrfx_pwm_uninit(&instances[2].drv_instance);
        instances[2].seq_end_handler(instances[2].seq_end_data);
        break;

    default:
        assert(0);
    }

}
#endif

#if MYNEWT_VAL(PWM_3)
static void handler_3(nrfx_pwm_evt_type_t event_type)
{
    switch (event_type)
    {
    case NRFX_PWM_EVT_END_SEQ0 :
    case NRFX_PWM_EVT_END_SEQ1 :
        instances[3].cycle_handler(instances[3].cycle_data);
        break;

    case NRFX_PWM_EVT_FINISHED :
        instances[3].playing = false;
        nrfx_pwm_uninit(&instances[3].drv_instance);
        instances[3].seq_end_handler(instances[3].seq_end_data);
        break;

    default:
        assert(0);
    }
}
#endif

static nrfx_pwm_handler_t internal_handlers[] = {
#if MYNEWT_VAL(PWM_0)
handler_0
#endif
#if MYNEWT_VAL(PWM_1)
,
handler_1
#endif
#if MYNEWT_VAL(PWM_2)
,
handler_2
#endif
#if MYNEWT_VAL(PWM_3)
,
handler_3
#endif
};

/**
 * Initialize a driver instance.
 */
static int
init_instance(int inst_id, nrfx_pwm_config_t* init_conf)
{
    nrfx_pwm_config_t *config;

    config = &instances[inst_id].config;
    if (!init_conf) {
        config->output_pins[0] = NRFX_PWM_PIN_NOT_USED;
        config->output_pins[1] = NRFX_PWM_PIN_NOT_USED;
        config->output_pins[2] = NRFX_PWM_PIN_NOT_USED;
        config->output_pins[3] = NRFX_PWM_PIN_NOT_USED;
        config->irq_priority = 3; /* APP_IRQ_PRIORITY_LOW */
        config->base_clock   = NRF_PWM_CLK_1MHz;
        config->count_mode   = NRF_PWM_MODE_UP;
        config->top_value    = 10000;
        config->load_mode    = NRF_PWM_LOAD_INDIVIDUAL;
        config->step_mode    = NRF_PWM_STEP_AUTO;
    } else {
        memcpy(config, init_conf, sizeof(nrfx_pwm_config_t));
    }
    return (0);
}

/**
 * Cleanup a driver instance.
 */
static void
cleanup_instance(int inst_id)
{
    instances[inst_id].in_use = false;
    instances[inst_id].playing = false;
    instances[inst_id].internal_handler = NULL;
    instances[inst_id].cycle_handler = NULL;
    instances[inst_id].seq_end_handler = NULL;
    instances[inst_id].cycle_data = NULL;
    instances[inst_id].seq_end_data = NULL;
    memset((uint16_t *) &instances[inst_id].duty_cycles,
           0x00,
           4 * sizeof(uint16_t));
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

    if (instances[inst_id].in_use) {
        return (EINVAL);
    }
    instances[inst_id].in_use = true;

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
 *
 * @return 0 on success, non-zero on failure.
 */
static int
nrf52_pwm_close(struct os_dev *odev)
{
    struct pwm_dev *dev;
    int inst_id;

    dev = (struct pwm_dev *) odev;
    inst_id = dev->pwm_instance_id;

    if (!instances[inst_id].in_use) {
        return (EINVAL);
    }

    if (!instances[inst_id].playing) {
        nrfx_pwm_uninit(&instances[inst_id].drv_instance);
    }
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
play_current_config(struct nrf52_pwm_dev_global *instance)
{
    nrf_pwm_sequence_t const seq =
        {
            .values.p_individual = &instance->duty_cycles,
            .length              = 4,
            .repeats             = 0,
            .end_delay           = 0
        };

    nrfx_pwm_simple_playback(&instance->drv_instance,
                             &seq,
                             instance->n_cycles,
                             instance->flags);
}

/**
 * Configure a PWM device.
 *
 * @param dev The device to configure.
 * @param cfg Configuration data for this device. If NULL the device will be
 * given default configuration values.
 *
 * @return 0 on success, non-zero error code on failure.
 */
int
nrf52_pwm_configure_device(struct pwm_dev *dev, struct pwm_dev_cfg *cfg)
{
    int inst_id = dev->pwm_instance_id;
    struct nrf52_pwm_dev_global *instance = &instances[inst_id];

    instance->n_cycles = (cfg->n_cycles) ? cfg->n_cycles : 1;

    /* Configure Interrupts  */
    if (cfg->cycle_handler || cfg->seq_end_handler) {
        instance->config.irq_priority = cfg->int_prio;
        instance->internal_handler = internal_handlers[inst_id];
        instance->cycle_handler = (user_handler_t) cfg->cycle_handler;
        instance->seq_end_handler = (user_handler_t) cfg->seq_end_handler;
        instance->cycle_data = cfg->cycle_data;
        instance->seq_end_data = cfg->seq_end_data;
    } else {
        instance->internal_handler = NULL;
    }

    instance->flags = (instance->n_cycles > 1) ?
        0 :
        NRFX_PWM_FLAG_LOOP;
    instance->flags |= (instance->cycle_handler) ?
        (NRFX_PWM_FLAG_SIGNAL_END_SEQ0 | NRFX_PWM_FLAG_SIGNAL_END_SEQ0) :
        0;
    instance->flags |= (instance->seq_end_handler) ?
        0 :
        NRFX_PWM_FLAG_NO_EVT_FINISHED;

    if (instance->playing) {
        nrfx_pwm_uninit(&instance->drv_instance);
        nrfx_pwm_init(&instance->drv_instance,
                      &instance->config,
                      instance->internal_handler);

        play_current_config(instance);
    }
    return (0);
}

/**
 * Configure a channel on the PWM device.
 *
 * @param dev The device to configure.
 * @param cnum The channel number to configure.
 * @param cfg  A pwm_chan_cfg data structure, where .
 *
 * @return 0 on success, non-zero error code on failure.
 */
static int
nrf52_pwm_configure_channel(struct pwm_dev *dev,
                            uint8_t cnum,
                            struct pwm_chan_cfg *cfg)
{
    int inst_id = dev->pwm_instance_id;
    struct nrf52_pwm_dev_global *instance = &instances[inst_id];
    nrfx_pwm_config_t *config = &instance->config;

    if (!instance->in_use) {
        return (EINVAL);
    }

    config->output_pins[cnum] = (uint8_t) cfg->pin;
    config->output_pins[cnum] |= (cfg->inverted) ?
        NRFX_PWM_PIN_INVERTED : config->output_pins[cnum];

    if (instance->playing) {
        nrfx_pwm_uninit(&instance->drv_instance);
        nrfx_pwm_init(&instance->drv_instance,
                      &instance->config,
                      instance->internal_handler);

        play_current_config(instance);
    }
    return (0);
}

/**
 * Set the specified duty cycle on a PWM Channel.
 *
 * This duty cycle is a fractional duty cycle where 0 == off,
 * base_freq / pwm_freq == 100% and any value in between is on for fraction clock
 * cycles and off for (base_freq / pwm_freq)-fraction clock cycles.
 *
 * @param dev The device which the channel belongs to.
 * @param cnum The channel number. This channel should be already configured.
 * @param fraction The fraction value.
 *
 * @return 0 on success, negative on error.
 */
static int
nrf52_pwm_set_duty_cycle(struct pwm_dev *dev, uint8_t cnum, uint16_t fraction)
{
    int inst_id = dev->pwm_instance_id;
    nrfx_pwm_config_t *config;
    bool inverted;

    if (!instances[inst_id].in_use) {
        return (-EINVAL);
    }

    config = &instances[inst_id].config;
    if (config->output_pins[cnum] == NRFX_PWM_PIN_NOT_USED) {
        return (-EINVAL);
    }

    inverted = ((config->output_pins[cnum] & NRFX_PWM_PIN_INVERTED) != 0);

    ((uint16_t *) &instances[inst_id].duty_cycles)[cnum] =
        (inverted) ? fraction : config->top_value - fraction;

    return (0);
}

/**
 * Enable a given PWM device.
 * This device should start playing on its previously configured channels.
 *
 * @param dev The PWM device to be enabled.
 *
 * @return 0 on success, negative on error.
 */
int
nrf52_pwm_enable(struct pwm_dev *dev)
{
    int inst_id = dev->pwm_instance_id;
    struct nrf52_pwm_dev_global *instance = &instances[inst_id];

    nrfx_pwm_init(&instance->drv_instance,
                  &instance->config,
                  instance->internal_handler);
    play_current_config(instance);
    instance->playing = true;

    return (0);
}

/**
 * Check whether a PWM channel is enabled on a given device.
 *
 * @param dev The device which the channel belongs to.
 * @param cnum The channel being queried.
 *
 * @return true if enabled, false if not.
 */
static bool
nrf52_pwm_is_enabled(struct pwm_dev *dev)
{
    return (instances[dev->pwm_instance_id].playing);
}

/**
 * Disable the PWM device, the device will stop playing while remaining configured.
 *
 * @param dev The device to disable.
 *
 * @return 0 on success, negative on error.
 */
static int
nrf52_pwm_disable(struct pwm_dev *dev)
{
    int inst_id = dev->pwm_instance_id;
    if (!instances[inst_id].in_use) {
        return (-EINVAL);
    }

    if (!nrfx_pwm_stop(&instances[inst_id].drv_instance, true)) {
        return (-EINVAL);
    }
    instances[inst_id].playing = false;

    nrfx_pwm_uninit(&instances[inst_id].drv_instance);
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
    if (!instances[inst_id].in_use) {
        return (-EINVAL);
    }

    nrf_pwm_clk_t *frq = &instances[inst_id].config.base_clock;
    uint16_t *top_value = &instances[inst_id].config.top_value;
    uint32_t base_freq_val;

    freq_hz = (freq_hz > 7999999) ? 7999999 : freq_hz;
    freq_hz = (freq_hz < 3) ? 3 : freq_hz;

    if (freq_hz > 488) {
        *frq = NRF_PWM_CLK_16MHz;
        base_freq_val = 16000000;
    } else if (freq_hz > 244) {
        *frq = NRF_PWM_CLK_8MHz;
        base_freq_val = 8000000;
    } else if (freq_hz > 122) {
        *frq = NRF_PWM_CLK_4MHz;
        base_freq_val = 4000000;
    } else if (freq_hz > 61) {
        *frq = NRF_PWM_CLK_2MHz;
        base_freq_val = 2000000;
    } else if (freq_hz > 30) {
        *frq = NRF_PWM_CLK_1MHz;
        base_freq_val = 1000000;
    } else if (freq_hz > 14) {
        *frq = NRF_PWM_CLK_500kHz;
        base_freq_val = 500000;
    } else if (freq_hz > 6) {
        *frq = NRF_PWM_CLK_250kHz;
        base_freq_val = 250000;
    } else if (freq_hz > 2) {
        *frq = NRF_PWM_CLK_125kHz;
        base_freq_val = 125000;
    }

    *top_value = base_freq_val / freq_hz;

    if (instances[inst_id].playing) {
        nrfx_pwm_uninit(&instances[inst_id].drv_instance);
        nrfx_pwm_init(&instances[inst_id].drv_instance,
                      &instances[inst_id].config,
                      instances[inst_id].internal_handler);

        play_current_config(&instances[inst_id]);
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
    if (!instances[inst_id].in_use) {
        return (-EINVAL);
    }

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
 * Get the top value for the cycle counter, i.e. the value which sets
 * the duty cycle to 100%.
 *
 * @param dev
 *
 * @return value in cycles on success, negative on error.
 */
int
nrf52_pwm_get_top_value(struct pwm_dev *dev)
{
    int inst_id = dev->pwm_instance_id;
    if (!instances[inst_id].in_use) {
        return (-EINVAL);
    }

    return (instances[inst_id].config.top_value);
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
    if (!instances[inst_id].in_use) {
        return (-EINVAL);
    }

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

#if MYNEWT_VAL(OS_SYSVIEW)
#if MYNEWT_VAL(PWM_0)
static void
pwm_0_irq_handler(void)
{
    os_trace_isr_enter();
    nrfx_pwm_0_irq_handler();
    os_trace_isr_exit();
}
#endif

#if MYNEWT_VAL(PWM_1)
static void
pwm_1_irq_handler(void)
{
    os_trace_isr_enter();
    nrfx_pwm_1_irq_handler();
    os_trace_isr_exit();
}
#endif

#if MYNEWT_VAL(PWM_2)
static void
pwm_2_irq_handler(void)
{
    os_trace_isr_enter();
    nrfx_pwm_2_irq_handler();
    os_trace_isr_exit();
}
#endif

#if MYNEWT_VAL(PWM_3)
static void
pwm_3_irq_handler(void)
{
    os_trace_isr_enter();
    nrfx_pwm_3_irq_handler();
    os_trace_isr_exit();
}
#endif

#define PWM_IRQ_HANDLER(_pwm_no) \
                            (uint32_t) pwm_ ## _pwm_no ## _irq_handler
#else
#define PWM_IRQ_HANDLER(_pwm_no) \
                            (uint32_t) nrfx_pwm_ ## _pwm_no ## _irq_handler
#endif

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
    IRQn_Type irqn;
    uint32_t irqh;

    assert(odev);
    dev = (struct pwm_dev *) odev;

    assert(arg);
    dev->pwm_instance_id = *((int*) arg);

    dev->pwm_chan_count = NRF_PWM_CHANNEL_COUNT;
    os_mutex_init(&dev->pwm_lock);

    OS_DEV_SETHANDLERS(odev, nrf52_pwm_open, nrf52_pwm_close);

    pwm_funcs = &dev->pwm_funcs;
    pwm_funcs->pwm_configure_device = nrf52_pwm_configure_device;
    pwm_funcs->pwm_configure_channel = nrf52_pwm_configure_channel;
    pwm_funcs->pwm_set_duty_cycle = nrf52_pwm_set_duty_cycle;
    pwm_funcs->pwm_enable = nrf52_pwm_enable;
    pwm_funcs->pwm_is_enabled = nrf52_pwm_is_enabled;
    pwm_funcs->pwm_set_frequency = nrf52_pwm_set_frequency;
    pwm_funcs->pwm_get_clock_freq = nrf52_pwm_get_clock_freq;
    pwm_funcs->pwm_get_top_value = nrf52_pwm_get_top_value;
    pwm_funcs->pwm_get_resolution_bits = nrf52_pwm_get_resolution_bits;
    pwm_funcs->pwm_disable = nrf52_pwm_disable;

    switch (dev->pwm_instance_id) {
#if MYNEWT_VAL(PWM_0)
    case 0:
        irqn = PWM0_IRQn;
        irqh = PWM_IRQ_HANDLER(0);
        break;
#endif

#if MYNEWT_VAL(PWM_1)
    case 1:
        irqn = PWM1_IRQn;
        irqh = PWM_IRQ_HANDLER(1);
        break;
#endif

#if MYNEWT_VAL(PWM_2)
    case 2:
        irqn = PWM2_IRQn;
        irqh = PWM_IRQ_HANDLER(2);
        break;
#endif

#if MYNEWT_VAL(PWM_3)
    case 3:
        irqn = PWM3_IRQn;
        irqh = PWM_IRQ_HANDLER(3);
        break;
#endif
    default:
        assert(0);
        return 0;
    }

    NVIC_SetVector(irqn, irqh);

    return (0);
}
