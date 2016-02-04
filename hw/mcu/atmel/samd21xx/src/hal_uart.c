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

#include "hal/hal_uart.h"
#include "hal/hal_gpio.h"
#include "bsp/cmsis_nvic.h"
#include "bsp/bsp.h"
#include <mcu/samd21.h>
#include <assert.h>
#include <stdlib.h>
#include <usart.h>
#  include "usart_interrupt.h"

#define UART_CNT    (SERCOM_INST_NUM)

struct hal_uart {
    struct usart_module instance;   /* must be at top */
    uint8_t u_open;
    uint8_t u_rx_data;
    int16_t rxdata;
    int16_t txdata;    
    hal_uart_rx_char u_rx_func;
    hal_uart_tx_char u_tx_func;
    hal_uart_tx_done u_tx_done;
    void *u_func_arg;
};
static struct hal_uart uarts[UART_CNT];

static void 
usart_callback_txdone(struct usart_module *const module) {
    
    struct hal_uart *u = (struct hal_uart*) module;
    
    if(!u->u_open) {
        return;
    }
    
    if(u->u_tx_func) {
        u->txdata = u->u_tx_func(u->u_func_arg);    
    }
    if(u->txdata >= 0) {
        usart_write_job(&u->instance, (uint16_t *) &u->txdata);
    } else {
        if(u->u_tx_done) {
            u->u_tx_done(u->u_func_arg);    
        }
    }
}

static void
usart_callback_rx(struct usart_module *const module) {
    struct hal_uart *u = (struct hal_uart*) module;
    
    if(!u->u_open) {
        return;
    }
    
    if(u->u_rx_func) {
        u->u_rx_func(u->u_func_arg, (uint8_t) u->rxdata);
    }
    usart_read_job(&u->instance, (uint16_t*) &u->rxdata);      
}

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

void
hal_uart_start_rx(int port)
{
    struct hal_uart *u;

    u = &uarts[port];
    if (port >= UART_CNT || !u->u_open) {
        return;
    }    
    usart_read_job(&u->instance, (uint16_t*) &u->rxdata);    
}

void
hal_uart_start_tx(int port)
{
    struct hal_uart *u;
    u = &uarts[port];
    if (port >= UART_CNT || !u->u_open) {
        return;
    }
    
    if(u->u_tx_func) {
        u->txdata = u->u_tx_func(u->u_func_arg);    
        if(u->txdata >= 0) {
            usart_write_job(&u->instance, (uint16_t *) &u->txdata);
        }
    }
}

void
hal_uart_blocking_tx(int port, uint8_t data)
{
    struct hal_uart *u;
    u = &uarts[port];
    
    if(!u->u_open) {
        return;
    }
    
    usart_disable_callback(&u->instance, USART_CALLBACK_BUFFER_TRANSMITTED);    
    usart_write_wait(&u->instance, data);
    usart_enable_callback(&u->instance, USART_CALLBACK_BUFFER_TRANSMITTED);    
}

int
hal_uart_config(int port, int32_t baudrate, uint8_t databits, uint8_t stopbits,
  enum hal_uart_parity parity, enum hal_uart_flow_ctl flow_ctl)
{
    struct usart_module *pinst = &uarts[port].instance;
    struct usart_config config_usart;

    /* TODO get this done earlier */
    system_clock_init();
    
    if(uarts[port].u_open) {
        return -1;
    }
    
    /* TODO move to BSP */
    #define BSP_SERCOM      (SERCOM2)        
    #define BSP_PINMUX      (USART_RX_3_TX_2_XCK_3)   
    #define BSP_CLK_GENERATOR   (GCLK_GENERATOR_0)

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
    config_usart.mux_setting = BSP_PINMUX;
    config_usart.generator_source = BSP_CLK_GENERATOR;

    /* TODO. This should also be BSP specific.... */
    config_usart.sample_adjustment     = USART_SAMPLE_ADJUSTMENT_7_8_9;
    config_usart.sample_rate           = USART_SAMPLE_RATE_16X_ARITHMETIC;    
    
    config_usart.pinmux_pad0 = PINMUX_DEFAULT;
    config_usart.pinmux_pad1 = PINMUX_DEFAULT;
    config_usart.pinmux_pad2 = PINMUX_DEFAULT;
    config_usart.pinmux_pad3 = PINMUX_DEFAULT;

    /* register callbacks */
    usart_register_callback(pinst, usart_callback_txdone, 
                            USART_CALLBACK_BUFFER_TRANSMITTED);    

    usart_register_callback(pinst, usart_callback_rx, 
                            USART_CALLBACK_BUFFER_RECEIVED);         

    if(usart_init(pinst, BSP_SERCOM, &config_usart) != STATUS_OK) {
        return -1;
    }
    
    usart_enable_callback(pinst, USART_CALLBACK_BUFFER_TRANSMITTED);
    usart_enable_callback(pinst, USART_CALLBACK_BUFFER_RECEIVED);
    usart_enable(pinst); 
        
    uarts[port].u_open = 1;
    
    return 0;
}
