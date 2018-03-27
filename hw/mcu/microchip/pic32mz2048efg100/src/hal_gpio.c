/**
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

#include "os/mynewt.h"
#include <hal/hal_gpio.h>
#include <mcu/mips_hal.h>
#include <mcu/p32mz2048efg100.h>

#define GPIO_INDEX(pin)     ((pin) & 0x0F)
#define GPIO_PORT(pin)      (((pin) >> 4) & 0x0F)
#define GPIO_MASK(pin)      (1 << GPIO_INDEX(pin))

#define LATxCLR(P)      (base_address[P].gpio[0x14 / 0x4])
#define LATxSET(P)      (base_address[P].gpio[0x18 / 0x4])
#define LATxINV(P)      (base_address[P].gpio[0x1C / 0x4])
#define PORTx(P)        (base_address[P].gpio[0x0])
#define CNPUxCLR(P)     (base_address[P].gpio[0x34 / 0x4])
#define CNPUxSET(P)     (base_address[P].gpio[0x38 / 0x4])
#define CNPDxCLR(P)     (base_address[P].gpio[0x44 / 0x4])
#define CNPDxSET(P)     (base_address[P].gpio[0x48 / 0x4])
#define ODCxCLR(P)      (base_address[P].gpio[0x24 / 0x4])
#define CNCONxSET(P)    (base_address[P].gpio[0x58 / 0x4])
#define CNENxCLR(P)     (base_address[P].gpio[0x64 / 0x4])
#define CNENxSET(P)     (base_address[P].gpio[0x68 / 0x4])
#define CNNExCLR(P)     (base_address[P].gpio[0x84 / 0x4])
#define CNNExSET(P)     (base_address[P].gpio[0x88 / 0x4])
#define CNFx(P)         (base_address[P].gpio[0x90 / 0x4])
#define CNFxCLR(P)      (base_address[P].gpio[0x94 / 0x4])
#define ANSELxCLR(P)    (base_address[P].ansel[0x04 / 0x4])
#define TRISxCLR(P)     (base_address[P].tris[0x04 / 0x4])
#define TRISxSET(P)     (base_address[P].tris[0x08 / 0x4])

struct hal_gpio_irq_t {
    int                    pin;
    hal_gpio_irq_trig_t    trig;
    hal_gpio_irq_handler_t handler;
    void                   *arg;
};

#define HAL_GPIO_MAX_IRQ    (8)
static struct hal_gpio_irq_t hal_gpio_irqs[HAL_GPIO_MAX_IRQ];

struct pic32_gpio_t {
    volatile uint32_t * gpio;
    volatile uint32_t * ansel;
    volatile uint32_t * tris;
};

static struct pic32_gpio_t base_address[] = {
    {
        .gpio  = (volatile uint32_t *)_PORTA_BASE_ADDRESS,
        .ansel = (volatile uint32_t *)&ANSELA,
        .tris  = (volatile uint32_t *)&TRISA
    },
    {
        .gpio  = (volatile uint32_t *)_PORTB_BASE_ADDRESS,
        .ansel = (volatile uint32_t *)&ANSELB,
        .tris  = (volatile uint32_t *)&TRISB
    },
    {
        .gpio  = (volatile uint32_t *)_PORTC_BASE_ADDRESS,
        .ansel = (volatile uint32_t *)&ANSELC,
        .tris  = (volatile uint32_t *)&TRISC
    },
    {
        .gpio  = (volatile uint32_t *)_PORTD_BASE_ADDRESS,
        .ansel = (volatile uint32_t *)&ANSELD,
        .tris  = (volatile uint32_t *)&TRISD
    },
    {
        .gpio  = (volatile uint32_t *)_PORTE_BASE_ADDRESS,
        .ansel = (volatile uint32_t *)&ANSELE,
        .tris  = (volatile uint32_t *)&TRISE
    },
    {
        .gpio  = (volatile uint32_t *)_PORTF_BASE_ADDRESS,
        .ansel = (volatile uint32_t *)&ANSELF,
        .tris  = (volatile uint32_t *)&TRISF
    },
    {
        .gpio  = (volatile uint32_t *)_PORTG_BASE_ADDRESS,
        .ansel = (volatile uint32_t *)&ANSELG,
        .tris  = (volatile uint32_t *)&TRISG
    }
};

static uint8_t
hal_gpio_find_pin(int pin)
{
    uint8_t index = 0;

    while (index < HAL_GPIO_MAX_IRQ) {
        if (hal_gpio_irqs[index].pin == pin) {
            break;
        }

        ++index;
    }

    return index;
}

static uint8_t
hal_gpio_find_empty_slot(void)
{
    uint8_t index = 0;

    while (index < HAL_GPIO_MAX_IRQ) {
        if (hal_gpio_irqs[index].handler == NULL) {
            break;
        }

        ++index;
    }

    return index;
}

static void
hal_gpio_handle_isr(uint32_t port)
{
    uint8_t index = 0;

    for (index = 0; index < HAL_GPIO_MAX_IRQ; ++index) {
        uint32_t mask, val;

        if (hal_gpio_irqs[index].handler == NULL) {
            continue;
        }
        if (GPIO_PORT(hal_gpio_irqs[index].pin) != port) {
            continue;
        }

        mask = GPIO_MASK(hal_gpio_irqs[index].pin);
        if (CNFx(port) & mask != mask) {
            continue;
        }

        val = PORTx(port) & mask;
        if ((val && (hal_gpio_irqs[index].trig & HAL_GPIO_TRIG_RISING)) ||
            (!val && (hal_gpio_irqs[index].trig & HAL_GPIO_TRIG_FALLING))) {
            hal_gpio_irqs[index].handler(hal_gpio_irqs[index].arg);
        }
        CNFxCLR(port) = mask;
    }
}

void
__attribute__((interrupt(IPL1AUTO), vector(_CHANGE_NOTICE_A_VECTOR)))
hal_gpio_porta_isr(void)
{
    hal_gpio_handle_isr(0);
    IFS3CLR = _IFS3_CNAIF_MASK;
}

void
__attribute__((interrupt(IPL1AUTO), vector(_CHANGE_NOTICE_B_VECTOR)))
hal_gpio_portb_isr(void)
{
    hal_gpio_handle_isr(1);
    IFS3CLR = _IFS3_CNBIF_MASK;
}

void
__attribute__((interrupt(IPL1AUTO), vector(_CHANGE_NOTICE_C_VECTOR)))
hal_gpio_portc_isr(void)
{
    hal_gpio_handle_isr(2);
    IFS3CLR = _IFS3_CNCIF_MASK;
}

void
__attribute__((interrupt(IPL1AUTO), vector(_CHANGE_NOTICE_D_VECTOR)))
hal_gpio_portd_isr(void)
{
    hal_gpio_handle_isr(3);
    IFS3CLR = _IFS3_CNDIF_MASK;
}

void
__attribute__((interrupt(IPL1AUTO), vector(_CHANGE_NOTICE_E_VECTOR)))
hal_gpio_porte_isr(void)
{
    hal_gpio_handle_isr(4);
    IFS3CLR = _IFS3_CNEIF_MASK;
}

void
__attribute__((interrupt(IPL1AUTO), vector(_CHANGE_NOTICE_F_VECTOR)))
hal_gpio_portf_isr(void)
{
    hal_gpio_handle_isr(5);
    IFS3CLR = _IFS3_CNFIF_MASK;
}

void
__attribute__((interrupt(IPL1AUTO), vector(_CHANGE_NOTICE_G_VECTOR)))
hal_gpio_portg_isr(void)
{
    hal_gpio_handle_isr(6);
    IFS3CLR = _IFS3_CNGIF_MASK;
}

int
hal_gpio_init_in(int pin, hal_gpio_pull_t pull)
{
    uint32_t port = GPIO_PORT(pin);
    uint32_t mask = GPIO_MASK(pin);

    /* Configure pin as digital */
    ANSELxCLR(port) = mask;

    ODCxCLR(port) = mask;

    switch (pull) {
        case HAL_GPIO_PULL_NONE:
            CNPUxCLR(port) = mask;
            CNPDxCLR(port) = mask;
            break;

        case HAL_GPIO_PULL_DOWN:
            CNPUxCLR(port) = mask;
            CNPDxSET(port) = mask;
            break;

        case HAL_GPIO_PULL_UP:
            CNPUxSET(port) = mask;
            CNPDxCLR(port) = mask;
            break;

        default:
            return -1;
    }

    /* Configure pin direction as input */
    TRISxSET(port) = mask;

    return 0;
}

int
hal_gpio_init_out(int pin, int val)
{
    uint32_t port = GPIO_PORT(pin);
    uint32_t mask = GPIO_MASK(pin);

    /* Configure pin as digital */
    ANSELxCLR(port) = mask;

    /* Disable pull-up, pull-down and open drain */
    CNPUxCLR(port) = mask;
    CNPDxCLR(port) = mask;
    ODCxCLR(port) = mask;

    if (val) {
        LATxSET(port) = mask;
    } else {
        LATxCLR(port) = mask;
    }

    /* Configure pin direction as output */
    TRISxCLR(port) = mask;

    return 0;
}

void
hal_gpio_write(int pin, int val)
{
    uint32_t port = GPIO_PORT(pin);
    uint32_t mask = GPIO_MASK(pin);

    if (val) {
        LATxSET(port) = mask;
    } else {
        LATxCLR(port) = mask;
    }
}

int
hal_gpio_read(int pin)
{
    uint32_t port = GPIO_PORT(pin);
    uint32_t mask = GPIO_MASK(pin);

    return !!(PORTx(port) & mask);
}

int
hal_gpio_toggle(int pin)
{
    uint32_t port = GPIO_PORT(pin);
    uint32_t mask = GPIO_MASK(pin);

    LATxINV(port) = mask;

    /*
     * One instruction cycle is required between a write and a read
     * operation on the same port.
     */
    asm volatile ("nop");

    return !!(PORTx(port) & mask);
}

int
hal_gpio_irq_init(int pin, hal_gpio_irq_handler_t handler, void *arg,
                  hal_gpio_irq_trig_t trig, hal_gpio_pull_t pull)
{
    uint32_t port = GPIO_PORT(pin);
    uint32_t mask = GPIO_MASK(pin);
    uint32_t ctx;
    int ret;
    uint8_t index;

    /* HAL_GPIO_TRIG_LOW and HAL_GPIO_TRIG_HIGH are not supported */
    if (trig == HAL_GPIO_TRIG_LOW ||
        trig == HAL_GPIO_TRIG_HIGH ||
        trig == HAL_GPIO_TRIG_NONE) {
        return -1;
    }

    /* Remove any existing irq handler attached to the pin */
    hal_gpio_irq_release(pin);
    hal_gpio_irq_disable(pin);

    index = hal_gpio_find_empty_slot();
    if (index == HAL_GPIO_MAX_IRQ) {
        return -1;
    }

    ret = hal_gpio_init_in(pin, pull);
    if (ret < 0) {
        return ret;
    }

    __HAL_DISABLE_INTERRUPTS(ctx);
    hal_gpio_irqs[index].arg = arg;
    hal_gpio_irqs[index].pin = pin;
    hal_gpio_irqs[index].trig = trig;
    hal_gpio_irqs[index].handler = handler;
    __HAL_ENABLE_INTERRUPTS(ctx);

    return 0;
}

void
hal_gpio_irq_release(int pin)
{
    uint32_t ctx;
    uint8_t index = hal_gpio_find_pin(pin);
    if (index == HAL_GPIO_MAX_IRQ) {
        return;
    }

    __HAL_DISABLE_INTERRUPTS(ctx);
    hal_gpio_irqs[index].handler = NULL;
    __HAL_ENABLE_INTERRUPTS(ctx);
}

void
hal_gpio_irq_enable(int pin)
{
    uint32_t port, mask, ctx;

    uint8_t index = hal_gpio_find_pin(pin);
    if (index == HAL_GPIO_MAX_IRQ)
        return;

    port = GPIO_PORT(pin);
    mask = GPIO_MASK(pin);

    __HAL_DISABLE_INTERRUPTS(ctx);

    /* Enable Change Notice module for the port */
    CNCONxSET(port) = _CNCONA_ON_MASK | _CNCONA_EDGEDETECT_MASK;

    switch (hal_gpio_irqs[index].trig) {
        case HAL_GPIO_TRIG_RISING:
            CNENxSET(port) = mask;
            break;
        case HAL_GPIO_TRIG_FALLING:
            CNNExSET(port) = mask;
            break;
        case HAL_GPIO_TRIG_BOTH:
            CNENxSET(port) = mask;
            CNNExSET(port) = mask;
            break;
        default:
            break;
    }

    /* Set interrupt priority */
    switch (port) {
        case 0:
            IPC29CLR = (_IPC29_CNAIP_MASK | _IPC29_CNAIS_MASK);
            IPC29 |= 1 << _IPC29_CNAIP_POSITION;
            break;
        case 1:
            IPC29CLR = (_IPC29_CNBIP_MASK | _IPC29_CNBIS_MASK);
            IPC29 |= 1 << _IPC29_CNBIP_POSITION;
            break;
        case 2:
            IPC30CLR = (_IPC30_CNCIP_MASK | _IPC30_CNCIS_MASK);
            IPC30 |= 1 << _IPC30_CNCIP_POSITION;
            break;
        case 3:
            IPC30CLR = (_IPC30_CNDIP_MASK | _IPC30_CNDIS_MASK);
            IPC30 |= 1 << _IPC30_CNDIP_POSITION;
            break;
        case 4:
            IPC30CLR = (_IPC30_CNEIP_MASK | _IPC30_CNEIS_MASK);
            IPC30 |= 1 << _IPC30_CNEIP_POSITION;
            break;
        case 5:
            IPC30CLR = (_IPC30_CNFIP_MASK | _IPC30_CNFIS_MASK);
            IPC30 |= 1 << _IPC30_CNFIP_POSITION;
            break;
        case 6:
            IPC31CLR = (_IPC31_CNGIP_MASK | _IPC31_CNGIS_MASK);
            IPC31 |= 1 << _IPC31_CNGIP_POSITION;
            break;
    }

    /* Clear interrupt flag and enable Change Notice interrupt */
    switch (port) {
        case 0:
            IFS3CLR = _IFS3_CNAIF_MASK;
            IEC3SET = _IEC3_CNAIE_MASK;
            break;
        case 1:
            IFS3CLR = _IFS3_CNBIF_MASK;
            IEC3SET = _IEC3_CNBIE_MASK;
            break;
        case 2:
            IFS3CLR = _IFS3_CNCIF_MASK;
            IEC3SET = _IEC3_CNCIE_MASK;
            break;
        case 3:
            IFS3CLR = _IFS3_CNDIF_MASK;
            IEC3SET = _IEC3_CNDIE_MASK;
            break;
        case 4:
            IFS3CLR = _IFS3_CNEIF_MASK;
            IEC3SET = _IEC3_CNEIE_MASK;
            break;
        case 5:
            IFS3CLR = _IFS3_CNFIF_MASK;
            IEC3SET = _IEC3_CNFIE_MASK;
            break;
        case 6:
            IFS3CLR = _IFS3_CNGIF_MASK;
            IEC3SET = _IEC3_CNGIE_MASK;
            break;
    }

    __HAL_ENABLE_INTERRUPTS(ctx);
}

void
hal_gpio_irq_disable(int pin)
{
    uint32_t port = GPIO_PORT(pin);
    uint32_t mask = GPIO_MASK(pin);

    CNENxCLR(port) = mask;
    CNNExCLR(port) = mask;
}
