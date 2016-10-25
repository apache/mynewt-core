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
#include <sys/types.h>
#include <stdio.h>

#include "os/os_dev.h"
#include "syscfg/syscfg.h"

#include "flash_map/flash_map.h"
#include "hal/hal_flash.h"
#include "uart/uart.h"
#include "uart_hal/uart_hal.h"
#include "hal/hal_uart.h"

#include "bsp/cmsis_nvic.h"

#include "mcu/frdm-k64f_hal.h"
#include "fsl_device_registers.h"
#include "fsl_common.h"
#include "fsl_clock.h"
#include "fsl_port.h"

#include "clock_config.h"

#if MYNEWT_VAL(UART_0)
static struct uart_dev os_bsp_uart0;
#endif
#if MYNEWT_VAL(UART_1)
static struct uart_dev os_bsp_uart1;
#endif
#if MYNEWT_VAL(UART_2)
static struct uart_dev os_bsp_uart2;
#endif
#if MYNEWT_VAL(UART_3)
static struct uart_dev os_bsp_uart3;
#endif
#if MYNEWT_VAL(UART_4)
static struct uart_dev os_bsp_uart4;
#endif
#if MYNEWT_VAL(UART_5)
static struct uart_dev os_bsp_uart5;
#endif

static void init_hardware(void)
{
    // Disable the MPU otherwise USB cannot access the bus
    MPU->CESR = 0;

    // Enable all the ports
    SIM->SCGC5 |= (SIM_SCGC5_PORTA_MASK | SIM_SCGC5_PORTB_MASK | SIM_SCGC5_PORTC_MASK | SIM_SCGC5_PORTD_MASK |
                   SIM_SCGC5_PORTE_MASK);
}

extern void BOARD_BootClockRUN(void);

void
hal_bsp_init(void)
{
    int rc = 0;

    // Init pinmux and other hardware setup.
    init_hardware();
    BOARD_BootClockRUN();

#if MYNEWT_VAL(UART_0)
    rc = os_dev_create((struct os_dev *) &os_bsp_uart0, "uart0",
      OS_DEV_INIT_PRIMARY, 0, uart_hal_init, NULL);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(UART_1)
    rc = os_dev_create((struct os_dev *) &os_bsp_uart1, "uart1",
      OS_DEV_INIT_PRIMARY, 0, uart_hal_init, NULL);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(UART_2)
    rc = os_dev_create((struct os_dev *) &os_bsp_uart2, "uart2",
      OS_DEV_INIT_PRIMARY, 0, uart_hal_init, NULL);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(UART_3)
    rc = os_dev_create((struct os_dev *) &os_bsp_uart3, "uart3",
      OS_DEV_INIT_PRIMARY, 0, uart_hal_init, NULL);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(UART_4)
    rc = os_dev_create((struct os_dev *) &os_bsp_uart4, "uart4",
      OS_DEV_INIT_PRIMARY, 0, uart_hal_init, NULL);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(UART_5)
    rc = os_dev_create((struct os_dev *) &os_bsp_uart5, "uart5",
      OS_DEV_INIT_PRIMARY, 0, uart_hal_init, NULL);
    assert(rc == 0);
#endif
}
