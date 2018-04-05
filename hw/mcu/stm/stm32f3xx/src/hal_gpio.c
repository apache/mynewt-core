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

/*
 * Currently no support for
 * - interrupts
 * - pin output speed
 */

#include "hal_gpio_stm32.h"
#include "hal/hal_gpio.h"
#include "mcu/cmsis_nvic.h"
#include "stm32f3xx.h"
#include "stm32f3xx_hal_cortex.h"
#include "stm32f3xx_hal_gpio.h"
#include "stm32f3xx_hal_rcc.h"
#include "mcu/stm32f3xx_mynewt_hal.h"
#include <assert.h>

 /* XXX: Notes
 * 1) Right now, we are not disabling the NVIC interrupt source; we only
 * disable the external interrupt from occurring. Dont think either way
 * to do it is an issue... when we release we may want to disable the NVIC
 *
 * 2) investigate how thread safe these routines are. HAL_GPIO_Init, for
 * example. Looks like if it gets interrupted while doing config an error
 * may occur. Read/modify write could cause screw-ups.
 *
 * 3) Currently, this code does not change the interrupt priority of the
 * external interrupt vectors in the NVIC. The application developer must
 * decide on the priority level for each external interrupt and program that
 * by using the CMSIS NVIC API  (NVIC_SetPriority and NVIC_SetPriorityGrouping)
 *
 * 4) The code probably does not handle "re-purposing" gpio very well.
 * "Re-purposing" means changing a gpio from input to output, or calling
 * gpio_init_in and expecting previously enabled interrupts to be stopped.
 *
 * 5) Possbily add access to HAL_GPIO_DeInit.
 */

/*
 * GPIO pin mapping
 *
 * The stm32F3xx processors have 16 gpio pins per port. We map the logical pin
 * numbers (from 0 to N) in the following manner:
 *      Port A: PA0-PA15 map to pins 0 - 15.
 *      Port B: PB0-PB15 map to pins 16 - 31.
 *      Port C: PC0-PC15 map to pins 32 - 47.
 *
 *      To convert a gpio to pin number, do the following:
 *          - Convert port label to its numeric value (A=0, B=1, C=2, etc).
 *          - Multiply by 16.
 *          - Add port pin number.
 *
 *      Ex: PD11 = (3 * 16) + 11 = 59.
 *          PA0 = (0 * 16) + 0 = 0
 */
#define HAL_GPIO_PIN_NUM(pin)   ((pin) & 0x0F)
#define HAL_GPIO_PORT(pin)      (((pin) >> 4) & 0x0F)
#define HAL_GPIO_PIN(pin)       (1 << HAL_GPIO_PIN_NUM(pin))

/* Port index to port address map */
static GPIO_TypeDef * const portmap[HAL_GPIO_PORT_COUNT] =
{
    HAL_GPIO_PORT_LIST
};

/* Storage for GPIO ISRS. */
struct hal_gpio_irq_isr_t
{
    hal_gpio_irq_handler_t  isr;
    void                    *arg;
    volatile uint32_t       invoked;
    volatile uint32_t       dropped;
};
struct hal_gpio_irq_isr_t hal_gpio_irq[16];

void hal_gpio_ext_irq_handler(uint16_t index)
{
    uint32_t mask;

    mask = HAL_GPIO_PIN(index);
    if (__HAL_GPIO_EXTI_GET_IT(mask) != RESET) {
        __HAL_GPIO_EXTI_CLEAR_IT(mask);
      if (hal_gpio_irq[index].isr) {
          ++hal_gpio_irq[index].invoked;
          hal_gpio_irq[index].isr(hal_gpio_irq[index].arg);
      } else {
          ++hal_gpio_irq[index].dropped;
      }
    }
}


/* External interrupt 0 */
static void
hal_gpio_ext_irq0(void)
{
    hal_gpio_ext_irq_handler(0);
}

/* External interrupt 1 */
static void
hal_gpio_ext_irq1(void)
{
    hal_gpio_ext_irq_handler(1);
}

/* External interrupt 2 */
static void
hal_gpio_ext_irq2(void)
{
    hal_gpio_ext_irq_handler(2);
}

/* External interrupt 3 */
static void
hal_gpio_ext_irq3(void)
{
    hal_gpio_ext_irq_handler(3);
}

/* External interrupt 4 */
static void
hal_gpio_ext_irq4(void)
{
    hal_gpio_ext_irq_handler(4);
}

/* External interrupt 9 through 5 */
static void
hal_gpio_ext_irq9_5(void)
{
    int index;

    for (index = 5; index <= 9; ++index) {
        hal_gpio_ext_irq_handler(index);
    }
}

/* External interrupt 15 through 10 */
static void
hal_gpio_ext_irq15_10(void)
{
    int index;

    for (index = 10; index <= 15; ++index) {
        hal_gpio_ext_irq_handler(index);
    }
}

/**
 * hal gpio clk enable
 *
 * Enable the port peripheral clock
 *
 * @param port_idx
 */
static void
hal_gpio_clk_enable(uint32_t port_idx)
{
    switch (port_idx) {
        case 0:
            if (!__HAL_RCC_GPIOA_IS_CLK_ENABLED()) {
                __HAL_RCC_GPIOA_CLK_ENABLE();
            }
            break;
#if HAL_GPIO_PORT_COUNT > 1
        case 1:
            if (!__HAL_RCC_GPIOB_IS_CLK_ENABLED()) {
                __HAL_RCC_GPIOB_CLK_ENABLE();
            }
            break;
#endif
#if HAL_GPIO_PORT_COUNT > 2
        case 2:
            if (!__HAL_RCC_GPIOC_IS_CLK_ENABLED()) {
                __HAL_RCC_GPIOC_CLK_ENABLE();
            }
            break;
#endif
#if HAL_GPIO_PORT_COUNT > 3
        case 3:
            if (!__HAL_RCC_GPIOD_IS_CLK_ENABLED()) {
                __HAL_RCC_GPIOD_CLK_ENABLE();
            }
            break;
#endif
#if HAL_GPIO_PORT_COUNT > 4 && defined GPIOE_BASE
        case 4:
            if (!__HAL_RCC_GPIOE_IS_CLK_ENABLED()) {
                __HAL_RCC_GPIOE_CLK_ENABLE();
            }
            break;
#endif
#if HAL_GPIO_PORT_COUNT > 5
        case 5:
            if (!__HAL_RCC_GPIOF_IS_CLK_ENABLED()) {
                __HAL_RCC_GPIOF_CLK_ENABLE();
            }
            break;
#endif
#if HAL_GPIO_PORT_COUNT > 6
        case 6:
            if (!__HAL_RCC_GPIOG_IS_CLK_ENABLED()) {
                __HAL_RCC_GPIOG_CLK_ENABLE();
            }
            break;
#endif
#if HAL_GPIO_PORT_COUNT > 7
        case 7:
            if (!__HAL_RCC_GPIOH_IS_CLK_ENABLED()) {
                __HAL_RCC_GPIOH_CLK_ENABLE();
            }
            break;
#endif
#if HAL_GPIO_PORT_COUNT > 8
        case 8:
            if (!__HAL_RCC_GPIOI_IS_CLK_ENABLED()) {
                __HAL_RCC_GPIOI_CLK_ENABLE();
            }
            break;
#endif
#if HAL_GPIO_PORT_COUNT > 9
        case 9:
            if (!__HAL_RCC_GPIOJ_IS_CLK_ENABLED()) {
                __HAL_RCC_GPIOJ_CLK_ENABLE();
            }
            break;
#endif
#if HAL_GPIO_PORT_COUNT > 10
        case 10:
            if (!__HAL_RCC_GPIOK_IS_CLK_ENABLED()) {
                __HAL_RCC_GPIOK_CLK_ENABLE();
            }
            break;
#endif
        default:
            assert(0);
            break;
    }
}

/**
 * hal gpio pin to irq
 *
 * Converts the logical pin number to the IRQ number associated with the
 * external interrupt for that particular GPIO.
 *
 * @param pin
 *
 * @return IRQn_Type
 */
static IRQn_Type
hal_gpio_pin_to_irq(int pin)
{
    int index;
    IRQn_Type irqn;

    index = HAL_GPIO_PIN_NUM(pin);
    if (index <= 4) {
        irqn = EXTI0_IRQn + index;
    } else if (index <=  9) {
        irqn = EXTI9_5_IRQn;
    } else {
        irqn = EXTI15_10_IRQn;
    }

    return irqn;
}

static void
hal_gpio_set_nvic(IRQn_Type irqn)
{
    void (*isr)(void);

    switch (irqn) {
    case EXTI0_IRQn:
        isr = hal_gpio_ext_irq0;
        break;
    case EXTI1_IRQn:
        isr = hal_gpio_ext_irq1;
        break;
    case EXTI2_TSC_IRQn:
        isr = hal_gpio_ext_irq2;
        break;
    case EXTI3_IRQn:
        isr = hal_gpio_ext_irq3;
        break;
    case EXTI4_IRQn:
        isr = hal_gpio_ext_irq4;
        break;
    case EXTI9_5_IRQn:
        isr = hal_gpio_ext_irq9_5;
        break;
    case EXTI15_10_IRQn:
        isr = hal_gpio_ext_irq15_10;
        break;
    default:
        assert(0);
        break;
    }

    /* Set isr in vector table if not yet set */
    if (NVIC_GetVector(irqn) != (uint32_t)isr) {
        NVIC_SetVector(irqn, (uint32_t)isr);
        NVIC_EnableIRQ(irqn);
    }
}


static int
hal_gpio_init_stm32_int(int pin, GPIO_InitTypeDef *cfg, GPIO_PinState *state)
{
    int port;
    uint32_t mcu_gpio_pin;

    /* Is this a valid pin? */
    port = HAL_GPIO_PORT(pin);
    if (port >= HAL_GPIO_PORT_COUNT) {
        return -1;
    }

    mcu_gpio_pin = HAL_GPIO_PIN(pin);
    cfg->Pin = mcu_gpio_pin;

    /* Enable the GPIO clock */
    hal_gpio_clk_enable(port);

    /* Write initial state if required */
    if (NULL != state) {
        HAL_GPIO_WritePin(portmap[port], mcu_gpio_pin, *state);
    }

    /* Initialize pin setting proper mode */
    HAL_GPIO_Init(portmap[port], cfg);

    return 0;
}

/**
 * hal gpio init
 *
 * Called to initialize a gpio.
 *
 * @param pin
 * @param cfg
 *
 * @return int
 */
int
hal_gpio_init_stm(int pin, GPIO_InitTypeDef *cfg)
{
  return hal_gpio_init_stm32_int(pin, cfg, NULL);
}
/**
 * hal gpio deinit
 *
 * Called to deinitialize a gpio.
 *
 * @param pin
 * @param cfg
 *
 * @return int
 */
int
hal_gpio_deinit_stm(int pin, GPIO_InitTypeDef *cfg)
{
    int port;
    uint32_t mcu_gpio_pin;

    /* Is this a valid pin? */
    port = HAL_GPIO_PORT(pin);
    if (port >= HAL_GPIO_PORT_COUNT) {
        return -1;
    }

    mcu_gpio_pin = HAL_GPIO_PIN(pin);
    cfg->Pin = mcu_gpio_pin;

    /* Initialize pin as an input, setting proper mode */
    HAL_GPIO_DeInit(portmap[port], cfg->Pin);

    return 0;
}

/**
 * gpio init in
 *
 * Initializes the specified pin as an input
 *
 * @param pin   Pin number to set as input
 * @param pull  pull type
 *
 * @return int  0: no error; -1 otherwise.
 */
int
hal_gpio_init_in(int pin, hal_gpio_pull_t pull)
{
    int rc;
    GPIO_InitTypeDef init_cfg;

    init_cfg.Mode = GPIO_MODE_INPUT;
    init_cfg.Pull = pull;

    rc = hal_gpio_init_stm(pin, &init_cfg);
    return rc;
}

/**
 * gpio init out
 *
 * Initialize the specified pin as an output, setting the pin to the specified
 * value.
 *
 * @param pin Pin number to set as output
 * @param val Value to set pin
 *
 * @return int  0: no error; -1 otherwise.
 */
int hal_gpio_init_out(int pin, int val)
{
    GPIO_InitTypeDef cfg;
    GPIO_PinState state;

    cfg.Mode  = GPIO_MODE_OUTPUT_PP;
    cfg.Pull  = GPIO_NOPULL;
    cfg.Speed = GPIO_SPEED_FREQ_HIGH;

    state = val ? GPIO_PIN_SET : GPIO_PIN_RESET;

    return hal_gpio_init_stm32_int(pin, &cfg, &state);
}

/**
 * gpio enumeration translation
 *
 * Convert hal pull types to mcu pull type
 *
 * @param pull
 *
 * @return uint32_t
 */
static uint32_t
hal_gpio_pull_to_stm_pull(enum hal_gpio_pull pull)
{
    switch (pull) {
        case HAL_GPIO_PULL_NONE:  return GPIO_NOPULL;
        case HAL_GPIO_PULL_UP:    return GPIO_PULLUP;
        case HAL_GPIO_PULL_DOWN:  return GPIO_PULLDOWN;
    }
    /* never reached */
    return GPIO_NOPULL;
}

/**
 * gpio init af
 *
 * Configure the specified pin for AF.
 */
int
hal_gpio_init_af(int pin, uint8_t af_type, enum hal_gpio_pull pull, uint8_t od)
{
    GPIO_InitTypeDef cfg;

    cfg.Mode      = od ? GPIO_MODE_AF_OD : GPIO_MODE_AF_PP;
    cfg.Speed     = GPIO_SPEED_FREQ_HIGH;
    cfg.Pull      = hal_gpio_pull_to_stm_pull(pull);
    cfg.Alternate = af_type;

    return hal_gpio_init_stm(pin, &cfg);
}

/**
 * gpio write
 *
 * Write a value (either high or low) to the specified pin.
 *
 * @param pin Pin to set
 * @param val Value to set pin (0:low 1:high)
 */
void hal_gpio_write(int pin, int val)
{
    int port;
    uint32_t mcu_gpio_pin;
    GPIO_PinState state;

    port         = HAL_GPIO_PORT(pin);
    mcu_gpio_pin = HAL_GPIO_PIN(pin);
    state        = val ? GPIO_PIN_SET : GPIO_PIN_RESET;

    HAL_GPIO_WritePin(portmap[port], mcu_gpio_pin, state);
}

/**
 * gpio read
 *
 * Reads the specified pin.
 *
 * @param pin Pin number to read
 *
 * @return int 0: low, 1: high
 */
int hal_gpio_read(int pin)
{
    int port;
    uint32_t mcu_gpio_pin;

    port = HAL_GPIO_PORT(pin);
    mcu_gpio_pin = HAL_GPIO_PIN(pin);
    return HAL_GPIO_ReadPin(portmap[port], mcu_gpio_pin);
}

/**
 * gpio toggle
 *
 * Toggles the specified pin
 *
 * @param pin Pin number to toggle
 *
 * @return current pin state int 0: low 1 : high
 */
int hal_gpio_toggle(int pin)
{
    int pin_state = (hal_gpio_read(pin) != 1);
    hal_gpio_write(pin, pin_state);
    return pin_state;
}

/**
 * gpio irq init
 *
 * Initialize an external interrupt on a gpio pin
 *
 * @param pin       Pin number to enable gpio.
 * @param handler   Interrupt handler
 * @param arg       Argument to pass to interrupt handler
 * @param trig      Trigger mode of interrupt
 * @param pull      Push/pull mode of input.
 *
 * @return int
 */
int
hal_gpio_irq_init(int pin, hal_gpio_irq_handler_t handler, void *arg,
                  hal_gpio_irq_trig_t trig, hal_gpio_pull_t pull)
{
    int rc;
    int irqn;
    int index;
    uint32_t pin_mask;
    uint32_t mode;
    GPIO_InitTypeDef cfg;

    /* Configure the gpio for an external interrupt */
    rc = 0;
    switch (trig) {
    case HAL_GPIO_TRIG_NONE:
        rc = -1;
        break;
    case HAL_GPIO_TRIG_RISING:
        mode = GPIO_MODE_IT_RISING;
        break;
    case HAL_GPIO_TRIG_FALLING:
        mode = GPIO_MODE_IT_FALLING;
        break;
    case HAL_GPIO_TRIG_BOTH:
        mode = GPIO_MODE_IT_RISING_FALLING;
        break;
    case HAL_GPIO_TRIG_LOW:
        rc = -1;
        break;
    case HAL_GPIO_TRIG_HIGH:
        rc = -1;
        break;
    default:
        rc = -1;
        break;
    }

    /* Check to make sure no error has occurred */
    if (!rc) {
        /* Disable interrupt and clear any pending */
        hal_gpio_irq_disable(pin);
        pin_mask = HAL_GPIO_PIN(pin);
        __HAL_GPIO_EXTI_CLEAR_FLAG(pin_mask);

        /* Set the gpio irq handler */
        index = HAL_GPIO_PIN_NUM(pin);
        hal_gpio_irq[index].isr = handler;
        hal_gpio_irq[index].arg = arg;
        hal_gpio_irq[index].invoked = 0;

        /* Configure the GPIO */
        cfg.Mode = mode;
        cfg.Pull = pull;
        rc = hal_gpio_init_stm(pin, &cfg);
        if (!rc) {
            /* Enable interrupt vector in NVIC */
            irqn = hal_gpio_pin_to_irq(pin);
            hal_gpio_set_nvic(irqn);
        }
    }

    return rc;
}

/**
 * gpio irq release
 *
 * No longer interrupt when something occurs on the pin. NOTE: this function
 * does not change the GPIO push/pull setting nor does it change the
 * SYSCFG EXTICR registers. It also does not disable the NVIC interrupt enable
 * setting for the irq.
 *
 * @param pin
 */
void
hal_gpio_irq_release(int pin)
{
    int index;
    uint32_t pin_mask;

    /* Disable the interrupt */
    hal_gpio_irq_disable(pin);

    /* Clear any pending interrupts */
    pin_mask = HAL_GPIO_PIN(pin);
    __HAL_GPIO_EXTI_CLEAR_FLAG(pin_mask);

    /* Clear out the irq handler */
    index = HAL_GPIO_PIN_NUM(pin);
    hal_gpio_irq[index].arg = NULL;
    hal_gpio_irq[index].isr = NULL;
}

/**
 * gpio irq enable
 *
 * Enable the irq on the specified pin
 *
 * @param pin
 */
void
hal_gpio_irq_enable(int pin)
{
    uint32_t ctx;
    uint32_t mask;

    mask = HAL_GPIO_PIN(pin);

    __HAL_DISABLE_INTERRUPTS(ctx);
    EXTI->IMR |= mask;
    __HAL_ENABLE_INTERRUPTS(ctx);
}

/**
 * gpio irq disable
 *
 *
 * @param pin
 */
void
hal_gpio_irq_disable(int pin)
{
    uint32_t ctx;
    uint32_t mask;

    mask = HAL_GPIO_PIN(pin);
    __HAL_DISABLE_INTERRUPTS(ctx);
    EXTI->IMR &= ~mask;
    __HAL_ENABLE_INTERRUPTS(ctx);
}
