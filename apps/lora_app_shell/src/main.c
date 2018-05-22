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

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "os/mynewt.h"
#include "hal/hal_gpio.h"
#include "hal/hal_spi.h"
#include "bsp/bsp.h"
#include "console/console.h"
#include "shell/shell.h"
#include "parse/parse.h"
#include "node/lora_priv.h"
#include "node/lora.h"
#include "oic/oc_api.h"

extern void las_cmd_init(void);
extern void las_cmd_disp_byte_str(uint8_t *bytes, int len);

/* XXX: should we count statistics for the app shell? You know, things
   like transmitted, etc, etc. */
void
lora_app_shell_txd_func(uint8_t port, LoRaMacEventInfoStatus_t status,
                        Mcps_t pkt_type, struct os_mbuf *om)
{
    struct lora_pkt_info *lpkt;

    assert(om != NULL);
    console_printf("Txd on port %u type=%s status=%d len=%u\n",
                   port, pkt_type == MCPS_CONFIRMED ? "conf" : "unconf",
                   status, OS_MBUF_PKTLEN(om));

    lpkt = LORA_PKT_INFO_PTR(om);
    console_printf("\tdr:%u\n", lpkt->txdinfo.datarate);
    console_printf("\ttxpower (dbm):%d\n", lpkt->txdinfo.txpower);
    console_printf("\ttries:%u\n", lpkt->txdinfo.retries);
    console_printf("\tack_rxd:%u\n", lpkt->txdinfo.ack_rxd);
    console_printf("\ttx_time_on_air:%lu\n", lpkt->txdinfo.tx_time_on_air);
    console_printf("\tuplink_cntr:%lu\n", lpkt->txdinfo.uplink_cntr);
    console_printf("\tuplink_chan:%lu\n", lpkt->txdinfo.uplink_chan);

    os_mbuf_free_chain(om);
}

void
lora_app_shell_rxd_func(uint8_t port, LoRaMacEventInfoStatus_t status,
                        Mcps_t pkt_type, struct os_mbuf *om)
{
    int rc;
    struct lora_pkt_info *lpkt;
    uint16_t cur_len;
    uint16_t len;
    uint16_t cur_off;
    uint8_t temp[16];

    assert(om != NULL);
    console_printf("Rxd on port %u type=%s status=%d len=%u\n",
                   port, pkt_type == MCPS_CONFIRMED ? "conf" : "unconf",
                   status, OS_MBUF_PKTLEN(om));

    lpkt = LORA_PKT_INFO_PTR(om);
    console_printf("\trxdr:%u\n", lpkt->rxdinfo.rxdatarate);
    console_printf("\tsnr:%u\n", lpkt->rxdinfo.snr);
    console_printf("\trssi:%d\n", lpkt->rxdinfo.rssi);
    console_printf("\trxslot:%u\n", lpkt->rxdinfo.rxslot);
    console_printf("\tack_rxd:%u\n", lpkt->rxdinfo.ack_rxd);
    console_printf("\trxdata:%u\n", lpkt->rxdinfo.rxdata);
    console_printf("\tmulticast:%u\n", lpkt->rxdinfo.multicast);
    console_printf("\tfp:%u\n", lpkt->rxdinfo.frame_pending);
    console_printf("\tdownlink_cntr:%lu\n", lpkt->rxdinfo.downlink_cntr);

    /* Dump the bytes received */
    len = OS_MBUF_PKTLEN(om);
    if (len != 0) {
        console_printf("Rxd data:\n");
        cur_off = 0;
        while (cur_off < len) {
            cur_len = len - cur_off;
            if (cur_len > 16) {
                cur_len = 16;
            }
            rc = os_mbuf_copydata(om, cur_off, cur_len, temp);
            if (rc) {
                break;
            }
            cur_off += cur_len;
            las_cmd_disp_byte_str(temp, cur_len);
        }
    }

    os_mbuf_free_chain(om);
}

void
lora_app_shell_join_cb(LoRaMacEventInfoStatus_t status, uint8_t attempts)
{
    console_printf("Join cb. status=%d attempts=%u\n", status, attempts);
}

void
lora_app_shell_link_chk_cb(LoRaMacEventInfoStatus_t status, uint8_t num_gw,
                           uint8_t demod_margin)
{
    console_printf("Link check cb. status=%d num_gw=%u demod_margin=%u\n",
                   status, num_gw, demod_margin);
}

static void
oic_app_init(void)
{
    oc_init_platform("MyNewt", NULL, NULL);
    oc_add_device("/oic/d", "oic.d.light", "MynewtLed", "1.0", "1.0", NULL,
                  NULL);
}

static const oc_handler_t omgr_oc_handler = {
    .init = oic_app_init,
};

int
main(void)
{
#ifdef ARCH_sim
    mcu_sim_parse_args(argc, argv);
#endif

    sysinit();

    console_printf("\n");
    console_printf("lora_app_shell\n");
    las_cmd_init();

    oc_main_init((oc_handler_t *)&omgr_oc_handler);

    /*
     * As the last thing, process events from default event queue.
     */
    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
}
