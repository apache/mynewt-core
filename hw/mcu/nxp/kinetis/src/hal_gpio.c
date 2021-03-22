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
#include <stdlib.h>
#include <assert.h>

#include "fsl_common.h"
#include "fsl_clock.h"
#include "fsl_gpio.h"
#include "fsl_port.h"

struct hal_gpio_irq {
    hal_gpio_irq_handler_t func;
    void *arg;
    int pin;
#if (defined(FSL_FEATURE_PORT_HAS_NO_INTERRUPT) && FSL_FEATURE_PORT_HAS_NO_INTERRUPT)
    gpio_interrupt_config_t sense_trig;
#else
    port_interrupt_t sense_trig;
#endif
};

/* Each GPIO port has pins from 0 to 31 */
#define GPIO_INDEX(pin)     ((pin) & 0x1F)
#define GPIO_PORT(pin)      (((pin) >> 5) & 0x07)
#define GPIO_MASK(pin)      (1 << GPIO_INDEX(pin))
#define GPIO_PIN(port, pin)  ((((port) & 0x07) << 5) | ((pin) & 0x1F))
#define HAL_GPIO_MAX_IRQ MYNEWT_VAL(GPIO_MAX_IRQ)

static GPIO_Type *const s_gpioBases[] = GPIO_BASE_PTRS;
static PORT_Type *const s_portBases[] = PORT_BASE_PTRS;
static clock_ip_name_t const s_portClocks[] = PORT_CLOCKS;
static IRQn_Type const port_irqs[] = PORT_IRQS;
static struct hal_gpio_irq hal_gpio_irqs[HAL_GPIO_MAX_IRQ] = {0};

uint16_t
hal_to_fsl_pull(hal_gpio_pull_t pull)
{
    switch ((int)pull) {
    case HAL_GPIO_PULL_UP:
        return kPORT_PullUp;
    case HAL_GPIO_PULL_DOWN:
        return kPORT_PullDown;
    default:
        return kPORT_PullDisable;
    }
}

int
hal_gpio_init_in(int pin, hal_gpio_pull_t pull)
{
    gpio_pin_config_t gconfig;
    port_pin_config_t pconfig;

    gconfig.pinDirection = kGPIO_DigitalInput;
    pconfig.pullSelect = hal_to_fsl_pull(pull);
    pconfig.mux = kPORT_MuxAsGpio;

    CLOCK_EnableClock(s_portClocks[GPIO_PORT(pin)]);
    PORT_SetPinConfig(s_portBases[GPIO_PORT(pin)], GPIO_INDEX(pin), &pconfig);
    GPIO_PinInit(s_gpioBases[GPIO_PORT(pin)], GPIO_INDEX(pin), &gconfig);

    return 0;
}

int
hal_gpio_init_out(int pin, int val)
{
    gpio_pin_config_t gconfig;

    gconfig.pinDirection = kGPIO_DigitalOutput;
    gconfig.outputLogic = (uint8_t) val;

    CLOCK_EnableClock(s_portClocks[GPIO_PORT(pin)]);
    GPIO_PinInit(s_gpioBases[GPIO_PORT(pin)], GPIO_INDEX(pin), &gconfig);
    PORT_SetPinMux(s_portBases[GPIO_PORT(pin)], GPIO_INDEX(pin), kPORT_MuxAsGpio);

    return 0;
}

void
hal_gpio_write(int pin, int val)
{
    GPIO_PinWrite(s_gpioBases[GPIO_PORT(pin)], GPIO_INDEX(pin), val);
}

int
hal_gpio_read(int pin)
{
    return (int)GPIO_PinRead(s_gpioBases[GPIO_PORT(pin)], GPIO_INDEX(pin));
}

int
hal_gpio_toggle(int pin)
{
    GPIO_PortToggle(s_gpioBases[GPIO_PORT(pin)], 1 << GPIO_INDEX(pin));

    return 0;
}

/*
 * Find out whether we have a GPIO event slot available.
 */
static int
hal_gpio_find_empty_slot(void)
{
    int i;
    for (i = 0; i < HAL_GPIO_MAX_IRQ; i++) {
        if (hal_gpio_irqs[i].func == NULL) {
            return i;
        }
    }
    return -1;
}

/*
 * Find a GPIO pin on the irq list.
 */
static int
hal_gpio_find_pin(int pin)
{
    int i;
    for (i = 0; i < HAL_GPIO_MAX_IRQ; i++) {
        if (hal_gpio_irqs[i].pin == pin) {
            return i;
        }
    }
    return -1;
}

/*
 * Find an entry which uses a pin belonging to port of given index.
 */
static int
hal_gpio_find_port(int port_idx)
{
    int i;
    for (i = 0; i < HAL_GPIO_MAX_IRQ; i++) {
        if (GPIO_PORT(hal_gpio_irqs[i].pin) == port_idx) {
            return i;
        }
    }
    return -1;
}

/*
 * GPIO irq handler
 *
 * Handles the gpio interrupt attached to a gpio pin.
 */
static void
hal_gpio_irq_handler(void)
{
    int i;
    int pin;
#if !(defined(FSL_FEATURE_PORT_HAS_NO_INTERRUPT) && FSL_FEATURE_PORT_HAS_NO_INTERRUPT)
    int mask;
#endif
    for (i = 0; i < HAL_GPIO_MAX_IRQ; i++) {
        pin = hal_gpio_irqs[i].pin;
#if (defined(FSL_FEATURE_PORT_HAS_NO_INTERRUPT) && FSL_FEATURE_PORT_HAS_NO_INTERRUPT)
        if (GPIO_PinGetInterruptFlag(s_portBases[GPIO_PORT(pin)],
                                     GPIO_INDEX(pin))) {
            GPIO_GpioClearInterruptFlags(s_portBases[GPIO_PORT(pin)],
                                         GPIO_MASK(pin));
            hal_gpio_irqs[i].func(hal_gpio_irqs[i].arg);
            return;
        }
#else
        mask = GPIO_MASK(pin);
        if (PORT_GetPinsInterruptFlags(s_portBases[GPIO_PORT(pin)]) &&
            mask) {
            GPIO_PortClearInterruptFlags(s_gpioBases[GPIO_PORT(pin)],
                                         GPIO_MASK(pin));
            hal_gpio_irqs[i].func(hal_gpio_irqs[i].arg);
        }
#endif
    }
#if (defined(FSL_FEATURE_PORT_HAS_NO_INTERRUPT) && FSL_FEATURE_PORT_HAS_NO_INTERRUPT)
    /* Clear external interrupt flag. */
    GPIO_GpioClearInterruptFlags(s_portBases[GPIO_PORT(pin)], GPIO_MASK(pin));
#else
    /* Clear external interrupt flag. */
    GPIO_PortClearInterruptFlags(s_gpioBases[GPIO_PORT(pin)], GPIO_MASK(pin));
#endif
}

/**
 * Initialize a given pin to trigger a GPIO IRQ callback.
 *
 * @param pin     The pin to trigger GPIO interrupt on
 * @param handler The handler function to call
 * @param arg     The argument to provide to the IRQ handler
 * @param trig    The trigger mode (e.g. rising, falling)
 * @param pull    The mode of the pin (e.g. pullup, pulldown)
 *
 * @return 0 on success, non-zero error code on failure.
 */
int
hal_gpio_irq_init(int pin,
                  hal_gpio_irq_handler_t handler,
                  void *arg,
                  hal_gpio_irq_trig_t trig,
                  hal_gpio_pull_t pull)
{
    int i = hal_gpio_find_empty_slot();
    if (i < 0) {
        return -1;
    }
    hal_gpio_irqs[i].pin = pin;
    hal_gpio_irqs[i].func = handler;
    hal_gpio_irqs[i].arg = arg;

#if (defined(FSL_FEATURE_PORT_HAS_NO_INTERRUPT) && FSL_FEATURE_PORT_HAS_NO_INTERRUPT)
    switch (trig) {
    case HAL_GPIO_TRIG_RISING:
        hal_gpio_irqs[i].sense_trig = kGPIO_InterruptRisingEdge;
        break;
    case HAL_GPIO_TRIG_FALLING:
        hal_gpio_irqs[i].sense_trig = kGPIO_InterruptFallingEdge;
        break;
    case HAL_GPIO_TRIG_BOTH:
        hal_gpio_irqs[i].sense_trig = kGPIO_InterruptEitherEdge;
        break;
    case HAL_GPIO_TRIG_LOW:
        hal_gpio_irqs[i].sense_trig = kGPIO_InterruptLogicZero;
        break;
    case HAL_GPIO_TRIG_HIGH:
        hal_gpio_irqs[i].sense_trig = kGPIO_InterruptLogicOne;
        break;
    default:
        hal_gpio_irqs[i].sense_trig = kGPIO_InterruptStatusFlagDisabled;
        return -1;
    }
#else
    switch (trig) {
    case HAL_GPIO_TRIG_RISING:
        hal_gpio_irqs[i].sense_trig = kPORT_InterruptRisingEdge;
        break;
    case HAL_GPIO_TRIG_FALLING:
        hal_gpio_irqs[i].sense_trig = kPORT_InterruptFallingEdge;
        break;
    case HAL_GPIO_TRIG_BOTH:
        hal_gpio_irqs[i].sense_trig = kPORT_InterruptEitherEdge;
        break;
    case HAL_GPIO_TRIG_LOW:
        hal_gpio_irqs[i].sense_trig = kPORT_InterruptLogicZero;
        break;
    case HAL_GPIO_TRIG_HIGH:
        hal_gpio_irqs[i].sense_trig = kPORT_InterruptLogicOne;
        break;
    default:
        hal_gpio_irqs[i].sense_trig = kPORT_InterruptOrDMADisabled;
        return -1;
    }
#endif
    return hal_gpio_init_in(pin, pull);
}


/**
 * Release a pin from being configured to trigger IRQ on state change.
 *
 * @param pin The pin to release
 */
void
hal_gpio_irq_release(int pin)
{
    int entry = hal_gpio_find_pin(pin);
    if (entry > -1) {
#if (defined(FSL_FEATURE_PORT_HAS_NO_INTERRUPT) && FSL_FEATURE_PORT_HAS_NO_INTERRUPT)
        GPIO_SetMultipleInterruptPinsConfig(s_gpioBases[GPIO_PORT(pin)],
                                            GPIO_MASK(pin),
                                            kGPIO_InterruptStatusFlagDisabled);
#else
#if defined(FSL_FEATURE_PORT_HAS_MULTIPLE_IRQ_CONFIG) && FSL_FEATURE_PORT_HAS_MULTIPLE_IRQ_CONFIG
        PORT_SetMultipleInterruptPinsConfig(s_portBases[GPIO_PORT(pin)],
                                            GPIO_MASK(pin),
                                            kPORT_InterruptOrDMADisabled);
#else
        PORT_SetPinInterruptConfig(s_portBases[GPIO_PORT(pin)],
                                   GPIO_INDEX(pin),
                                   kPORT_InterruptOrDMADisabled);
#endif
#endif
        memset(&hal_gpio_irqs[entry], 0, sizeof(struct hal_gpio_irq));
        if (hal_gpio_find_port(GPIO_PORT(pin)) < 0) {
            NVIC_ClearPendingIRQ(GPIO_PORT(pin));
            NVIC_DisableIRQ(GPIO_PORT(pin));
        }
    }
}

/**
 * Enable IRQs on the passed pin
 *
 * @param pin The pin to enable IRQs on
 */
void
hal_gpio_irq_enable(int pin)
{
    int entry = hal_gpio_find_pin(pin);
    if (entry > -1) {
#if (defined(FSL_FEATURE_PORT_HAS_NO_INTERRUPT) && FSL_FEATURE_PORT_HAS_NO_INTERRUPT)
        GPIO_SetMultipleInterruptPinsConfig(s_gpioBases[GPIO_PORT(pin)],
                                            GPIO_MASK(pin),
                                            hal_gpio_irqs[entry].sense_trig);
#else
#if defined(FSL_FEATURE_PORT_HAS_MULTIPLE_IRQ_CONFIG) && FSL_FEATURE_PORT_HAS_MULTIPLE_IRQ_CONFIG
        PORT_SetMultipleInterruptPinsConfig(s_portBases[GPIO_PORT(pin)],
                                            GPIO_MASK(pin),
                                            hal_gpio_irqs[entry].sense_trig);
#else
        PORT_SetPinInterruptConfig(s_portBases[GPIO_PORT(pin)],
                                   GPIO_INDEX(pin),
                                   hal_gpio_irqs[entry].sense_trig);
#endif
#endif
        NVIC_SetVector(port_irqs[GPIO_PORT(pin)], (uint32_t) hal_gpio_irq_handler);
        NVIC_EnableIRQ(port_irqs[GPIO_PORT(pin)]);
    }
}

/**
 * Disable IRQs on the passed pin
 *
 * @param pin The pin to disable IRQs on
 */
void
hal_gpio_irq_disable(int pin)
{
    int entry = hal_gpio_find_pin(pin);
    if (entry > -1) {
#if (defined(FSL_FEATURE_PORT_HAS_NO_INTERRUPT) && FSL_FEATURE_PORT_HAS_NO_INTERRUPT)
        GPIO_SetMultipleInterruptPinsConfig(s_gpioBases[GPIO_PORT(pin)],
                                            GPIO_MASK(pin),
                                            kGPIO_InterruptStatusFlagDisabled);
#else
#if defined(FSL_FEATURE_PORT_HAS_MULTIPLE_IRQ_CONFIG) && FSL_FEATURE_PORT_HAS_MULTIPLE_IRQ_CONFIG
        PORT_SetMultipleInterruptPinsConfig(s_portBases[GPIO_PORT(pin)],
                                            GPIO_MASK(pin),
                                            kPORT_InterruptOrDMADisabled);
#else
        PORT_SetPinInterruptConfig(s_portBases[GPIO_PORT(pin)],
                                   GPIO_INDEX(pin),
                                   kPORT_InterruptOrDMADisabled);
#endif
#endif
    }
}
