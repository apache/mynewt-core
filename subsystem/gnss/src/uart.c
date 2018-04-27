#include <stdint.h>
#include <uart/uart.h>
#include <gnss/gnss.h>
#include <gnss/uart.h>

static int
gnss_uart_speed(gnss_t *ctx, uint32_t speed)
{
    struct gnss_uart *uart = ctx->transport.conf;

    assert(uart->dev == NULL);
    uart->uc.uc_speed = speed;

    return 0;
}
		    
static int
gnss_uart_send(gnss_t *ctx, uint8_t *bytes, uint16_t size)
{
#if MYNEWT_VAL(GNSS_RX_ONLY) == 0
    struct gnss_uart *uart = ctx->transport.conf;
    
    if ((bytes == NULL) || (size == 0)) {
	return 0;
    }

    os_sem_pend(&uart->sem, OS_TIMEOUT_NEVER);
    uart->buffer  = bytes;
    uart->bufsize = size;
    uart_start_tx(uart->dev);
    return size;
#else
    return -1;
#endif
}

static int
gnss_uart_start_rx(gnss_t *ctx)
{
    struct gnss_uart *uart = ctx->transport.conf;

    uart->dev = (struct uart_dev *)
	os_dev_open(uart->device, OS_TIMEOUT_NEVER, &uart->uc);

    return (uart->dev == NULL) ? -1 : 0;
}

static int
gnss_uart_stop_rx(gnss_t *ctx)
{
    int rc;
    struct gnss_uart *uart = ctx->transport.conf;
    
    rc = os_dev_close((struct os_dev *)uart->dev);
    if (rc == 0) {
	uart->dev = NULL;
    }

    return rc == 0 ? 0 : -1;
}

static int
gnss_uart_rx_char(void *arg, uint8_t byte)
{
    gnss_t *ctx = (gnss_t *)arg;
    
    assert(ctx->protocol.decoder != NULL);

    /* Stop transport layer if:
     *   - we've got an error event pending
     *   - the decoder aborted
     *   => usually means baud rate need to be fixed
     */
    if ((ctx->error != 0 ) ||
	(ctx->protocol.decoder(ctx, byte) == GNSS_BYTE_DECODER_ABORTED)) {
        // XXX: is it safe to close in interrupt context?
	gnss_uart_stop_rx(ctx); 
	return 0;
    } 

    return 1;
}

#if MYNEWT_VAL(GNSS_RX_ONLY) == 0
static int
gnss_uart_tx_char(void *arg)
{
    gnss_t *ctx = (gnss_t *)arg;
    struct gnss_uart *uart = ctx->transport.conf;
    if ((uart->buffer == NULL) || (uart->bufsize == 0)) {
        return -1;
    }
    uart->bufsize--;
    return *uart->buffer++;
}

static void
gnss_uart_tx_done(void *arg)
{
    gnss_t *ctx = (gnss_t *)arg;
    struct gnss_uart *uart = ctx->transport.conf;
    os_sem_release(&uart->sem);
}
#endif

int
gnss_uart_init(gnss_t *ctx, struct gnss_uart *uart)
{
    if (os_dev_lookup(uart->device) == NULL) {
	return -1;
    }
    
    ctx->transport.conf     = uart;
    ctx->transport.speed    = gnss_uart_speed;
    ctx->transport.start_rx = gnss_uart_start_rx;
    ctx->transport.stop_rx  = gnss_uart_stop_rx;
#if MYNEWT_VAL(GNSS_RX_ONLY) == 0
    ctx->transport.send     = gnss_uart_send;
#endif
    
    os_sem_init(&uart->sem, 1);

    uart->uc = (struct uart_conf) {
	.uc_speed    = 9600,
	.uc_databits = 8,
	.uc_stopbits = 1,
	.uc_parity   = UART_PARITY_NONE,
	.uc_flow_ctl = UART_FLOW_CTL_NONE,
	.uc_rx_char  = gnss_uart_rx_char,
#if MYNEWT_VAL(GNSS_RX_ONLY) == 0
	.uc_tx_char  = gnss_uart_tx_char,
	.uc_tx_done  = gnss_uart_tx_done,
#endif
	.uc_cb_arg   = ctx,
    };

    return 0;
}
