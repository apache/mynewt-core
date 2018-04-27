#ifndef _GNSS_UART_H_
#define _GNSS_UART_H_

#include <stdint.h>
#include <uart/uart.h>

/**
 * GNSS UART configuration
 */
struct gnss_uart {
    char            *device; /**< Device used for UART (ex: uart0, uart1) */

    struct uart_conf uc;    
    struct uart_dev *dev; 
    struct os_sem    sem;
    uint8_t         *buffer;
    uint16_t         bufsize;
};

/**
 * Initialize the transport layer with UART.
 *
 * @param ctx		GNSS context 
 * @param uart		Configuration
 *
 * @return true on success
 */
int gnss_uart_init(gnss_t *ctx, struct gnss_uart *uart);

#endif

