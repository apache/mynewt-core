/**
 * Copyright (c) 2015 Stack Inc.
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
#include "nrf52xxx/nrf52.h"
#include "nrf52xxx/nrf52_bitfields.h"
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
 */ 
#define GPIO_INDEX(pin)     (pin)
#define GPIO_MASK(pin)      (1 << GPIO_INDEX(pin))

/* Storage for GPIO callbacks. */
struct gpio_irq_obj
{
    void *arg;
    gpio_irq_handler_t isr;
};

#if 0
static struct gpio_irq_obj gpio_irq_handlers[16];
#endif

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

#if 0
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
        case 0:
            __HAL_RCC_GPIOA_CLK_ENABLE();
            break;
        case 1:
            __HAL_RCC_GPIOB_CLK_ENABLE();
            break;
        case 2:
            __HAL_RCC_GPIOC_CLK_ENABLE();
            break;
        case 3:
            __HAL_RCC_GPIOD_CLK_ENABLE();
            break;
        case 4:
            __HAL_RCC_GPIOE_CLK_ENABLE();
            break;
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
        irqn = GPIOTE_IRQn + index;
    } else if (index <=  9) {
        irqn = EXTI9_5_IRQn;
    } else {
        irqn = EXTI15_10_IRQn;
    }

    return irqn;
}
#endif

static void
hal_gpio_set_nvic(IRQn_Type irqn)
{
#if 0
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
#endif
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
gpio_init_in(int pin, gpio_pull_t pull)
{
    uint32_t conf;

    switch (pull) {
    case GPIO_PULL_UP:
        conf = GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos;
        break;
    case GPIO_PULL_DOWN:
        conf = GPIO_PIN_CNF_PULL_Pulldown << GPIO_PIN_CNF_PULL_Pos;
        break;
    case GPIO_PULL_NONE:
    default:
        conf = 0;
        break;
    }

    NRF_P0->PIN_CNF[pin] = conf;
    NRF_P0->DIRCLR = GPIO_MASK(pin);

    return 0;
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
    if (val) {
        NRF_P0->OUTSET = GPIO_MASK(pin);
    } else {
        NRF_P0->OUTCLR = GPIO_MASK(pin);
    }
    NRF_P0->PIN_CNF[pin] = GPIO_PIN_CNF_DIR_Output;
    NRF_P0->DIRSET = GPIO_MASK(pin);
    return 0;
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
    NRF_P0->OUTSET = GPIO_MASK(pin);
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
    NRF_P0->OUTCLR = GPIO_MASK(pin);
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
    if (val) {
        gpio_set(pin);
    } else {
        gpio_clear(pin);
    }
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
    return (NRF_P0->IN & GPIO_MASK(pin));
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
    if (gpio_read(pin)) {
        gpio_clear(pin);
    } else {
        gpio_set(pin);
    }
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
              gpio_irq_trig_t trig, gpio_pull_t pull)
{
#if 0
    int rc;
    int irqn;
    int index;
    uint32_t pin_mask;
    uint32_t mode;
    GPIO_InitTypeDef init_cfg;

    /* Configure the gpio for an external interrupt */
    rc = 0;
    switch (trig) {
    case GPIO_TRIG_NONE:
        rc = -1;
        break;
    case GPIO_TRIG_RISING:
        mode = GPIO_MODE_IT_RISING;
        break;
    case GPIO_TRIG_FALLING:
        mode = GPIO_MODE_IT_FALLING;
        break;
    case GPIO_TRIG_BOTH:
        mode = GPIO_MODE_IT_RISING_FALLING;
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

    /* Check to make sure no error has occurred */
    if (!rc) {
        /* Disable interrupt and clear any pending */
        gpio_irq_disable(pin);
        pin_mask = GPIO_MASK(pin);
        __HAL_GPIO_EXTI_CLEAR_FLAG(pin_mask);

        /* Set the gpio irq handler */
        index = GPIO_INDEX(pin);
        gpio_irq_handlers[index].isr = handler;
        gpio_irq_handlers[index].arg = arg;

        /* Configure the GPIO */
        init_cfg.Mode = mode;
        init_cfg.Pull = pull;
        rc = hal_gpio_init(pin, &init_cfg);
        if (!rc) {
            /* Enable interrupt vector in NVIC */
            irqn = hal_gpio_pin_to_irq(pin);
            hal_gpio_set_nvic(irqn);
        }
    }

    return rc;
#else
    hal_gpio_set_nvic(0);
    return 0;
#endif
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
#if 0
    int index;
    uint32_t pin_mask;

    /* Disable the interrupt */
    gpio_irq_disable(pin);

    /* Clear any pending interrupts */
    pin_mask = GPIO_MASK(pin);
    __HAL_GPIO_EXTI_CLEAR_FLAG(pin_mask);

    /* Clear out the irq handler */
    index = GPIO_INDEX(pin);
    gpio_irq_handlers[index].arg = NULL;
    gpio_irq_handlers[index].isr = NULL;
#else
    return;
#endif
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
#if 0
    uint32_t ctx;
    uint32_t mask;

    mask = GPIO_MASK(pin);

    __HAL_DISABLE_INTERRUPTS(ctx);
    EXTI->IMR |= mask;
    __HAL_ENABLE_INTERRUPTS(ctx);
#else
    return;
#endif
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
#if 0
    uint32_t ctx;
    uint32_t mask;

    mask = GPIO_MASK(pin);
    __HAL_DISABLE_INTERRUPTS(ctx);
    EXTI->IMR &= ~mask;
    __HAL_ENABLE_INTERRUPTS(ctx);
#else
    return;
#endif
}
