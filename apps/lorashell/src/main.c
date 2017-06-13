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

/**
 * A simple app for LoRa phy testing.  A typical usage scenario is:
 *
 * ##### Receiver
 * # Sit on a single channel.
 * lora set_freq 915000000
 *
 * # Allow 250-byte packets.
 * lora max_payload_len 1 250
 *
 * # Configure LoRa receiver (specify no arguments for usage).
 * lora rx_cfg 1 0 7 1 0 8 5 0 0 1 0 0 0 1
 *
 * # Print message on each receive.
 * lora_rx_verbose 1
 *
 * # Clear receive log
 * lora_rx_info clear
 *
 * # Keep receiving 50-byte packets until manual stop.
 * lora_rx_rpt 50
 *
 * # Display information about recent receives.
 * lora_rx_info
 *
 * ##### Transmitter
 * # Sit on a single channel.
 * lora set_freq 915000000
 *
 * # Allow 250-byte packets.
 * lora max_payload_len 1 250
 *
 * # Configure LoRa transceiver (specify no arguments for usage).
 * lora tx_cfg 1 14 0 0 7 1 8 0 1 0 0 0 3000
 *
 * # Send; size=50, count=5, interval=100ms.
 * lora_tx_rpt 50 5 100
 */

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "sysinit/sysinit.h"
#include "syscfg/syscfg.h"
#include "hal/hal_gpio.h"
#include "hal/hal_spi.h"
#include "bsp/bsp.h"
#include "os/os.h"
#include "node/radio.h"
#include "console/console.h"
#include "shell/shell.h"
#include "parse/parse.h"

struct lorashell_rx_entry {
    uint16_t size;
    int16_t rssi;
    int8_t snr;
};

static struct lorashell_rx_entry
    lorashell_rx_entries[MYNEWT_VAL(LORASHELL_NUM_RX_ENTRIES)];
static int lorashell_rx_entry_idx;
static int lorashell_rx_entry_cnt;

static int lorashell_rx_rpt;
static int lorashell_rx_size;
static int lorashell_rx_verbose;
static int lorashell_txes_pending;
static uint8_t lorashell_tx_size;
static uint32_t lorashell_tx_itvl; /* OS ticks. */

static int lorashell_rx_info_cmd(int argc, char **argv);
static int lorashell_rx_rpt_cmd(int argc, char **argv);
static int lorashell_rx_verbose_cmd(int argc, char **argv);
static int lorashell_tx_rpt_cmd(int argc, char **argv);

static void lorashell_print_last_rx(struct os_event *ev);

static struct shell_cmd lorashell_cli_cmds[] = {
    {
        .sc_cmd = "lora_rx_info",
        .sc_cmd_func = lorashell_rx_info_cmd,
    },
    {
        .sc_cmd = "lora_rx_rpt",
        .sc_cmd_func = lorashell_rx_rpt_cmd,
    },
    {
        .sc_cmd = "lora_rx_verbose",
        .sc_cmd_func = lorashell_rx_verbose_cmd,
    },
    {
        .sc_cmd = "lora_tx_rpt",
        .sc_cmd_func = lorashell_tx_rpt_cmd,
    },
};
#define LORASHELL_NUM_CLI_CMDS  \
    (sizeof lorashell_cli_cmds / sizeof lorashell_cli_cmds[0])

static struct os_event lorashell_print_last_rx_ev = {
    .ev_cb = lorashell_print_last_rx,
};
static struct os_callout lorashell_tx_timer;

static const uint8_t lorashell_payload[UINT8_MAX] = {
          0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
    0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
    0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
    0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
    0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
    0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
    0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
    0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
    0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
    0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
    0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
    0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
    0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
    0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
    0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
    0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
    0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
    0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
    0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
    0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff,
};

static void
lorashell_rx_rpt_begin(void)
{
    Radio.Rx(0);
}

static void
lorashell_tx_timer_exp(struct os_event *ev)
{
    if (lorashell_txes_pending <= 0) {
        Radio.Sleep();
        return;
    }
    lorashell_txes_pending--;

    Radio.Send((void *)lorashell_payload, lorashell_tx_size);
}

static const char *
lorashell_rx_entry_str(const struct lorashell_rx_entry *entry)
{
    static char buf[32];

    snprintf(buf, sizeof buf, "size=%-4d rssi=%-4d snr=%-4d",
             entry->size, entry->rssi, entry->snr);
    return buf;
}

static void
lorashell_tx_timer_reset(void)
{
    int rc;

    rc = os_callout_reset(&lorashell_tx_timer, lorashell_tx_itvl);
    assert(rc == 0);
}

static void
on_tx_done(void)
{
    if (lorashell_txes_pending <= 0) {
        Radio.Sleep();
    } else {
        lorashell_tx_timer_reset();
    }
}

static void
on_rx_done(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr)
{
    struct lorashell_rx_entry *entry;

    if (lorashell_rx_size != 0) {
        if (size != lorashell_rx_size ||
            memcmp(payload, lorashell_payload, size) != 0) {

            /* Packet error. */
            goto done;
        }
    }

    entry = lorashell_rx_entries + lorashell_rx_entry_idx;
    entry->size = size;
    entry->rssi = rssi;
    entry->snr = snr;

    if (lorashell_rx_verbose) {
        os_eventq_put(os_eventq_dflt_get(), &lorashell_print_last_rx_ev);
    }

    lorashell_rx_entry_idx++;
    if (lorashell_rx_entry_idx >= MYNEWT_VAL(LORASHELL_NUM_RX_ENTRIES)) {
        lorashell_rx_entry_idx = 0;
    }

    if (lorashell_rx_entry_cnt < MYNEWT_VAL(LORASHELL_NUM_RX_ENTRIES)) {
        lorashell_rx_entry_cnt++;
    }

done:
    Radio.Sleep();
    if (lorashell_rx_rpt) {
        lorashell_rx_rpt_begin();
    }
}

static void
on_tx_timeout(void)
{
    assert(0);
    lorashell_tx_timer_reset();
}

static void
on_rx_timeout(void)
{
    Radio.Sleep();
}

static void
on_rx_error(void)
{
    Radio.Sleep();
}

static void
lorashell_print_last_rx(struct os_event *ev)
{
    const struct lorashell_rx_entry *entry;
    int idx;

    idx = lorashell_rx_entry_idx - 1;
    if (idx < 0) {
        idx = lorashell_rx_entry_cnt - 1;
    }

    entry = lorashell_rx_entries + idx;
    console_printf("rxed lora packet: %s\n", lorashell_rx_entry_str(entry));
}

static void
lorashell_avg_rx_entry(struct lorashell_rx_entry *out_entry)
{
    long long rssi_sum;
    long long size_sum;
    long long snr_sum;
    int i;

    rssi_sum = 0;
    size_sum = 0;
    snr_sum = 0;
    for (i = 0; i < lorashell_rx_entry_cnt; i++) {
        rssi_sum += lorashell_rx_entries[i].rssi;
        size_sum += lorashell_rx_entries[i].size;
        snr_sum += lorashell_rx_entries[i].snr;
    }

    memset(out_entry, 0, sizeof *out_entry);
    if (lorashell_rx_entry_cnt > 0) {
        out_entry->size = size_sum / lorashell_rx_entry_cnt;
        out_entry->rssi = rssi_sum / lorashell_rx_entry_cnt;
        out_entry->snr = snr_sum / lorashell_rx_entry_cnt;
    }
}

static int
lorashell_rx_rpt_cmd(int argc, char **argv)
{
    const char *err;
    int rc;

    if (argc > 1) {
        if (strcmp(argv[1], "stop") == 0) {
            lorashell_rx_rpt = 0;
            Radio.Sleep();
            console_printf("lora rx stopped\n");
            return 0;
        }

        lorashell_rx_size = parse_ull_bounds(argv[1], 0, UINT8_MAX, &rc);
        if (rc != 0) {
            err = "invalid size";
            goto err;
        }
    } else {
        lorashell_rx_size = 0;
    }

    lorashell_rx_rpt = 1;
    lorashell_rx_rpt_begin();

    return 0;

err:
    if (err != NULL) {
        console_printf("error: %s\n", err);
    }

    console_printf(
"usage:\n"
"    lora_rx_rpt [size]\n"
"    lora_rx_rpt stop\n");

    return rc;
}

static int
lorashell_rx_verbose_cmd(int argc, char **argv)
{
    int rc;

    if (argc <= 1) {
        console_printf("lora rx verbose: %d\n", lorashell_rx_verbose);
        return 0;
    }

    lorashell_rx_verbose = parse_ull_bounds(argv[1], 0, 1, &rc);
    if (rc != 0) {
        console_printf("error: rc=%d\n", rc);
        return rc;
    }

    return 0;
}

static int
lorashell_rx_info_cmd(int argc, char **argv)
{
    const struct lorashell_rx_entry *entry;
    struct lorashell_rx_entry avg;
    int idx;
    int i;

    if (argc > 1 && argv[1][0] == 'c') {
        lorashell_rx_entry_idx = 0;
        lorashell_rx_entry_cnt = 0;
        console_printf("lora rx info cleared\n");
        return 0;
    }

    if (lorashell_rx_entry_cnt < MYNEWT_VAL(LORASHELL_NUM_RX_ENTRIES)) {
        idx = 0;
    } else {
        idx = lorashell_rx_entry_idx;
    }

    console_printf("entries in log: %d\n", lorashell_rx_entry_cnt);

    for (i = 0; i < lorashell_rx_entry_cnt; i++) {
        entry = lorashell_rx_entries + idx;
        console_printf("%4d: %s\n", i + 1, lorashell_rx_entry_str(entry));

        idx++;
        if (idx >= MYNEWT_VAL(LORASHELL_NUM_RX_ENTRIES)) {
            idx = 0;
        }
    }

    if (lorashell_rx_entry_cnt > 0) {
        lorashell_avg_rx_entry(&avg);
        console_printf(" avg: %s\n", lorashell_rx_entry_str(&avg));
    }

    return 0;
}

static int
lorashell_tx_rpt_cmd(int argc, char **argv)
{
    const char *err;
    uint32_t itvl_ms;
    int rc;

    if (argc < 1) {
        rc = 1;
        err = NULL;
        goto err;
    }

    if (strcmp(argv[1], "stop") == 0) {
        lorashell_txes_pending = 0;
        Radio.Sleep();
        console_printf("lora tx stopped\n");
        return 0;
    }

    lorashell_tx_size = parse_ull_bounds(argv[1], 0, UINT8_MAX, &rc);
    if (rc != 0) {
        err = "invalid size";
        goto err;
    }

    if (argc >= 2) {
        lorashell_txes_pending = parse_ull_bounds(argv[2], 0, INT_MAX, &rc);
        if (rc != 0) {
            err = "invalid count";
            goto err;
        }
    } else {
        lorashell_txes_pending = 1;
    }

    if (argc >= 3) {
        itvl_ms = parse_ull_bounds(argv[3], 0, UINT32_MAX, &rc);
        if (rc != 0) {
            err = "invalid interval";
            goto err;
        }
    } else {
        itvl_ms = 1000;
    }

    rc = os_time_ms_to_ticks(itvl_ms, &lorashell_tx_itvl);
    if (rc != 0) {
        err = "invalid interval";
        goto err;
    }

    lorashell_tx_timer_exp(NULL);

    return 0;

err:
    if (err != NULL) {
        console_printf("error: %s\n", err);
    }

    console_printf(
"usage:\n"
"    lora_tx_rpt <size> [count] [interval (ms)]\n"
"    lora_tx_rpt stop\n");

    return rc;
}

int
main(void)
{
    RadioEvents_t radio_events;
    int rc;
    int i;

#ifdef ARCH_sim
    mcu_sim_parse_args(argc, argv);
#endif

    sysinit();

    for (i = 0; i < LORASHELL_NUM_CLI_CMDS; i++) {
        rc = shell_cmd_register(lorashell_cli_cmds + i);
        SYSINIT_PANIC_ASSERT_MSG(
            rc == 0, "Failed to register lorashell CLI commands");
    }

    os_callout_init(&lorashell_tx_timer, os_eventq_dflt_get(),
                    lorashell_tx_timer_exp, NULL);

    /* Radio initialization. */
    radio_events.TxDone = on_tx_done;
    radio_events.RxDone = on_rx_done;
    radio_events.TxTimeout = on_tx_timeout;
    radio_events.RxTimeout = on_rx_timeout;
    radio_events.RxError = on_rx_error;

    Radio.Init(&radio_events);
    Radio.SetMaxPayloadLength(MODEM_LORA, 250);

    console_printf("lorashell\n");

    /*
     * As the last thing, process events from default event queue.
     */
    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
}
