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

#include "hal/hal_gpio.h"
#include "mcu/cmsis_nvic.h"
#include "mcu/stm32_hal.h"
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
 * 5) Possibly add access to HAL_GPIO_DeInit.
 */

/*
 * GPIO pin mapping
 *
 * The stm32Fxxx processors have 16 gpio pins per port. We map the logical pin
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
#define GPIO_INDEX(pin)     ((pin) & 0x0F)
#define GPIO_PORT(pin)      (((pin) >> 4) & 0x0F)
#define GPIO_MASK(pin)      (1 << GPIO_INDEX(pin))

#ifndef GPIOA
#   define GPIOA    0
#endif
#ifndef GPIOB
#   define GPIOB    0
#endif
#ifndef GPIOC
#   define GPIOC    0
#endif
#ifndef GPIOD
#   define GPIOD    0
#endif
#ifndef GPIOE
#   define GPIOE    0
#endif
#ifndef GPIOF
#   define GPIOF    0
#endif
#ifndef GPIOG
#   define GPIOG    0
#endif
#ifndef GPIOH
#   define GPIOH    0
#endif
#ifndef GPIOI
#   define GPIOI    0
#endif
#ifndef GPIOJ
#   define GPIOJ    0
#endif
#ifndef GPIOK
#   define GPIOK    0
#endif


#if defined GPIOK_BASE
#   define  HAL_GPIO_PORT_COUNT   (11)
#   define  HAL_GPIO_PORT_LIST    GPIOA,GPIOB,GPIOC,GPIOD,GPIOE,GPIOF,GPIOG,GPIOH,GPIOI,GPIOJ,GPIOK
#elif defined GPIOJ_BASE
#   define  HAL_GPIO_PORT_COUNT   (10)
#   define  HAL_GPIO_PORT_LIST    GPIOA,GPIOB,GPIOC,GPIOD,GPIOE,GPIOF,GPIOG,GPIOH,GPIOI,GPIOJ
#elif defined GPIOI_BASE
#   define  HAL_GPIO_PORT_COUNT   (9)
#   define  HAL_GPIO_PORT_LIST    GPIOA,GPIOB,GPIOC,GPIOD,GPIOE,GPIOF,GPIOG,GPIOH,GPIOI
#elif defined GPIOH_BASE
#   define  HAL_GPIO_PORT_COUNT   (8)
#   define  HAL_GPIO_PORT_LIST    GPIOA,GPIOB,GPIOC,GPIOD,GPIOE,GPIOF,GPIOG,GPIOH
#elif defined GPIOG_BASE
#   define  HAL_GPIO_PORT_COUNT   (7)
#   define  HAL_GPIO_PORT_LIST    GPIOA,GPIOB,GPIOC,GPIOD,GPIOE,GPIOF,GPIOG
#elif defined GPIOF_BASE
#   define  HAL_GPIO_PORT_COUNT   (6)
#   define  HAL_GPIO_PORT_LIST    GPIOA,GPIOB,GPIOC,GPIOD,GPIOE,GPIOF
#elif defined GPIOE_BASE
#   define  HAL_GPIO_PORT_COUNT   (5)
#   define  HAL_GPIO_PORT_LIST    GPIOA,GPIOB,GPIOC,GPIOD,GPIOE
#elif defined GPIOD_BASE
#   define  HAL_GPIO_PORT_COUNT   (4)
#   define  HAL_GPIO_PORT_LIST    GPIOA,GPIOB,GPIOC,GPIOD
#elif defined GPIOC_BASE
#   define  HAL_GPIO_PORT_COUNT   (3)
#   define  HAL_GPIO_PORT_LIST    GPIOA,GPIOB,GPIOC
#elif defined GPIOB_BASE
#   define  HAL_GPIO_PORT_COUNT   (2)
#   define  HAL_GPIO_PORT_LIST    GPIOA,GPIOB
#elif defined GPIOA_BASE
#   define  HAL_GPIO_PORT_COUNT   (1)
#   define  HAL_GPIO_PORT_LIST    GPIOA
#else
#   error "No GPIO ports found - MCU not supported!"
#endif


/* Port index to port address map */
static GPIO_TypeDef * const portmap[HAL_GPIO_PORT_COUNT] =
{
    HAL_GPIO_PORT_LIST
};

/* Storage for GPIO callbacks. */
struct gpio_irq_obj
{
    void *arg;
    hal_gpio_irq_handler_t isr;
};

static struct gpio_irq_obj gpio_irq_handlers[16];

struct ext_irqs
{
    volatile uint32_t irq0;
    volatile uint32_t irq1;
    volatile uint32_t irq2;
    volatile uint32_t irq3;
    volatile uint32_t irq4;
    volatile uint32_t irq9_5;
    volatile uint32_t irq15_10;
};
struct ext_irqs ext_irq_counts;

/**
 * ext irq handler
 *
 * Handles the gpio interrupt attached to a gpio pin.
 *
 * @param index
 */
static void
ext_irq_handler(int index)
{
    uint32_t mask;

    mask = 1 << index;
    if (__HAL_GPIO_EXTI_GET_IT(mask) != RESET) {
        __HAL_GPIO_EXTI_CLEAR_IT(mask);
        gpio_irq_handlers[index].isr(gpio_irq_handlers[index].arg);
    }
}

/* External interrupt 0 */
static void
ext_irq0(void)
{
    ++ext_irq_counts.irq0;
    ext_irq_handler(0);
}

/* External interrupt 1 */
static void
ext_irq1(void)
{
    ++ext_irq_counts.irq1;
    ext_irq_handler(1);
}

/* External interrupt 2 */
static void
ext_irq2(void)
{
    ++ext_irq_counts.irq2;
    ext_irq_handler(2);
}

/* External interrupt 3 */
static void
ext_irq3(void)
{
    ++ext_irq_counts.irq3;
    ext_irq_handler(3);
}

/**
 * ext irq4
 *
 *  External interrupt handler for external interrupt 4.
 *
 */
static void
ext_irq4(void)
{
    ++ext_irq_counts.irq4;
    ext_irq_handler(4);
}

/**
 * ext irq9_5
 *
 *  External interrupt handler for irqs 9 through 5.
 *
 */
static void
ext_irq9_5(void)
{
    int index;

    ++ext_irq_counts.irq9_5;
    for (index = 5; index <= 9; ++index) {
        ext_irq_handler(index);
    }
}

/**
 * ext irq15_10
 *
 *  External interrupt handler for irqs 15 through 10.
 *
 */
static void
ext_irq15_10(void)
{
    int index;

    ++ext_irq_counts.irq15_10;
    for (index = 10; index <= 15; ++index) {
        ext_irq_handler(index);
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
#if defined GPIOA_BASE
        case 0:
            __HAL_RCC_GPIOA_CLK_ENABLE();
            break;
#endif
#if defined GPIOB_BASE
        case 1:
            __HAL_RCC_GPIOB_CLK_ENABLE();
            break;
#endif
#if defined GPIOC_BASE
        case 2:
            __HAL_RCC_GPIOC_CLK_ENABLE();
            break;
#endif
#if defined GPIOD_BASE
        case 3:
            __HAL_RCC_GPIOD_CLK_ENABLE();
            break;
#endif
#if defined GPIOE_BASE
        case 4:
            __HAL_RCC_GPIOE_CLK_ENABLE();
            break;
#endif
#if defined GPIOF_BASE
        case 5:
            __HAL_RCC_GPIOF_CLK_ENABLE();
            break;
#endif
#if defined GPIOG_BASE
        case 6:
            __HAL_RCC_GPIOG_CLK_ENABLE();
            break;
#endif
#if defined GPIOH_BASE
        case 7:
            __HAL_RCC_GPIOH_CLK_ENABLE();
            break;
#endif
#if defined GPIOI_BASE
        case 8:
            __HAL_RCC_GPIOI_CLK_ENABLE();
            break;
#endif
#if defined GPIOJ_BASE
        case 9:
            __HAL_RCC_GPIOJ_CLK_ENABLE();
            break;
#endif
#if defined GPIOK_BASE
        case 10:
            __HAL_RCC_GPIOK_CLK_ENABLE();
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

    index = GPIO_INDEX(pin);
    if (index <= 4) {
        irqn = EXTI0_IRQn + index;
    } else if (index <=  9) {
        irqn = EXTI9_5_IRQn;
    } else {
        irqn = EXTI15_10_IRQn;
    }

    return irqn;
}

#if MYNEWT_VAL(MCU_STM32F3)
#   define EXTI2_IRQn   EXTI2_TSC_IRQn
#endif

static void
hal_gpio_set_nvic(IRQn_Type irqn)
{
    uint32_t isr;

    switch (irqn) {
    case EXTI0_IRQn:
        isr = (uint32_t)&ext_irq0;
        break;
    case EXTI1_IRQn:
        isr = (uint32_t)&ext_irq1;
        break;
    case EXTI2_IRQn:
        isr = (uint32_t)&ext_irq2;
        break;
    case EXTI3_IRQn:
        isr = (uint32_t)&ext_irq3;
        break;
    case EXTI4_IRQn:
        isr = (uint32_t)&ext_irq4;
        break;
    case EXTI9_5_IRQn:
        isr = (uint32_t)&ext_irq9_5;
        break;
    case EXTI15_10_IRQn:
        isr = (uint32_t)&ext_irq15_10;
        break;
    default:
        assert(0);
        break;
    }

    /* Set isr in vector table if not yet set */
    if (NVIC_GetVector(irqn) != isr) {
        NVIC_SetVector(irqn, isr);
        NVIC_EnableIRQ(irqn);
    }
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
    int port;
    uint32_t mcu_pin_mask;

    /* Is this a valid pin? */
    port = GPIO_PORT(pin);
    if (port >= HAL_GPIO_PORT_COUNT) {
        return -1;
    }

    mcu_pin_mask = GPIO_MASK(pin);
    cfg->Pin = mcu_pin_mask;

    /* Enable the GPIO clock */
    hal_gpio_clk_enable(port);

    /* Initialize pin as an input, setting proper mode */
    HAL_GPIO_Init(portmap[port], cfg);

    return 0;
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
    uint32_t mcu_pin_mask;

    /* Is this a valid pin? */
    port = GPIO_PORT(pin);
    if (port >= HAL_GPIO_PORT_COUNT) {
        return -1;
    }

    mcu_pin_mask = GPIO_MASK(pin);
    cfg->Pin = mcu_pin_mask;

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
    int port;

    /* Is this a valid pin? */
    port = GPIO_PORT(pin);
    if (port >= HAL_GPIO_PORT_COUNT) {
        return -1;
    }

    /* Enable the GPIO clock */
    hal_gpio_clk_enable(port);

    /* Write initial output value */
    hal_gpio_write(pin, val);

    cfg.Pin = GPIO_MASK(pin);
    cfg.Mode = GPIO_MODE_OUTPUT_PP;
    cfg.Pull = GPIO_NOPULL;
#if defined(GPIO_SPEED_FREQ_VERY_HIGH)
    cfg.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
#elif defined(GPIO_SPEED_HIGH)
    cfg.Speed = GPIO_SPEED_HIGH;
#else
    cfg.Speed = GPIO_SPEED_FREQ_HIGH;
#endif
#if !MYNEWT_VAL(MCU_STM32F1)
    cfg.Alternate = 0;
#endif

    /* Initialize pin as an output, setting proper mode */
    HAL_GPIO_Init(portmap[port], &cfg);

    return 0;
}

/**
 * gpio init af
 *
 * Configure the specified pin for AF.
 */
int
hal_gpio_init_af(int pin, uint8_t af_type, enum hal_gpio_pull pull, uint8_t od)
{
    GPIO_InitTypeDef gpio;

    if (!od) {
        gpio.Mode = GPIO_MODE_AF_PP;
    } else {
        gpio.Mode = GPIO_MODE_AF_OD;
    }
    gpio.Pull = pull;
#if defined(GPIO_SPEED_FREQ_VERY_HIGH)
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
#elif defined(GPIO_SPEED_HIGH)
    gpio.Speed = GPIO_SPEED_HIGH;
#else
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
#endif
#if !MYNEWT_VAL(MCU_STM32F1)
    gpio.Alternate = af_type;
#endif

    return hal_gpio_init_stm(pin, &gpio);
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
    uint32_t mcu_pin_mask;
    GPIO_PinState state;

    port = GPIO_PORT(pin);
    mcu_pin_mask = GPIO_MASK(pin);

    if (val) {
        state = GPIO_PIN_SET;
    } else {
        state = GPIO_PIN_RESET;
    }

    HAL_GPIO_WritePin(portmap[port], mcu_pin_mask, state);
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
    uint32_t mcu_pin_mask;

    port = GPIO_PORT(pin);
    mcu_pin_mask = GPIO_MASK(pin);
    return HAL_GPIO_ReadPin(portmap[port], mcu_pin_mask);
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
    GPIO_InitTypeDef init_cfg;

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
        pin_mask = GPIO_MASK(pin);
        __HAL_GPIO_EXTI_CLEAR_FLAG(pin_mask);

        /* Set the gpio irq handler */
        index = GPIO_INDEX(pin);
        gpio_irq_handlers[index].isr = handler;
        gpio_irq_handlers[index].arg = arg;

        /* Configure the GPIO */
        init_cfg.Mode = mode;
        init_cfg.Pull = pull;
        rc = hal_gpio_init_stm(pin, &init_cfg);
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
    pin_mask = GPIO_MASK(pin);
    __HAL_GPIO_EXTI_CLEAR_FLAG(pin_mask);

    /* Clear out the irq handler */
    index = GPIO_INDEX(pin);
    gpio_irq_handlers[index].arg = NULL;
    gpio_irq_handlers[index].isr = NULL;
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

    mask = GPIO_MASK(pin);

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

    mask = GPIO_MASK(pin);
    __HAL_DISABLE_INTERRUPTS(ctx);
    EXTI->IMR &= ~mask;
    __HAL_ENABLE_INTERRUPTS(ctx);
}
