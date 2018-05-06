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

#if MYNEWT_VAL(MCU_STM32F3)
#   include "mcu/stm32f3_bsp.h"
#   include "stm32f3xx.h"
#   include "stm32f3xx_hal.h"
#   include "stm32f3xx_hal_tim.h"
#elif MYNEWT_VAL(MCU_STM32F7)
#   include "mcu/stm32f7_bsp.h"
#   include "stm32f7xx.h"
#   include "stm32f7xx_hal.h"
#   include "stm32f7xx_hal_tim.h"
#   include "stm32f7xx_ll_tim.h"
#else
#   error "MCU not yet supported."
#endif

#define STM32_PWM_CH_MAX      4
#define STM32_PWM_CH_DISABLED 0x0FFFFFFF
#define STM32_PWM_CH_NOPIN    0xFF
#define STM32_PWM_CH_NOAF     0x0F

typedef void (*stm32_pwm_isr_t)(void);

typedef struct {
    union {
        uint32_t      config;
        struct {
            uint32_t  duty:16;
            uint32_t  pin:8;
            uint32_t  af:4;
            uint32_t  invert:1;     /* invert output */
            uint32_t  enabled:1;    /* channel enabled */
            uint32_t  update:1;     /* channel needs update */
            uint32_t  configure:1;  /* channel needs HW configuration step */
        };
    };
    uint32_t cycle_count;
    uint32_t cycle;
    user_handler_t cycle_callback;
    user_handler_t sequence_callback;
    void *cycle_data;
    void *sequence_data;
} stm32_pwm_ch_t;

typedef struct {
    TIM_HandleTypeDef timx;
    stm32_pwm_ch_t    ch[STM32_PWM_CH_MAX];
} stm32_pwm_dev_t;

static stm32_pwm_dev_t stm32_pwm_dev[PWM_COUNT];

static uint32_t
stm32_pwm_ch(int ch)
{
    switch (ch) {
        case 0: return TIM_CHANNEL_1;
        case 1: return TIM_CHANNEL_2;
        case 2: return TIM_CHANNEL_3;
        case 3: return TIM_CHANNEL_4;
    }
    assert(0);
    return 0;
}

static inline bool
stm32_pwm_has_assigned_pin(const stm32_pwm_ch_t *ch)
{
    return STM32_PWM_CH_NOPIN != ch->pin && STM32_PWM_CH_NOAF != ch->af;
}

static int
stm32_pwm_disable_ch(stm32_pwm_dev_t *pwm, uint8_t cnum)
{
    if (HAL_OK != HAL_TIMEx_PWMN_Stop(&pwm->timx, stm32_pwm_ch(cnum))) {
        return STM32_PWM_ERR_HAL;
    }
    if (HAL_OK != HAL_TIM_PWM_Stop(&pwm->timx, stm32_pwm_ch(cnum))) {
        return STM32_PWM_ERR_HAL;
    }

    if (stm32_pwm_has_assigned_pin(&pwm->ch[cnum])) {
        /* unconfigure the previously used pin */
        if (hal_gpio_init_af(pwm->ch[cnum].pin,
                             0,
                             HAL_GPIO_PULL_NONE,
                             0)) {
            return STM32_PWM_ERR_GPIO;
        }
    }

    pwm->ch[cnum].config = STM32_PWM_CH_DISABLED;

    for (int i=0; i<STM32_PWM_CH_MAX; ++i) {
        if (pwm->ch[i].enabled) {
            return STM32_PWM_ERR_OK;
        }
    }

    __HAL_TIM_DISABLE_IT(&pwm->timx, TIM_IT_UPDATE);
    return STM32_PWM_ERR_OK;
}

/*
 * This could be more efficient by using different implementations of ISRS for the
 * individual timers. But some timers share the interrupt anyway so we would still
 * have to go and look for the trigger. And, the number of PWM peripherals is most
 * likely rather low.
 */
static void
stm32_pwm_isr()
{
    for (int i=0; i<PWM_COUNT; ++i) {
        stm32_pwm_dev_t *pwm = &stm32_pwm_dev[i];
        uint32_t sr = pwm->timx.Instance->SR;
        pwm->timx.Instance->SR = ~sr;

        if (sr & TIM_IT_UPDATE) {
            for (int j=0; j<STM32_PWM_CH_MAX; ++j) {
                stm32_pwm_ch_t *ch = &pwm->ch[j];
                if (ch->enabled) {
                    if (1 == ch->cycle) {
                        ch->cycle = ch->cycle_count;
                        if (ch->sequence_callback) {
                            ch->sequence_callback(ch->sequence_data);
                        } else {
                            stm32_pwm_disable_ch(pwm, j);
                        }
                    } else {
                        if (ch->cycle) {
                            --ch->cycle;
                        }
                        if (ch->cycle_callback) {
                            ch->cycle_callback(ch->cycle_data);
                        }
                    }
                }
            }
        }
    }
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
stm32_pwm_update_channels(stm32_pwm_dev_t *pwm, bool update_all)
{
    TIM_OC_InitTypeDef cfg;
    int rc = STM32_PWM_ERR_OK;

    cfg.OCMode        = TIM_OCMODE_PWM1;
    cfg.OCFastMode    = TIM_OCFAST_DISABLE;
    cfg.OCNIdleState  = TIM_OCNIDLESTATE_RESET;
    cfg.OCIdleState   = TIM_OCIDLESTATE_RESET;

    for (int i=0; i<STM32_PWM_CH_MAX; ++i) {
        if (STM32_PWM_CH_DISABLED != pwm->ch[i].config) {
            if (pwm->ch[i].enabled && (update_all || pwm->ch[i].update)) {
                cfg.Pulse = pwm->ch[i].duty;
                if (pwm->ch[i].configure) {
                    if (pwm->ch[i].invert) {
                        cfg.OCPolarity  = TIM_OCPOLARITY_LOW;
                        cfg.OCNPolarity = TIM_OCNPOLARITY_HIGH;
                    } else {
                        cfg.OCPolarity  = TIM_OCPOLARITY_HIGH;
                        cfg.OCNPolarity = TIM_OCNPOLARITY_LOW;
                    }

                    if (HAL_OK != HAL_TIM_PWM_ConfigChannel(&pwm->timx, &cfg, stm32_pwm_ch(i))) {
                        rc = STM32_PWM_ERR_HAL;
                    }

                    if (HAL_OK != HAL_TIM_PWM_Start(&pwm->timx, stm32_pwm_ch(i))) {
                        rc = STM32_PWM_ERR_HAL;
                    }
                    if (HAL_OK != HAL_TIMEx_PWMN_Start(&pwm->timx, stm32_pwm_ch(i))) {
                        rc = STM32_PWM_ERR_HAL;
                    }

                    pwm->ch[i].configure = false;
                } else {
                    __HAL_TIM_SET_COMPARE(&pwm->timx, stm32_pwm_ch(i), cfg.Pulse);
                }

                pwm->ch[i].update = false;
            }
        }
    }
    return rc;
}


static int
stm32_pwm_configure_channel(struct pwm_dev *dev, uint8_t cnum, struct pwm_chan_cfg *cfg)
{
    stm32_pwm_dev_t *pwm;
    stm32_pwm_ch_t *ch;
    uint8_t af;

    assert(dev);
    assert(dev->pwm_instance_id < PWM_COUNT);
    if (cnum >= dev->pwm_chan_count) {
        return STM32_PWM_ERR_CHAN;
    }

    pwm = &stm32_pwm_dev[dev->pwm_instance_id];
    ch = &pwm->ch[cnum];
    af = (uint8_t)(uintptr_t)cfg->data & 0x0F;

    if (cfg->pin != ch->pin || af != ch->af) {
        if (stm32_pwm_has_assigned_pin(ch)) {
            /* unconfigure the previously used pin */
            if (hal_gpio_init_af(ch->pin, 0, HAL_GPIO_PULL_NONE, 0)) {
                return STM32_PWM_ERR_GPIO;
            }
        }

        if (STM32_PWM_CH_NOPIN != cfg->pin && STM32_PWM_CH_NOAF != af) {
            /* configure the assigned pin */
            if (hal_gpio_init_af(cfg->pin, af, HAL_GPIO_PULL_NONE, 0)) {
                return STM32_PWM_ERR_GPIO;
            }
        }
    }

    ch->pin         = cfg->pin;
    ch->af          = af;
    ch->invert      = cfg->inverted;
    ch->update      = ch->enabled;
    ch->configure   = true;

    ch->cycle_count = cfg->n_cycles;
    ch->cycle       = cfg->n_cycles;

    if (cfg->interrupts_cfg) {
        struct pwm_dev_interrupt_cfg *icfg = (struct pwm_dev_interrupt_cfg*)cfg;
        ch->cycle_callback    = icfg->cycle_handler;
        ch->cycle_data        = icfg->cycle_data;
        ch->sequence_callback = icfg->seq_end_handler;
        ch->sequence_data     = icfg->seq_end_data;
    } else {
        ch->cycle_callback    = 0;
        ch->cycle_data        = 0;
        ch->sequence_callback = 0;
        ch->sequence_data     = 0;
    }

    if (cfg->interrupts_cfg || cfg->n_cycles) {
        __HAL_TIM_ENABLE_IT(&pwm->timx, TIM_IT_UPDATE);
    }

    return stm32_pwm_update_channels(pwm, false);
}


static int
stm32_pwm_enable_duty_cycle(struct pwm_dev *dev, uint8_t cnum, uint16_t fraction)
{
    stm32_pwm_dev_t *pwm;

    assert(dev);
    assert(dev->pwm_instance_id < PWM_COUNT);
    if (cnum >= dev->pwm_chan_count) {
        return STM32_PWM_ERR_CHAN;
    }

    pwm = &stm32_pwm_dev[dev->pwm_instance_id];
    pwm->ch[cnum].duty    = fraction;
    pwm->ch[cnum].update  = true;
    pwm->ch[cnum].enabled = true;

    return stm32_pwm_update_channels(pwm, false);
}

static int
stm32_pwm_disable(struct pwm_dev *dev, uint8_t cnum)
{
    stm32_pwm_dev_t *pwm;

    assert(dev);
    assert(dev->pwm_instance_id < PWM_COUNT);
    if (cnum >= dev->pwm_chan_count) {
        return STM32_PWM_ERR_CHAN;
    }

    pwm = &stm32_pwm_dev[dev->pwm_instance_id];

    return stm32_pwm_disable_ch(pwm, cnum);
}

static int
stm32_pwm_set_frequency(struct pwm_dev *dev, uint32_t freq_hz)
{
    stm32_pwm_dev_t *pwm;
    uint32_t id;
    uint32_t timer_clock;
    uint32_t freq_base;

    assert(dev);
    assert(dev->pwm_instance_id < PWM_COUNT);

    if (!freq_hz) {
        return STM32_PWM_ERR_FREQ;
    }

    id = dev->pwm_instance_id;
    pwm = &stm32_pwm_dev[id];

    timer_clock = stm32_hal_timer_get_freq(pwm->timx.Instance);
    assert(timer_clock);

    if (freq_hz > timer_clock) {
      pwm->timx.Init.Prescaler = 0;
      pwm->timx.Init.Period = 1;
    } else {
      if (freq_hz > 0xFFFFu) {
          pwm->timx.Init.Prescaler = 0;
      } else {
          pwm->timx.Init.Prescaler = timer_clock / (0x10000 * freq_hz);
          if (0x10000 * freq_hz * pwm->timx.Init.Prescaler == timer_clock) {
              pwm->timx.Init.Prescaler -= 1;
          }
      }
      freq_base = timer_clock / (pwm->timx.Init.Prescaler + 1);
      pwm->timx.Init.Period            = freq_base / freq_hz - 1;
    }
    pwm->timx.Init.ClockDivision     = 0;
    pwm->timx.Init.CounterMode       = TIM_COUNTERMODE_UP;
    pwm->timx.Init.RepetitionCounter = 0;
    pwm->timx.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    if (HAL_OK != HAL_TIM_PWM_Init(&pwm->timx)) {
        return STM32_PWM_ERR_HAL;
    }

    return stm32_pwm_update_channels(pwm, true);
}

static int
stm32_pwm_get_clock_freq(struct pwm_dev *dev)
{
    stm32_pwm_dev_t *pwm;

    assert(dev);
    assert(dev->pwm_instance_id < PWM_COUNT);

    pwm = &stm32_pwm_dev[dev->pwm_instance_id];
    return stm32_hal_timer_get_freq(pwm->timx.Instance) / (pwm->timx.Init.Prescaler + 1);
}

static int
stm32_pwm_get_top_value(struct pwm_dev *dev)
{
    stm32_pwm_dev_t *pwm;

    assert(dev);
    assert(dev->pwm_instance_id < PWM_COUNT);

    pwm = &stm32_pwm_dev[dev->pwm_instance_id];
    return LL_TIM_GetAutoReload(pwm->timx.Instance) + 1;
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
        if (!pwm->timx.Instance) {
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

    pwm->timx.Instance = cfg->tim;
    pwm->timx.Init.Period = 0;

    dev = (struct pwm_dev *)odev;
    dev->pwm_instance_id = id;

    switch ((uintptr_t)cfg->tim) {
#ifdef TIM1
      case (uintptr_t)TIM1:
          __HAL_RCC_TIM1_CLK_ENABLE();
          dev->pwm_chan_count = 4;
          break;
#endif
#ifdef TIM2
      case (uintptr_t)TIM2:
          __HAL_RCC_TIM2_CLK_ENABLE();
          dev->pwm_chan_count = 4;
          break;
#endif
#ifdef TIM3
      case (uintptr_t)TIM3:
          __HAL_RCC_TIM3_CLK_ENABLE();
          dev->pwm_chan_count = 4;
          break;
#endif
#ifdef TIM4
      case (uintptr_t)TIM4:
          __HAL_RCC_TIM4_CLK_ENABLE();
          dev->pwm_chan_count = 4;
          break;
#endif
#ifdef TIM5
      case (uintptr_t)TIM5:
          __HAL_RCC_TIM5_CLK_ENABLE();
          dev->pwm_chan_count = 4;
          break;
#endif

      /* basic timers TIM6 and TIM7 have no PWM capabilities */

#ifdef TIM8
      case (uintptr_t)TIM8:
          __HAL_RCC_TIM8_CLK_ENABLE();
          dev->pwm_chan_count = 4;
          break;
#endif
#ifdef TIM9
      case (uintptr_t)TIM9:
          __HAL_RCC_TIM9_CLK_ENABLE();
          dev->pwm_chan_count = 2;
          break;
#endif
#ifdef TIM10
      case (uintptr_t)TIM10:
          __HAL_RCC_TIM10_CLK_ENABLE();
          dev->pwm_chan_count = 1;
          break;
#endif
#ifdef TIM11
      case (uintptr_t)TIM11:
          __HAL_RCC_TIM11_CLK_ENABLE();
          dev->pwm_chan_count = 1;
          break;
#endif
#ifdef TIM12
      case (uintptr_t)TIM12:
          __HAL_RCC_TIM12_CLK_ENABLE();
          dev->pwm_chan_count = 2;
          break;
#endif
#ifdef TIM13
      case (uintptr_t)TIM13:
          __HAL_RCC_TIM13_CLK_ENABLE();
          dev->pwm_chan_count = 1;
          break;
#endif
#ifdef TIM14
      case (uintptr_t)TIM14:
          __HAL_RCC_TIM14_CLK_ENABLE();
          dev->pwm_chan_count = 1;
          break;
#endif
#ifdef TIM15
      case (uintptr_t)TIM15:
          __HAL_RCC_TIM15_CLK_ENABLE();
          dev->pwm_chan_count = 2;
          break;
#endif
#ifdef TIM16
      case (uintptr_t)TIM16:
          __HAL_RCC_TIM16_CLK_ENABLE();
          dev->pwm_chan_count = 1;
          break;
#endif
#ifdef TIM17
      case (uintptr_t)TIM17:
          __HAL_RCC_TIM17_CLK_ENABLE();
          dev->pwm_chan_count = 1;
          break;
#endif

      /* basic timer TIM18 has no PWM capabilities */

#ifdef TIM19
      case (uintptr_t)TIM19:
          __HAL_RCC_TIM19_CLK_ENABLE();
          dev->pwm_chan_count = 4;
          break;
#endif
#ifdef TIM20
      case (uintptr_t)TIM20:
          __HAL_RCC_TIM20_CLK_ENABLE();
          dev->pwm_chan_count = 4;
          break;
#endif

      default:
          assert(0);
    }

    for (int i=0; i<STM32_PWM_CH_MAX; ++i) {
        pwm->ch[i].config = STM32_PWM_CH_DISABLED;
    }

    dev->pwm_funcs.pwm_configure_channel = stm32_pwm_configure_channel;
    dev->pwm_funcs.pwm_enable_duty_cycle = stm32_pwm_enable_duty_cycle;
    dev->pwm_funcs.pwm_set_frequency = stm32_pwm_set_frequency;
    dev->pwm_funcs.pwm_get_clock_freq = stm32_pwm_get_clock_freq;
    dev->pwm_funcs.pwm_get_resolution_bits = stm32_pwm_get_resolution_bits;
    dev->pwm_funcs.pwm_get_top_value = stm32_pwm_get_top_value;
    dev->pwm_funcs.pwm_disable = stm32_pwm_disable;

    os_mutex_init(&dev->pwm_lock);
    OS_DEV_SETHANDLERS(odev, stm32_pwm_open, stm32_pwm_close);

    NVIC_SetPriority(cfg->irq, (1 << __NVIC_PRIO_BITS) - 1);
    NVIC_SetVector(cfg->irq, (uintptr_t)stm32_pwm_isr);
    NVIC_EnableIRQ(cfg->irq);

    return STM32_PWM_ERR_OK;
}
