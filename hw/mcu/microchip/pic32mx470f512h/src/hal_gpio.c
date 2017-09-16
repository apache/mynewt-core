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

#include <os/os.h>
#include <hal/hal_gpio.h>
#include <mcu/mips_hal.h>
#include <mcu/p32mx470f512h.h>

#define GPIO_INDEX(pin)     ((pin) & 0x0F)
#define GPIO_PORT(pin)      (((pin) >> 4) & 0x0F)
#define GPIO_MASK(pin)      (1 << GPIO_INDEX(pin))

#define PORTx(P)        (base_address[P].gpio[0x0])
#define LATxCLR(P)      (base_address[P].gpio[0x14 / 0x4])
#define LATxSET(P)      (base_address[P].gpio[0x18 / 0x4])
#define LATxINV(P)      (base_address[P].gpio[0x1C / 0x4])
#define ODCxCLR(P)      (base_address[P].gpio[0x24 / 0x4])
#define CNPUxCLR(P)     (base_address[P].gpio[0x34 / 0x4])
#define CNPUxSET(P)     (base_address[P].gpio[0x38 / 0x4])
#define CNPDxCLR(P)     (base_address[P].gpio[0x44 / 0x4])
#define CNPDxSET(P)     (base_address[P].gpio[0x48 / 0x4])
#define CNCONxSET(P)    (base_address[P].gpio[0x58 / 0x4])
#define CNENxCLR(P)     (base_address[P].gpio[0x64 / 0x4])
#define CNENxSET(P)     (base_address[P].gpio[0x68 / 0x4])
#define CNSTATx(P)      (base_address[P].gpio[0x70 / 0x4])
#define CNSTATxCLR(P)   (base_address[P].gpio[0x74 / 0x4])
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
        /* No PORT A on PIC32MX470F512H */
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
        if (CNSTATx(port) & mask != mask) {
            continue;
        }

        val = PORTx(port) & mask;
        if ((val && (hal_gpio_irqs[index].trig & HAL_GPIO_TRIG_RISING)) ||
            (!val && (hal_gpio_irqs[index].trig & HAL_GPIO_TRIG_FALLING))) {
            hal_gpio_irqs[index].handler(hal_gpio_irqs[index].arg);
        }
        CNSTATxCLR(port) = mask;
    }
}

void
__attribute__((interrupt(IPL1AUTO), vector(_CHANGE_NOTICE_VECTOR)))
hal_gpio_isr(void)
{
    if (IFS1 & _IFS1_CNBIF_MASK) {
        hal_gpio_handle_isr(1);
        IFS1CLR = _IFS1_CNBIF_MASK;
    }

    if (IFS1 & _IFS1_CNCIF_MASK) {
        hal_gpio_handle_isr(2);
        IFS1CLR = _IFS1_CNCIF_MASK;
    }

    if (IFS1 & _IFS1_CNDIF_MASK) {
        hal_gpio_handle_isr(3);
        IFS1CLR = _IFS1_CNDIF_MASK;
    }

    if (IFS1 & _IFS1_CNEIF_MASK) {
        hal_gpio_handle_isr(4);
        IFS1CLR = _IFS1_CNEIF_MASK;
    }

    if (IFS1 & _IFS1_CNFIF_MASK) {
        hal_gpio_handle_isr(5);
        IFS1CLR = _IFS1_CNFIF_MASK;
    }

    if (IFS1 & _IFS1_CNGIF_MASK) {
        hal_gpio_handle_isr(6);
        IFS1CLR = _IFS1_CNGIF_MASK;
    }
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

    CNCONxSET(port) = _CNCONB_ON_MASK;
    CNENxSET(port) = mask;

    /* Read PORT register to clear mismatch condition on CN input pin */
    (void)PORTx(port);

    /* Set Change Notice interrupt priority */
    IPC8CLR = _IPC8_CNIP_MASK | _IPC8_CNIS_MASK;
    IPC8SET = 1 << _IPC8_CNIP_POSITION;

    /* Clear interrupt flag and enable Change Notification interrupt */
    switch (port) {
        case 1:
            IFS1CLR = _IFS1_CNBIF_MASK;
            IEC1SET = _IEC1_CNBIE_MASK;
            break;
        case 2:
            IFS1CLR = _IFS1_CNCIF_MASK;
            IEC1SET = _IEC1_CNCIE_MASK;
            break;
        case 3:
            IFS1CLR = _IFS1_CNDIF_MASK;
            IEC1SET = _IEC1_CNDIE_MASK;
            break;
        case 4:
            IFS1CLR = _IFS1_CNEIF_MASK;
            IEC1SET = _IEC1_CNEIE_MASK;
            break;
        case 5:
            IFS1CLR = _IFS1_CNFIF_MASK;
            IEC1SET = _IEC1_CNFIE_MASK;
            break;
        case 6:
            IFS1CLR = _IFS1_CNGIF_MASK;
            IEC1SET = _IEC1_CNGIE_MASK;
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
}
