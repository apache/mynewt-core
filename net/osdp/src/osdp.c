/*
 * Copyright (c) 2020 Siddharth Chandrasekaran <siddharth@embedjournal.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "modlog/modlog.h"
#include "uart/uart.h"
#include "tinycrypt/aes.h"

#include "osdp/osdp_common.h"

/* Interval in ticks */
#define OSDP_REFRESH_INTERVAL \
    (OS_TICKS_PER_SEC * MYNEWT_VAL(OSDP_REFRESH_INTERVAL_MS)/1000 + 1)

struct osdp_ring {
    uint8_t head;
    uint8_t tail;
    uint16_t size;
    uint8_t *buf;
};

struct osdp_device {
    struct osdp_ring rx_ring;
    struct osdp_ring tx_ring;
    uint8_t rx_buf[MYNEWT_VAL(OSDP_UART_RX_BUFFER_LENGTH)];
    uint8_t tx_buf[MYNEWT_VAL(OSDP_UART_TX_BUFFER_LENGTH)];
    struct uart_dev *uart;
};

static struct osdp_ctx osdp_ctx;
static struct osdp_device osdp_device;
static struct os_callout osdp_refresh_timer;

static inline int
inc_and_wrap(int i, int max)
{
    return (i + 1) & (max - 1);
}

static void
osdp_ring_add_char(struct osdp_ring *or, char ch)
{
    or->buf[or->head] = ch;
    or->head = inc_and_wrap(or->head, or->size);
}

static uint8_t
osdp_ring_pull_char(struct osdp_ring *or)
{
    uint8_t ch;

    ch = or->buf[or->tail];
    or->tail = inc_and_wrap(or->tail, or->size);
    return ch;
}

static bool
osdp_ring_is_full(const struct osdp_ring *or)
{
    return inc_and_wrap(or->head, or->size) == or->tail;
}

static bool
osdp_ring_is_empty(const struct osdp_ring *or)
{
    return or->head == or->tail;
}

/*
 * Handle incoming byte
 */
static void
osdp_handle_in_byte(struct osdp_device *od, uint8_t *buf, int len)
{
    for (int i = 0; i < len; ++i) {
        osdp_ring_add_char(&od->rx_ring, buf[i]);
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

    if (osdp_ring_is_empty(&od->tx_ring)) {
        return -1;
    }
    ch = osdp_ring_pull_char(&od->tx_ring);

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
        if (osdp_ring_is_empty(&od->rx_ring)) {
            break;
        }
        buf[i] = osdp_ring_pull_char(&od->rx_ring);
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
        if (osdp_ring_is_full(&od->tx_ring)) {
            break;
        }
        osdp_ring_add_char(&od->tx_ring, buf[i]);
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

    od->tx_ring.head = 0;
    od->tx_ring.tail = 0;

    od->rx_ring.head = 0;
    od->rx_ring.tail = 0;
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
 * Stop OSDP library. Called by application
 */
void
osdp_stop(void)
{
    int rc;
    struct osdp *ctx;
    struct osdp_device *od = &osdp_device;

    ctx = osdp_get_ctx();
    assert(ctx);

    /* Stop timer */
    os_callout_stop(&osdp_refresh_timer);

    /* Cleanup */
#if MYNEWT_VAL(OSDP_MODE_PD)
    osdp_pd_teardown(ctx);
#else
    osdp_cp_teardown(ctx);
#endif

    /* Stop uart */
    assert(od->uart);
    rc = os_dev_close((struct os_dev *)od->uart);
    assert(rc == 0);

    /* Flush circular buffers */
    osdp_uart_flush((void *)od);

    /* Reset OSDP context */
    memset(&osdp_ctx, 0, sizeof(struct osdp_ctx));
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

    od->tx_ring.size = MYNEWT_VAL(OSDP_UART_TX_BUFFER_LENGTH);
    od->rx_ring.size = MYNEWT_VAL(OSDP_UART_RX_BUFFER_LENGTH);
    od->tx_ring.buf = od->tx_buf;
    od->rx_ring.buf = od->rx_buf;

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

    OSDP_LOG_INFO("osdp: init OK\n");
}
