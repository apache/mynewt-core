/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "hal/hal_gpio.h"
#include "bsp/cmsis_nvic.h"
#include "mcu/stm32f30x.h"
#include "mcu/stm32f30x_exti.h"
#include "mcu/stm32f30x_gpio.h"
#include "mcu/stm32f30x_rcc.h"
#include "mcu/stm32f30x_syscfg.h"
#include <stdlib.h>
#include <assert.h>

 /* XXX: Notes
 * 1) Right now, we are not disabling the NVIC interrupt source; we only
 * disable the external interrupt from occurring. Dont think either way
 * to do it is an issue... when we release we may want to disable the NVIC
 *
 * 2) investigate how thread safe these routines are. _GPIO_Init, for
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
 * The stm32F4xx processors have 16 gpio pins per port. We map the logical pin
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
 *      Ex: PD11 = (4 * 16) + 11 = 75.
 *          PA0 = (0 * 16) + 0 = 0
 */
#define GPIO_INDEX(pin)     ((pin) & 0x0F)
#define GPIO_PORT(pin)      (((pin) >> 4) & 0x0F)
#define GPIO_MASK(pin)      (1 << GPIO_INDEX(pin))

/* Define the number of ports on processor */
#if defined GPIOK_BASE
#define HAL_GPIO_NUM_PORTS  (11)
#elif defined GPIOJ_BASE
#define HAL_GPIO_NUM_PORTS  (10)
#elif defined GPIOI_BASE
#define HAL_GPIO_NUM_PORTS  (9)
#elif defined GPIOH_BASE
#define HAL_GPIO_NUM_PORTS  (8)
#elif defined GPIOG_BASE
#define HAL_GPIO_NUM_PORTS  (7)
#elif defined GPIOF_BASE
#define HAL_GPIO_NUM_PORTS  (6)
#else
#define HAL_GPIO_NUM_PORTS  (5)
#endif

/* Port index to port address map */
static GPIO_TypeDef * const portmap[HAL_GPIO_NUM_PORTS] =
{
    GPIOA,
    GPIOB,
    GPIOC,
    GPIOD,
    GPIOE,
#if defined GPIOF_BASE
    GPIOF,
#endif
#if defined GPIOG_BASE
    GPIOG,
#endif
#if defined GPIOH_BASE
    GPIOH,
#endif
#if defined GPIOI_BASE
    GPIOI
#endif
#if defined GPIOJ_BASE
    GPIOJ
#endif
#if defined GPIOK_BASE
    GPIOK
#endif
};

/* Storage for GPIO callbacks. */
struct gpio_irq_obj {
    void *arg;
    gpio_irq_handler_t isr;
};

static struct gpio_irq_obj gpio_irq_handlers[16];

struct ext_irqs {
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
    if (EXTI_GetFlagStatus(index) != RESET) {
        EXTI_ClearFlag(index);
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
        case 0:
            RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE);
            break;
        case 1:
            RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOB, ENABLE);
            break;
        case 2:
            RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOC, ENABLE);
            break;
        case 3:
            RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOD, ENABLE);
            break;
        case 4:
            RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOE, ENABLE);
            break;
#if defined GPIOF_BASE
        case 5:
            RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOF, ENABLE);
            break;
#endif
#if defined GPIOG_BASE
        case 6:
            RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOG, ENABLE);
            break;
#endif
#if defined GPIOH_BASE
        case 7:
            RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOH, ENABLE);
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
    case EXTI2_TS_IRQn:
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
static int
hal_gpio_init(int pin, GPIO_InitTypeDef *cfg)
{
    int port;
    uint32_t mcu_pin_mask;

    /* Is this a valid pin? */
    port = GPIO_PORT(pin);
    if (port >= HAL_GPIO_NUM_PORTS) {
        return -1;
    }

    mcu_pin_mask = GPIO_MASK(pin);
    cfg->GPIO_Pin = mcu_pin_mask;

    /* Enable the GPIO clockl */
    hal_gpio_clk_enable(port);

    /* Initialize pin as an input, setting proper mode */
    GPIO_Init(portmap[port], cfg);

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
gpio_init_in(int pin, enum gpio_pull pull)
{
    int rc;
    GPIO_InitTypeDef init_cfg;

    init_cfg.GPIO_Mode = GPIO_Mode_IN;
    init_cfg.GPIO_Speed = GPIO_Speed_Level_2;
    init_cfg.GPIO_OType = GPIO_OType_PP;
    switch (pull) {
    case GPIO_PULL_NONE:
        init_cfg.GPIO_PuPd = GPIO_PuPd_NOPULL;
        break;
    case GPIO_PULL_UP:
        init_cfg.GPIO_PuPd = GPIO_PuPd_UP;
        break;
    case GPIO_PULL_DOWN:
        init_cfg.GPIO_PuPd = GPIO_PuPd_DOWN;
        break;
    }

    rc = hal_gpio_init(pin, &init_cfg);
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
int gpio_init_out(int pin, int val)
{
    int rc;
    GPIO_InitTypeDef init_cfg;

    init_cfg.GPIO_Mode = GPIO_Mode_OUT;
    init_cfg.GPIO_Speed = GPIO_Speed_Level_2;
    init_cfg.GPIO_OType = GPIO_OType_PP;
    init_cfg.GPIO_PuPd = GPIO_PuPd_NOPULL;

    rc = hal_gpio_init(pin, &init_cfg);
    return rc;
}

/**
 * gpio set
 *
 * Sets specified pin to 1 (high)
 *
 * @param pin
 */
void gpio_set(int pin)
{
    gpio_write(pin, 1);
}

/**
 * gpio clear
 *
 * Sets specified pin to 0 (low).
 *
 * @param pin
 */
void gpio_clear(int pin)
{
    gpio_write(pin, 0);
}

/**
 * gpio write
 *
 * Write a value (either high or low) to the specified pin.
 *
 * @param pin Pin to set
 * @param val Value to set pin (0:low 1:high)
 */
void gpio_write(int pin, int val)
{
    int port;
    uint32_t mcu_pin_mask;

    port = GPIO_PORT(pin);
    mcu_pin_mask = GPIO_MASK(pin);
    GPIO_WriteBit(portmap[port], mcu_pin_mask, val);
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
int gpio_read(int pin)
{
    int port;
    uint32_t mcu_pin_mask;

    port = GPIO_PORT(pin);
    mcu_pin_mask = GPIO_MASK(pin);
    return GPIO_ReadInputDataBit(portmap[port], mcu_pin_mask);
}

/**
 * gpio toggle
 *
 * Toggles the specified pin
 *
 * @param pin Pin number to toggle
 */
void gpio_toggle(int pin)
{
    gpio_write(pin, !gpio_read(pin));
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
gpio_irq_init(int pin, gpio_irq_handler_t handler, void *arg,
              gpio_irq_trig_t trig, enum gpio_pull pull)
{
    int rc;
    int irqn;
    int index;
    uint32_t pin_mask;
    EXTI_InitTypeDef exti_cfg;

    /* Configure the gpio for an external interrupt */
    rc = 0;

    pin_mask = GPIO_MASK(pin);
    index = GPIO_INDEX(pin);

    exti_cfg.EXTI_Line = index;
    exti_cfg.EXTI_Mode = EXTI_Mode_Interrupt;
    switch (trig) {
    case GPIO_TRIG_NONE:
        rc = -1;
        break;
    case GPIO_TRIG_RISING:
        exti_cfg.EXTI_Trigger = EXTI_Trigger_Rising;
        break;
    case GPIO_TRIG_FALLING:
        exti_cfg.EXTI_Trigger = EXTI_Trigger_Falling;
        break;
    case GPIO_TRIG_BOTH:
        exti_cfg.EXTI_Trigger = EXTI_Trigger_Rising_Falling;
        break;
    case GPIO_TRIG_LOW:
        rc = -1;
        break;
    case GPIO_TRIG_HIGH:
        rc = -1;
        break;
    default:
        rc = -1;
        break;
    }
    exti_cfg.EXTI_LineCmd = ENABLE;

    /* Check to make sure no error has occurred */
    if (!rc) {
        /* Disable interrupt and clear any pending */
        gpio_irq_disable(pin);
        EXTI_ClearFlag(pin_mask);

        gpio_init_in(pin, pull);

        /* Configure SYSCFG */
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);

        /* Configure EXTI */
        SYSCFG_EXTILineConfig(GPIO_PORT(pin), index);

        EXTI_Init(&exti_cfg);

        /* Set the gpio irq handler */
        gpio_irq_handlers[index].isr = handler;
        gpio_irq_handlers[index].arg = arg;

        /* Configure the GPIO */
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
gpio_irq_release(int pin)
{
    int index;
    uint32_t pin_mask;

    /* Disable the interrupt */
    gpio_irq_disable(pin);

    /* Clear any pending interrupts */
    pin_mask = GPIO_MASK(pin);
    EXTI_ClearFlag(pin_mask);

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
gpio_irq_enable(int pin)
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
gpio_irq_disable(int pin)
{
    uint32_t ctx;
    uint32_t mask;

    mask = GPIO_MASK(pin);
    __HAL_DISABLE_INTERRUPTS(ctx);
    EXTI->IMR &= ~mask;
    __HAL_ENABLE_INTERRUPTS(ctx);
}
