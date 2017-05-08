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
 * This file contains code to collect and print receive statistics to the
 * console.
 */

#include <assert.h>
#include <string.h>
#include <stdint.h>

#include "console/console.h"
#include "loraping.h"

#define LORAPING_NUM_RXINFOS                10

struct loraping_rxinfo {
    int8_t rssi;
    int8_t snr;

    uint8_t rxed:1;
};

static struct loraping_rxinfo loraping_rxinfos[LORAPING_NUM_RXINFOS];
static int loraping_rxinfo_idx;
static int loraping_rxinfo_rollover;

static void
loraping_rxinfo_avg(struct loraping_rxinfo *out_info, int *out_pkt_loss)
{
    long long rssi_sum;
    long long snr_sum;
    int num_rxed;
    int count;
    int i;

    if (!loraping_rxinfo_rollover) {
        count = loraping_rxinfo_idx;
    } else {
        count = LORAPING_NUM_RXINFOS;
    }

    assert(count > 0);

    rssi_sum = 0;
    snr_sum = 0;
    num_rxed = 0;
    for (i = 0; i < count; i++) {
        if (loraping_rxinfos[i].rxed) {
            num_rxed++;
            rssi_sum += loraping_rxinfos[i].rssi;
            snr_sum += loraping_rxinfos[i].snr;
        }
    }

    memset(out_info, 0, sizeof *out_info);
    if (num_rxed > 0) {
        out_info->rssi = rssi_sum / num_rxed;
        out_info->snr = snr_sum / num_rxed;
    }

    *out_pkt_loss = (count - num_rxed) * 10000 / count;
}

static void
loraping_rxinfo_inc_idx(void)
{
    loraping_rxinfo_idx++;
    if (loraping_rxinfo_idx >= LORAPING_NUM_RXINFOS) {
        loraping_rxinfo_idx = 0;
        loraping_rxinfo_rollover = 1;
    }
}

void
loraping_rxinfo_print(void)
{
    const struct loraping_rxinfo *last;
    struct loraping_rxinfo avg;
    int last_idx;
    int pkt_loss;
    int width;

    last_idx = loraping_rxinfo_idx - 1;
    if (last_idx < 0) {
        last_idx += LORAPING_NUM_RXINFOS;
    }
    last = loraping_rxinfos + last_idx;

    loraping_rxinfo_avg(&avg, &pkt_loss);

    if (last->rxed) {
        width = console_printf("[LAST] rssi=%-4d snr=%-4d",
                               last->rssi, last->snr);
    } else {
        width = console_printf("[LAST] TIMEOUT");
    }

    for (; width < 48; width++) {
        console_printf(" ");
    }

    console_printf("[AVG-%d] rssi=%-4d snr=%-4d pkt_loss=%d.%02d%%\n",
                   LORAPING_NUM_RXINFOS, avg.rssi, avg.snr,
                   pkt_loss / 100, pkt_loss % 100);
}

void
loraping_rxinfo_timeout(void)
{
    loraping_rxinfos[loraping_rxinfo_idx].rxed = 0;
    loraping_rxinfo_inc_idx();
}

void
loraping_rxinfo_rxed(int8_t rssi, int8_t snr)
{
    loraping_rxinfos[loraping_rxinfo_idx].rssi = rssi;
    loraping_rxinfos[loraping_rxinfo_idx].snr = snr;
    loraping_rxinfos[loraping_rxinfo_idx].rxed = 1;
    loraping_rxinfo_inc_idx();
}
