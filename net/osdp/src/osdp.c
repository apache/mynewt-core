/*
 * Copyright (c) 2020 Siddharth Chandrasekaran <siddharth@embedjournal.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "modlog/modlog.h"
#include "uart/uart.h"
#include "tinycrypt/aes.h"

#include "osdp/circular_buffer.h"
#include "osdp/osdp_common.h"

#define OSDP_REFRESH_INTERVAL (OS_TICKS_PER_SEC / 20)

struct osdp_device {
    cbuf_handle_t rx_buf;
    cbuf_handle_t tx_buf;
    uint8_t rx_fbuf[MYNEWT_VAL(OSDP_UART_BUFFER_LENGTH)];
    uint8_t tx_fbuf[MYNEWT_VAL(OSDP_UART_BUFFER_LENGTH)];
    struct uart_dev *uart;
};

static struct osdp_ctx osdp_ctx;
static struct osdp_device osdp_device;
static struct os_callout osdp_refresh_timer;

/*
 * Handle incoming byte
 */
static void
osdp_handle_in_byte(struct osdp_device *od, uint8_t *buf, int len)
{
    for (int i = 0; i < len; ++i) {
        circular_buf_put(od->rx_buf, buf[i]);
    }
}

/*
 * ISR handler for tx
 */
static int
osdp_uart_tx(void *arg)
{
    uint8_t ch = 0;
    struct osdp_device *od = arg;

    if (circular_buf_get(od->tx_buf, &ch) == -1) {
        return -1;
    }

    return ch;
}

/*
 * ISR handler for rx
 */
static int
osdp_uart_rx(void *arg, uint8_t ch)
{
    struct osdp_device *od = arg;

    osdp_handle_in_byte(od, &ch, 1);

    return 0;
}

/*
 * Retrieve characters from rx buffer
 * Called from refresh handler
 */
static int
osdp_uart_receive(void *data, uint8_t *buf, int len)
{
    struct osdp_device *od = data;
    int i;

    /* Get characters from buffer to parse */
    for (i = 0; i < len; ++i) {
        if (circular_buf_get(od->rx_buf, &buf[i]) == -1) {
            break;
        }
    }

    return i;
}

/*
 * Place characters in the tx buffer
 * Called from the refresh handler
 */
static int
osdp_uart_send(void *data, uint8_t *buf, int len)
{
    int i;
    struct osdp_device *od = data;

    /* Place characters in the tx buffer */
    for (i = 0; i < len; ++i) {
        if (circular_buf_put2(od->tx_buf, buf[i]) == -1) {
            break;
        }
    }

    /* Start transmission */
    uart_start_tx(od->uart);

    /* Return total characters to be sent */
    return i;
}

/*
 * Reset tx/rx buffers
 */
static void
osdp_uart_flush(void *data)
{
    struct osdp_device *od = data;

    circular_buf_reset(od->tx_buf);
    circular_buf_reset(od->rx_buf);
}

/*
 * Get context handle
 */
struct osdp *
osdp_get_ctx()
{
    return &osdp_ctx.ctx;
}

/*
 * Timer handler
 */
void
osdp_refresh_handler(struct os_event *ev)
{
    struct osdp *ctx = osdp_get_ctx();
    osdp_refresh(ctx);

    /* Restart */
    os_callout_reset(&osdp_refresh_timer, OSDP_REFRESH_INTERVAL);
}

/*
 * Start OSDP. Called by application
 */
void
osdp_init(osdp_pd_info_t *info, uint8_t *scbk)
{
    struct osdp *ctx;
    struct osdp_device *od = &osdp_device;

    /* Assign remaining function handlers not managed by the application layer */
    info->channel.send = osdp_uart_send;
    info->channel.recv = osdp_uart_receive;
    info->channel.flush = osdp_uart_flush;
    info->channel.data = &osdp_device;

    od->tx_buf = circular_buf_init(od->tx_fbuf, sizeof(od->tx_fbuf));
    od->rx_buf = circular_buf_init(od->rx_fbuf, sizeof(od->rx_fbuf));

    struct uart_conf uc = {
        .uc_speed = info->baud_rate,
        .uc_databits = 8,
        .uc_stopbits = 1,
        .uc_parity = UART_PARITY_NONE,
        .uc_flow_ctl = UART_FLOW_CTL_NONE,
        .uc_tx_char = osdp_uart_tx,
        .uc_rx_char = osdp_uart_rx,
        .uc_cb_arg = od,
    };

    od->uart = (struct uart_dev *)os_dev_open(MYNEWT_VAL(OSDP_UART_DEV_NAME),
          OS_TIMEOUT_NEVER, &uc);
    assert(od->uart != NULL);

    /* Setup OSDP */
#if MYNEWT_VAL(OSDP_MODE_PD)
    ctx = osdp_pd_setup(&osdp_ctx, info, scbk);
    assert(ctx != NULL);
#else
    ctx = osdp_cp_setup(&osdp_ctx, MYNEWT_VAL(OSDP_NUM_CONNECTED_PD), info, scbk);
    assert(ctx != NULL);
#endif

    /* Configure and reset timer */
    os_callout_init(&osdp_refresh_timer, os_eventq_dflt_get(),
      osdp_refresh_handler, NULL);

    os_callout_reset(&osdp_refresh_timer, OSDP_REFRESH_INTERVAL);

    OSDP_LOG_INFO("OSDP OK\n");
}
