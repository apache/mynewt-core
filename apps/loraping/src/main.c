/*
Copyright (c) 2013, SEMTECH S.A.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the Semtech corporation nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL SEMTECH S.A. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Description: Ping-Pong implementation.  Adapted to run in the MyNewt OS.
*/

#include <string.h>
#include "os/mynewt.h"
#include "hal/hal_gpio.h"
#include "hal/hal_spi.h"
#include "bsp/bsp.h"
#include "radio/radio.h"
#include "loraping.h"

#define USE_BAND_915

#if defined(USE_BAND_433)

#define RF_FREQUENCY               434000000 /* Hz */

#elif defined(USE_BAND_780)

#define RF_FREQUENCY               780000000 /* Hz */

#elif defined(USE_BAND_868)

#define RF_FREQUENCY               868000000 /* Hz */

#elif defined(USE_BAND_915)

#define RF_FREQUENCY               915000000 /* Hz */

#else
    #error "Please define a frequency band in the compiler options."
#endif

#define LORAPING_TX_OUTPUT_POWER            14        /* dBm */

#define LORAPING_BANDWIDTH                  0         /* [0: 125 kHz, */
                                                  /*  1: 250 kHz, */
                                                  /*  2: 500 kHz, */
                                                  /*  3: Reserved] */
#define LORAPING_SPREADING_FACTOR           7         /* [SF7..SF12] */
#define LORAPING_CODINGRATE                 1         /* [1: 4/5, */
                                                  /*  2: 4/6, */
                                                  /*  3: 4/7, */
                                                  /*  4: 4/8] */
#define LORAPING_PREAMBLE_LENGTH            8         /* Same for Tx and Rx */
#define LORAPING_SYMBOL_TIMEOUT             5         /* Symbols */
#define LORAPING_FIX_LENGTH_PAYLOAD_ON      false
#define LORAPING_IQ_INVERSION_ON            false

#define LORAPING_TX_TIMEOUT_MS              3000    /* ms */
#define LORAPING_RX_TIMEOUT_MS              1000    /* ms */
#define LORAPING_BUFFER_SIZE                64

const uint8_t loraping_ping_msg[] = "PING";
const uint8_t loraping_pong_msg[] = "PONG";

static uint8_t loraping_buffer[LORAPING_BUFFER_SIZE];
static int loraping_rx_size;
static int loraping_is_master = 1;

struct {
    int rx_timeout;
    int rx_ping;
    int rx_pong;
    int rx_other;
    int rx_error;
    int tx_timeout;
    int tx_success;
} loraping_stats;

static void loraping_tx(struct os_event *ev);
static void loraping_rx(struct os_event *ev);

static struct os_event loraping_ev_tx = {
    .ev_cb = loraping_tx,
};
static struct os_event loraping_ev_rx = {
    .ev_cb = loraping_rx,
};

static void
send_once(int is_ping)
{
    int i;

    if (is_ping) {
        memcpy(loraping_buffer, loraping_ping_msg, 4);
    } else {
        memcpy(loraping_buffer, loraping_pong_msg, 4);
    }
    for (i = 4; i < sizeof loraping_buffer; i++) {
        loraping_buffer[i] = i - 4;
    }

    Radio.Send(loraping_buffer, sizeof loraping_buffer);
}

static void
loraping_tx(struct os_event *ev)
{
    /* Print information about last rx attempt. */
    loraping_rxinfo_print();

    if (loraping_rx_size == 0) {
        /* Timeout. */
    } else {
        os_time_delay(1);
        if (memcmp(loraping_buffer, loraping_pong_msg, 4) == 0) {
            loraping_stats.rx_ping++;
        } else if (memcmp(loraping_buffer, loraping_ping_msg, 4) == 0) {
            loraping_stats.rx_pong++;

            /* A master already exists.  Become a slave. */
            loraping_is_master = 0;
        } else {
            /* Valid reception but neither a PING nor a PONG message. */
            loraping_stats.rx_other++;
            /* Set device as master and start again. */
            loraping_is_master = 1;
        }
    }

    loraping_rx_size = 0;
    send_once(loraping_is_master);
}

static void
loraping_rx(struct os_event *ev)
{
    Radio.Rx(LORAPING_RX_TIMEOUT_MS);
}

static void
on_tx_done(void)
{
    loraping_stats.tx_success++;
    Radio.Sleep();

    os_eventq_put(os_eventq_dflt_get(), &loraping_ev_rx);
}

static void
on_rx_done(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr)
{
    Radio.Sleep();

    if (size > sizeof loraping_buffer) {
        size = sizeof loraping_buffer;
    }

    loraping_rx_size = size;
    memcpy(loraping_buffer, payload, size);

    loraping_rxinfo_rxed(rssi, snr);

    os_eventq_put(os_eventq_dflt_get(), &loraping_ev_tx);
}

static void
on_tx_timeout(void)
{
    Radio.Sleep();

    loraping_stats.tx_timeout++;

    os_eventq_put(os_eventq_dflt_get(), &loraping_ev_rx);
}

static void
on_rx_timeout(void)
{
    Radio.Sleep();

    loraping_stats.rx_timeout++;
    loraping_rxinfo_timeout();

    os_eventq_put(os_eventq_dflt_get(), &loraping_ev_tx);
}

static void
on_rx_error(void)
{
    loraping_stats.rx_error++;
    Radio.Sleep();

    os_eventq_put(os_eventq_dflt_get(), &loraping_ev_tx);
}

int
main(void)
{
    RadioEvents_t radio_events;

#ifdef ARCH_sim
    mcu_sim_parse_args(argc, argv);
#endif

    sysinit();

    hal_timer_config(4, 1000000);

    /* Radio initialization. */
    radio_events.TxDone = on_tx_done;
    radio_events.RxDone = on_rx_done;
    radio_events.TxTimeout = on_tx_timeout;
    radio_events.RxTimeout = on_rx_timeout;
    radio_events.RxError = on_rx_error;

    Radio.Init(&radio_events);

    Radio.SetChannel(RF_FREQUENCY);

    Radio.SetTxConfig(MODEM_LORA,
                      LORAPING_TX_OUTPUT_POWER,
                      0,        /* Frequency deviation; unused with LoRa. */
                      LORAPING_BANDWIDTH,
                      LORAPING_SPREADING_FACTOR,
                      LORAPING_CODINGRATE,
                      LORAPING_PREAMBLE_LENGTH,
                      LORAPING_FIX_LENGTH_PAYLOAD_ON,
                      true,     /* CRC enabled. */
                      0,        /* Frequency hopping disabled. */
                      0,        /* Hop period; N/A. */
                      LORAPING_IQ_INVERSION_ON,
                      LORAPING_TX_TIMEOUT_MS);

    Radio.SetRxConfig(MODEM_LORA,
                      LORAPING_BANDWIDTH,
                      LORAPING_SPREADING_FACTOR,
                      LORAPING_CODINGRATE,
                      0,        /* AFC bandwisth; unused with LoRa. */
                      LORAPING_PREAMBLE_LENGTH,
                      LORAPING_SYMBOL_TIMEOUT,
                      LORAPING_FIX_LENGTH_PAYLOAD_ON,
                      0,        /* Fixed payload length; N/A. */
                      true,     /* CRC enabled. */
                      0,        /* Frequency hopping disabled. */
                      0,        /* Hop period; N/A. */
                      LORAPING_IQ_INVERSION_ON,
                      true);    /* Continuous receive mode. */

    /* Immediately receive on start up. */
    os_eventq_put(os_eventq_dflt_get(), &loraping_ev_rx);

    /*
     * As the last thing, process events from default event queue.
     */
    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
}
