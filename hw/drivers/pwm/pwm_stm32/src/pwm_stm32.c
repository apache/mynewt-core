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
#include "pwm_stm32/pwm_stm32.h"

#include <bsp.h>
#include <hal/hal_bsp.h>
#include <mcu/cmsis_nvic.h>
#include <os/os.h>
#include <pwm/pwm.h>
#include <stm32/stm32_hal.h>

#include <assert.h>
#include <stdint.h>
#include <string.h>

#define STM32_PWM_CH_MAX      4
#define STM32_PWM_CH_DISABLED 0x0FFFFFFF
#define STM32_PWM_CH_NOPIN    0xFF
#define STM32_PWM_CH_NOAF     0x0F

#define STM32_PWM_CH_IDLE     0x0000

typedef void (*stm32_pwm_isr_t)(void);

typedef struct {
    TIM_TypeDef       *timx;
} stm32_pwm_dev_t;

static stm32_pwm_dev_t stm32_pwm_dev[PWM_COUNT];

static uint32_t
stm32_pwm_ch(int ch)
{
    switch (ch) {
        case 0: return LL_TIM_CHANNEL_CH1;
        case 1: return LL_TIM_CHANNEL_CH2;
        case 2: return LL_TIM_CHANNEL_CH3;
        case 3: return LL_TIM_CHANNEL_CH4;
    }
    assert(0);
    return 0;
}

static void
stm32_pwm_ch_set_compare(TIM_TypeDef *tim, int ch, uint32_t value)
{
    switch (ch) {
        case 0:
            LL_TIM_OC_SetCompareCH1(tim, value);
            break;
        case 1:
            LL_TIM_OC_SetCompareCH2(tim, value);
            break;
        case 2:
            LL_TIM_OC_SetCompareCH3(tim, value);
            break;
        case 3:
            LL_TIM_OC_SetCompareCH4(tim, value);
            break;
    }
}

static int
stm32_pwm_enable(struct pwm_dev *dev)
{
    stm32_pwm_dev_t *pwm;

    assert(dev);
    assert(dev->pwm_instance_id < PWM_COUNT);

    pwm = &stm32_pwm_dev[dev->pwm_instance_id];

    LL_TIM_EnableCounter(pwm->timx);
    return STM32_PWM_ERR_OK;
}

static int
stm32_pwm_disable(struct pwm_dev *dev)
{
    stm32_pwm_dev_t *pwm;

    assert(dev);
    assert(dev->pwm_instance_id < PWM_COUNT);

    pwm = &stm32_pwm_dev[dev->pwm_instance_id];

    LL_TIM_DisableCounter(pwm->timx);
    LL_TIM_SetCounter(pwm->timx, 0);

    return STM32_PWM_ERR_OK;
}

static int
stm32_pwm_open(struct os_dev *odev, uint32_t wait, void *arg)
{
    struct pwm_dev *dev;
    int rc;

    dev = (struct pwm_dev *)odev;
    assert(dev);

    if (os_started()) {
        rc = os_mutex_pend(&dev->pwm_lock, wait);
        if (OS_OK != rc) {
            return rc;
        }
    }

    if (odev->od_flags & OS_DEV_F_STATUS_OPEN) {
        os_mutex_release(&dev->pwm_lock);
        rc = OS_EBUSY;
        return rc;
    }

    return STM32_PWM_ERR_OK;
}

static int
stm32_pwm_close(struct os_dev *odev)
{
    struct pwm_dev *dev;

    dev = (struct pwm_dev *)odev;
    assert(dev);

    if (os_started()) {
        os_mutex_release(&dev->pwm_lock);
    }

    return STM32_PWM_ERR_OK;
}

static int
stm32_pwm_configure_channel(struct pwm_dev *dev, uint8_t cnum, struct pwm_chan_cfg *cfg)
{
    stm32_pwm_dev_t *pwm;
    uint8_t af;

    assert(dev);
    assert(dev->pwm_instance_id < PWM_COUNT);
    if (cnum >= dev->pwm_chan_count) {
        return STM32_PWM_ERR_CHAN;
    }

    pwm = &stm32_pwm_dev[dev->pwm_instance_id];
    af = (uint8_t)(uintptr_t)cfg->data & 0x0F;

    uint32_t channelID = stm32_pwm_ch(cnum);
    LL_TIM_OC_SetMode(pwm->timx, channelID, LL_TIM_OCMODE_PWM2);
    LL_TIM_OC_SetPolarity(pwm->timx, channelID, cfg->inverted ? LL_TIM_OCPOLARITY_HIGH : LL_TIM_OCPOLARITY_LOW);
    LL_TIM_OC_EnablePreload(pwm->timx,  channelID);

    stm32_pwm_ch_set_compare(pwm->timx, cnum, STM32_PWM_CH_IDLE);

    if (STM32_PWM_CH_NOPIN != cfg->pin && STM32_PWM_CH_NOAF != af) {
      if (hal_gpio_init_af(cfg->pin, af, HAL_GPIO_PULL_NONE, 0)) {
        return STM32_PWM_ERR_GPIO;
      }
    }

    LL_TIM_CC_EnableChannel(pwm->timx, channelID);
    return STM32_PWM_ERR_OK;
}


static int
stm32_pwm_set_duty_cycle(struct pwm_dev *dev, uint8_t cnum, uint16_t fraction)
{
    stm32_pwm_dev_t *pwm;

    assert(dev);
    assert(dev->pwm_instance_id < PWM_COUNT);
    if (cnum >= dev->pwm_chan_count) {
        return STM32_PWM_ERR_CHAN;
    }

    pwm = &stm32_pwm_dev[dev->pwm_instance_id];

    stm32_pwm_ch_set_compare(pwm->timx, cnum, fraction);

    return STM32_PWM_ERR_OK;
}

static int
stm32_pwm_set_frequency(struct pwm_dev *dev, uint32_t freq_hz)
{
    stm32_pwm_dev_t *pwm;
    uint32_t id;
    uint32_t timer_clock;
    uint32_t div, div1, div2;

    assert(dev);
    assert(dev->pwm_instance_id < PWM_COUNT);

    if (!freq_hz) {
        return STM32_PWM_ERR_FREQ;
    }

    id = dev->pwm_instance_id;
    pwm = &stm32_pwm_dev[id];

    timer_clock = stm32_hal_timer_get_freq(pwm->timx);
    assert(timer_clock);

    div  = timer_clock / freq_hz;
    if (!div) {
        return STM32_PWM_ERR_FREQ;
    }

    div1 = div >> 16;
    div2 = div / (div1 + 1);

    if (div1 > div2) {
        uint32_t tmp = div1;
        div1 = div2;
        div2 = tmp;
    }
    div2 -= 1;

    LL_TIM_SetPrescaler(pwm->timx, div1);
    LL_TIM_SetAutoReload(pwm->timx, div2);

    return STM32_PWM_ERR_OK;
}

static int
stm32_pwm_get_clock_freq(struct pwm_dev *dev)
{
    stm32_pwm_dev_t *pwm;

    assert(dev);
    assert(dev->pwm_instance_id < PWM_COUNT);

    pwm = &stm32_pwm_dev[dev->pwm_instance_id];
    return stm32_hal_timer_get_freq(pwm->timx) / (LL_TIM_GetPrescaler(pwm->timx) + 1);
}

static int
stm32_pwm_get_top_value(struct pwm_dev *dev)
{
    stm32_pwm_dev_t *pwm;

    assert(dev);
    assert(dev->pwm_instance_id < PWM_COUNT);

    pwm = &stm32_pwm_dev[dev->pwm_instance_id];
    return LL_TIM_GetAutoReload(pwm->timx) + 1;
}

static int
stm32_pwm_get_resolution_bits(struct pwm_dev *dev)
{
    uint32_t period = stm32_pwm_get_top_value(dev) - 1;

    for (int bit=15; bit; --bit) {
        if (period & (0x01 << bit)) {
            return bit + 1;
        }
    }

    return period ? 1 : 0;
}


int
stm32_pwm_dev_init(struct os_dev *odev, void *arg)
{
    stm32_pwm_conf_t *cfg;
    stm32_pwm_dev_t *pwm;
    struct pwm_dev *dev;
    uint32_t id;

    for (id = 0; id < PWM_COUNT; ++id) {
        pwm = &stm32_pwm_dev[id];
        if (!pwm->timx) {
            break;
        }
    }
    if (PWM_COUNT <= id) {
        return STM32_PWM_ERR_NODEV;
    }

    if (NULL == arg) {
        return STM32_PWM_ERR_NOTIM;
    }

    cfg = (stm32_pwm_conf_t*)arg;

    pwm->timx = cfg->tim;

    LL_TIM_SetPrescaler(cfg->tim, 0xffff);
    LL_TIM_SetAutoReload(cfg->tim, 0);

    dev = (struct pwm_dev *)odev;
    dev->pwm_instance_id = id;

    switch ((uintptr_t)cfg->tim) {
#ifdef TIM1
      case (uintptr_t)TIM1:
          LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM1);
          dev->pwm_chan_count = 4;
          break;
#endif
#ifdef TIM2
      case (uintptr_t)TIM2:
          LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM2);
          dev->pwm_chan_count = 4;
          break;
#endif
#ifdef TIM3
      case (uintptr_t)TIM3:
          LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM3);
          dev->pwm_chan_count = 4;
          break;
#endif
#ifdef TIM4
      case (uintptr_t)TIM4:
          LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM4);
          dev->pwm_chan_count = 4;
          break;
#endif
#ifdef TIM5
      case (uintptr_t)TIM5:
          LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM5);
          dev->pwm_chan_count = 4;
          break;
#endif

      /* basic timers TIM6 and TIM7 have no PWM capabilities */

#ifdef TIM8
      case (uintptr_t)TIM8:
          LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM8);
          dev->pwm_chan_count = 4;
          break;
#endif
#ifdef TIM9
      case (uintptr_t)TIM9:
          LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM9);
          dev->pwm_chan_count = 2;
          break;
#endif
#ifdef TIM10
      case (uintptr_t)TIM10:
          LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM10);
          dev->pwm_chan_count = 1;
          break;
#endif
#ifdef TIM11
      case (uintptr_t)TIM11:
          LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM11);
          dev->pwm_chan_count = 1;
          break;
#endif
#ifdef TIM12
      case (uintptr_t)TIM12:
          LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM12);
          dev->pwm_chan_count = 2;
          break;
#endif
#ifdef TIM13
      case (uintptr_t)TIM13:
          LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM13);
          dev->pwm_chan_count = 1;
          break;
#endif
#ifdef TIM14
      case (uintptr_t)TIM14:
          LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM14);
          dev->pwm_chan_count = 1;
          break;
#endif
#ifdef TIM15
      case (uintptr_t)TIM15:
          LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM15);
          dev->pwm_chan_count = 2;
          break;
#endif
#ifdef TIM16
      case (uintptr_t)TIM16:
          LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM16);
          dev->pwm_chan_count = 1;
          break;
#endif
#ifdef TIM17
      case (uintptr_t)TIM17:
          LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM17);
          dev->pwm_chan_count = 1;
          break;
#endif

      /* basic timer TIM18 has no PWM capabilities */

#ifdef TIM19
      case (uintptr_t)TIM19:
          LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM19);
          dev->pwm_chan_count = 4;
          break;
#endif
#ifdef TIM20
      case (uintptr_t)TIM20:
          LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM20);
          dev->pwm_chan_count = 4;
          break;
#endif

      default:
          assert(0);
    }

    for (int i=0; i<STM32_PWM_CH_MAX; ++i) {
        stm32_pwm_ch_set_compare(pwm->timx, i, STM32_PWM_CH_IDLE);
    }

    dev->pwm_funcs.pwm_configure_channel = stm32_pwm_configure_channel;
    dev->pwm_funcs.pwm_set_duty_cycle = stm32_pwm_set_duty_cycle;
    dev->pwm_funcs.pwm_set_frequency = stm32_pwm_set_frequency;
    dev->pwm_funcs.pwm_get_clock_freq = stm32_pwm_get_clock_freq;
    dev->pwm_funcs.pwm_get_resolution_bits = stm32_pwm_get_resolution_bits;
    dev->pwm_funcs.pwm_get_top_value = stm32_pwm_get_top_value;
    dev->pwm_funcs.pwm_enable = stm32_pwm_enable;
    dev->pwm_funcs.pwm_disable = stm32_pwm_disable;

    os_mutex_init(&dev->pwm_lock);
    OS_DEV_SETHANDLERS(odev, stm32_pwm_open, stm32_pwm_close);

    LL_TIM_EnableARRPreload(cfg->tim);
    LL_TIM_CC_EnablePreload(cfg->tim);
    LL_TIM_SetCounter(pwm->timx, 0);

#if 0
    LL_TIM_EnableIT_UPDATE(cfg->tim);
    NVIC_SetPriority(cfg->irq, (1 << __NVIC_PRIO_BITS) - 1);
    NVIC_SetVector(cfg->irq, (uintptr_t)stm32_pwm_isr);
    NVIC_EnableIRQ(cfg->irq);
#endif

    return STM32_PWM_ERR_OK;
}
