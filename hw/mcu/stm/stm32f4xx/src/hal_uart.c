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

#include "hal/hal_uart.h"
#include "hal/hal_gpio.h"
#include "bsp/cmsis_nvic.h"
#include "bsp/bsp.h"
#include <mcu/samd21.h>
#include <assert.h>
#include <stdlib.h>
#include "usart.h"

#define UART_CNT    (SERCOM_INST_NUM)

struct hal_uart {
    uint8_t u_open:1;
    uint8_t u_rx_stall:1;
    uint8_t u_tx_end:1;
    uint8_t u_rx_data;
    hal_uart_rx_char u_rx_func;
    hal_uart_tx_char u_tx_func;
    hal_uart_tx_done u_tx_done;
    void *u_func_arg;
};
static struct hal_uart uarts[UART_CNT];

struct hal_uart_irq {
    struct hal_uart *ui_uart;
    volatile uint32_t ui_cnt;
};
static struct hal_uart_irq uart_irqs[UART_CNT];

/* the samd atmel code instance */
struct usart_module usart_instance;

int
hal_uart_init_cbs(int port, hal_uart_tx_char tx_func, hal_uart_tx_done tx_done,
  hal_uart_rx_char rx_func, void *arg)
{
    struct hal_uart *u;

    u = &uarts[port];
    if (port >= UART_CNT || u->u_open) {
        return -1;
    }
    u->u_rx_func = rx_func;
    u->u_tx_func = tx_func;
    u->u_tx_done = tx_done;
    u->u_func_arg = arg;
    return 0;
}

static void
uart_irq_handler(int num)
{

}

void
hal_uart_start_rx(int port)
{

}

void
hal_uart_start_tx(int port)
{

}

void
hal_uart_blocking_tx(int port, uint8_t data)
{

}

static void
uart_irq1(void)
{
    uart_irq_handler(0);
}

static void
uart_irq2(void)
{
    uart_irq_handler(1);

}

static void
uart_irq3(void)
{
    uart_irq_handler(2);
}

static void
uart_irq4(void)
{
    uart_irq_handler(3);
}

static void
uart_irq5(void)
{
    uart_irq_handler(4);
}

static void
uart_irq6(void)
{
    uart_irq_handler(5);
}

static void
hal_uart_set_nvic(IRQn_Type irqn, struct hal_uart *uart)
{

}

int
hal_uart_config(int port, int32_t baudrate, uint8_t databits, uint8_t stopbits,
  enum hal_uart_parity parity, enum hal_uart_flow_ctl flow_ctl)
{
#if 1
	struct usart_config config_usart;
        
	usart_get_config_defaults(&config_usart);
        
	config_usart.baudrate    = baudrate;
        
        config_usart.transfer_mode = USART_TRANSFER_ASYNCHRONOUSLY;
        
        switch(databits) {
            case 5:
                config_usart.character_size = USART_CHARACTER_SIZE_5BIT;
                break;
            case 6:
                config_usart.character_size = USART_CHARACTER_SIZE_6BIT;
                break;
            case 7:
                config_usart.character_size = USART_CHARACTER_SIZE_7BIT;
                break;
            case 8:
                config_usart.character_size = USART_CHARACTER_SIZE_8BIT;
                break;
            case 9:
                config_usart.character_size = USART_CHARACTER_SIZE_9BIT;
                break;
            default:
                return -1;                        
        }

        switch(parity) {
            case HAL_UART_PARITY_NONE:
                config_usart.parity = USART_PARITY_NONE;
                break;
            case HAL_UART_PARITY_ODD:
                config_usart.parity = USART_PARITY_ODD;
                break;
            case HAL_UART_PARITY_EVEN:
                config_usart.parity = USART_PARITY_EVEN;
                break;
            default:
                return -1;
        }

        switch(stopbits) {
            case 1:
                config_usart.stopbits = USART_STOPBITS_1;
                break;
            case 2:
                config_usart.stopbits = USART_STOPBITS_2;
                break;
            default:
                return -1;
        }
                        
        switch(flow_ctl) {
            case HAL_UART_FLOW_CTL_RTS_CTS:
                break;
            case HAL_UART_FLOW_CTL_NONE:
                break;
            default:
                return -1;
        }
                    
        /* hard code for now but move to BSP */
        config_usart.mux_settings = USART_RX_3_TX_2_XCK_3;
        
	config_usart.pinmux_pad0 = PINMUX_DEFAULT;
	config_usart.pinmux_pad1 = PINMUX_DEFAULT;
	config_usart.pinmux_pad2 = PINMUX_DEFAULT;
	config_usart.pinmux_pad3 = PINMUX_DEFAULT;

//! [setup_set_config]
	while (usart_init(&usart_instance,
			SERCOM2, &config_usart) != STATUS_OK) {
	}
	usart_enable(&usart_instance);
    
    return 0;
}
