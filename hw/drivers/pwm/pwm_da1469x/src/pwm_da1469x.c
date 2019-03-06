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

#include <stdint.h>
#include "syscfg/syscfg.h"
#include "pwm/pwm.h"
#include "os/util.h"
#include "mcu/mcu.h"
#include "mcu/da1469x_hal.h"
#include <DA1469xAB.h>

struct da1469x_pwm {
    TIMER_Type *timer_regs;
    uint32_t freq;
    uint8_t gpio_func;
    uint8_t in_use;
};

#if MYNEWT_VAL(PWM_0)
static struct da1469x_pwm da1469x_pwm_0 = {
    .gpio_func = MCU_GPIO_FUNC_TIM_PWM,
    .timer_regs = (void *)TIMER_BASE,
};
#endif

#if MYNEWT_VAL(PWM_1)
static struct da1469x_pwm da1469x_pwm_3 = {
    .gpio_func = MCU_GPIO_FUNC_TIM3_PWM,
    .timer_regs = (void *)TIMER3_BASE,
};
#endif

#if MYNEWT_VAL(PWM_2)
static struct da1469x_pwm da1469x_pwm_4 = {
    .gpio_func = MCU_GPIO_FUNC_TIM4_PWM,
    .timer_regs = (void *)TIMER4_BASE,
};
#endif

#define DA1469X_PWM_MAX (3)

static struct da1469x_pwm *da1469x_pwms[DA1469X_PWM_MAX] = {
#if MYNEWT_VAL(PWM_0)
    &da1469x_pwm_0,
#else
    NULL,
#endif
#if MYNEWT_VAL(PWM_1)
    &da1469x_pwm_3,
#else
    NULL,
#endif
#if MYNEWT_VAL(PWM_2)
    &da1469x_pwm_4,
#else
    NULL,
#endif
};

static struct da1469x_pwm *
da1469x_pwm_resolve(uint8_t pwm_num)
{
    if (pwm_num >= DA1469X_PWM_MAX) {
        return NULL;
    }

    return da1469x_pwms[pwm_num];
}

static int
da1469x_pwm_configure_channel(struct pwm_dev *dev, uint8_t channel,
                              struct pwm_chan_cfg *cfg)
{
    struct da1469x_pwm *pwm;

    pwm = da1469x_pwm_resolve(dev->pwm_instance_id);
    if (!pwm || !pwm->in_use) {
       return SYS_EINVAL;
    }

    mcu_gpio_set_pin_function(cfg->pin, MCU_GPIO_MODE_OUTPUT, pwm->gpio_func);

    return 0;
}

static int
da1469x_pwm_configure_device(struct pwm_dev *dev, struct pwm_dev_cfg *cfg)
{
    /* Since PWM is independent from Timer it is not possible
     * to support user callbacks for cycle and sequence end.
     */
    return SYS_ENOTSUP;
}

static int
da1469x_pwm_disable(struct pwm_dev *dev)
{
    struct da1469x_pwm *pwm;

    pwm = da1469x_pwm_resolve(dev->pwm_instance_id);
    if (!pwm || !pwm->in_use) {
        return SYS_EINVAL;
    }

    pwm->timer_regs->TIMER_CTRL_REG &= ~(TIMER_TIMER_CTRL_REG_TIM_CLK_EN_Msk |
                                         TIMER_TIMER_CTRL_REG_TIM_FREE_RUN_MODE_EN_Msk |
                                         TIMER_TIMER_CTRL_REG_TIM_EN_Msk);

    return 0;
}

static int
da1469x_pwm_calculate_freq(int freq_hz, uint32_t *sys_clk_en, uint32_t *out_freq)
{
    int base_freq_hz;
    int higher_freq_hz;
    int lower_freq_hz;
    int div;

    /* We can support 1-16 MHz */
    if (!((freq_hz >= 1) && (freq_hz <= 16000000))) {
        return SYS_EINVAL;
    }

    if (freq_hz <= 16384) {
        base_freq_hz = 32768;
        *sys_clk_en = 0 << TIMER_TIMER_CTRL_REG_TIM_SYS_CLK_EN_Pos;
    } else {
        base_freq_hz = 32000000;
        *sys_clk_en = 1 << TIMER_TIMER_CTRL_REG_TIM_SYS_CLK_EN_Pos;
    }

    /*
     * Compute divider, it can be rounded down due to integers
     * so frequency could be a bit higher than requested
     */
    div = base_freq_hz / freq_hz;

    higher_freq_hz = base_freq_hz / div;

    /* Compute next lower frequency */
    lower_freq_hz = base_freq_hz / (div + 1);

    /* Choose frequency which is closer to requested one */
    if (freq_hz - lower_freq_hz <= higher_freq_hz - freq_hz) {
        *out_freq = lower_freq_hz;
        div++;
    } else {
        *out_freq = higher_freq_hz;
    }

    assert((div - 1) <= 0xffff);

    return div - 1;
}

static int
da1469x_pwm_enable(struct pwm_dev *dev)
{
    struct da1469x_pwm *pwm;

    pwm = da1469x_pwm_resolve(dev->pwm_instance_id);
    if (!pwm || !pwm->in_use || pwm->freq == 0) {
        return SYS_EINVAL;
    }

    pwm->timer_regs->TIMER_CTRL_REG |= (TIMER_TIMER_CTRL_REG_TIM_CLK_EN_Msk |
                                        TIMER_TIMER_CTRL_REG_TIM_FREE_RUN_MODE_EN_Msk |
                                        TIMER_TIMER_CTRL_REG_TIM_EN_Msk);

    return 0;
}

static bool
da1469x_pwm_is_enabled(struct pwm_dev *dev)
{
    struct da1469x_pwm *pwm;

    pwm = da1469x_pwm_resolve(dev->pwm_instance_id);
    if (!pwm || !pwm->in_use) {
        return false;
    }

    return pwm->timer_regs->TIMER_CTRL_REG & TIMER_TIMER_CTRL_REG_TIM_CLK_EN_Msk;
}

static int
da1469x_pwm_get_clock_freq(struct pwm_dev *dev)
{
    struct da1469x_pwm *pwm;

    pwm = da1469x_pwm_resolve(dev->pwm_instance_id);
    if (!pwm || !pwm->in_use) {
        return SYS_EINVAL;
    }

    return pwm->freq;
}

static int
da1469x_pwm_set_freq(struct pwm_dev *dev, uint32_t freq)
{
    struct da1469x_pwm *pwm;
    int tim_pwm_freq;
    uint32_t sys_clk_en;
    uint32_t actual_freq;

    pwm = da1469x_pwm_resolve(dev->pwm_instance_id);
    if (!pwm || !pwm->in_use) {
        return SYS_EINVAL;
    }

    tim_pwm_freq = da1469x_pwm_calculate_freq(freq, &sys_clk_en, &actual_freq);
    if (tim_pwm_freq < 0) {
        return SYS_EINVAL;
    }

    pwm->timer_regs->TIMER_PRESCALER_REG = 0;
    pwm->timer_regs->TIMER_CTRL_REG = sys_clk_en;
    pwm->timer_regs->TIMER_PWM_FREQ_REG = tim_pwm_freq;

    pwm->freq = actual_freq;

    return actual_freq;
}

static int
da1469x_pwm_get_top_value(struct pwm_dev *dev)
{
    struct da1469x_pwm *pwm;

    pwm = da1469x_pwm_resolve(dev->pwm_instance_id);
    if (!pwm || !pwm->in_use || pwm->freq == 0) {
        return SYS_EINVAL;
    }

    return pwm->timer_regs->TIMER_PWM_FREQ_REG + 1;
}

static int
da1469x_pwm_get_resolution_bits(struct pwm_dev *dev)
{
    int top;
    struct da1469x_pwm *pwm;

    pwm = da1469x_pwm_resolve(dev->pwm_instance_id);
    if (!pwm || !pwm->in_use) {
        return SYS_EINVAL;
    }

    top = da1469x_pwm_get_top_value(dev);

    if (top < 0) {
        return SYS_EINVAL;
    }

    return ((8 * sizeof(int)) - __builtin_clz(top) - 1);
}

static int
da1469x_pwm_set_duty_cycle(struct pwm_dev *dev, uint8_t cnum, uint16_t fraction)
{
    struct da1469x_pwm *pwm;

    pwm = da1469x_pwm_resolve(dev->pwm_instance_id);
    if (!pwm || !pwm->in_use || pwm->freq == 0) {
        return SYS_EINVAL;
    }

    pwm->timer_regs->TIMER_PWM_DC_REG = fraction;

    return 0;
}

static int
da1469x_pwm_open(struct os_dev *odev, uint32_t wait, void *arg)
{
    struct pwm_dev *dev;
    struct da1469x_pwm *pwm;
    int rc;

    dev = (struct pwm_dev *) odev;
    assert(dev != NULL);

    if (os_started()) {
        rc = os_mutex_pend(&dev->pwm_lock, wait);
        if (rc != OS_OK) {
            return rc;
        }
    }

    if (odev->od_flags & OS_DEV_F_STATUS_OPEN) {
        if (os_started()) {
            os_mutex_release(&dev->pwm_lock);
        }
        return OS_EBUSY;
    }

    pwm = da1469x_pwm_resolve(dev->pwm_instance_id);
    if (!pwm) {
        if (os_started()) {
            os_mutex_release(&dev->pwm_lock);
        }
        return OS_ENOENT;
    }

    pwm->in_use = 1;

    return OS_OK;
}

static int
da1469x_pwm_close(struct os_dev *odev)
{
    struct pwm_dev *dev;
    struct da1469x_pwm *pwm;

    dev = (struct pwm_dev *) odev;
    assert(dev != NULL);

    pwm = da1469x_pwm_resolve(dev->pwm_instance_id);
    if (!pwm) {
        return OS_ENOENT;
    }

    if (!pwm->in_use) {
        return OS_EINVAL;
    }

    if (pwm->timer_regs->TIMER_CTRL_REG & TIMER_TIMER_CTRL_REG_TIM_CLK_EN_Msk) {
        da1469x_pwm_disable(dev);
    }

    pwm->in_use = 0;

    if (os_started()) {
        os_mutex_release(&dev->pwm_lock);
    }

    return OS_OK;
}

int
da1469x_pwm_init(struct os_dev *odev, void *arg)
{
    struct pwm_dev *dev;
    struct pwm_driver_funcs *pwm_funcs;

    dev = (struct pwm_dev *) odev;
    assert(dev != NULL);

    dev->pwm_instance_id = POINTER_TO_UINT(arg);
    dev->pwm_chan_count = 1;
    os_mutex_init(&dev->pwm_lock);

    pwm_funcs = &dev->pwm_funcs;

    pwm_funcs->pwm_configure_channel = da1469x_pwm_configure_channel;
    pwm_funcs->pwm_configure_device = da1469x_pwm_configure_device;
    pwm_funcs->pwm_disable = da1469x_pwm_disable;
    pwm_funcs->pwm_enable = da1469x_pwm_enable;
    pwm_funcs->pwm_is_enabled = da1469x_pwm_is_enabled;
    pwm_funcs->pwm_get_clock_freq = da1469x_pwm_get_clock_freq;
    pwm_funcs->pwm_set_frequency = da1469x_pwm_set_freq;
    pwm_funcs->pwm_get_resolution_bits = da1469x_pwm_get_resolution_bits;
    pwm_funcs->pwm_get_top_value = da1469x_pwm_get_top_value;
    pwm_funcs->pwm_set_duty_cycle = da1469x_pwm_set_duty_cycle;

    OS_DEV_SETHANDLERS(odev, da1469x_pwm_open, da1469x_pwm_close);

    return OS_OK;
}
