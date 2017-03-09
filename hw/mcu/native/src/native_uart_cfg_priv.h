#ifndef H_NATIVE_UART_CFG_PRIV_
#define H_NATIVE_UART_CFG_PRIV_

#include <termios.h>
#include "hal/hal_uart.h"

speed_t uart_baud_to_speed(int_least32_t baud);
int uart_dev_set_attr(int fd, int32_t baudrate, uint8_t databits,
                      uint8_t stopbits, enum hal_uart_parity parity,
                      enum hal_uart_flow_ctl flow_ctl);

#endif
