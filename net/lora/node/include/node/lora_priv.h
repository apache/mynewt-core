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

#ifndef H_LORA_PRIV_
#define H_LORA_PRIV_

#include "node/mac/LoRaMac.h"
#include "os/mynewt.h"
#include "node/lora.h"

/*
 * Lora MAC data object
 */
struct lora_mac_obj
{
    /* Task event queue */
    struct os_eventq lm_evq;

    /* Transmit queue */
    struct os_mqueue lm_txq;

    /* Join event */
    struct os_event lm_join_ev;

    /* Link check event */
    struct os_event lm_link_chk_ev;

    /* TODO: this is temporary until we figure out a better way to deal */
    /* Transmit queue timer */
    struct os_callout lm_txq_timer;

    /*
     * Pointer to the lora packet information of the packet being currently
     * transmitted.
     */
    struct lora_pkt_info *curtx;

    /*
     * Global lora tx packet info structure. Used when transmitting but no mbuf
     * was available for transmission.
     */
    struct lora_pkt_info txpkt;

    /* Pointer to current transmit mbuf. Can be NULL and still txing */
    struct os_mbuf *cur_tx_mbuf;

    /*
     * Global lora rx packet info structure. Used when receiving and prior to
     * obtaining a mbuf.
     */
    uint16_t rxbufsize;
    uint8_t *rxbuf;
    struct lora_pkt_info rxpkt;
};

extern struct lora_mac_obj g_lora_mac_data;

void lora_cli_init(void);
void lora_app_init(void);

struct os_mbuf;
void lora_app_mcps_indicate(struct os_mbuf *om);
void lora_app_mcps_confirm(struct os_mbuf *om);
void lora_app_join_confirm(LoRaMacEventInfoStatus_t status, uint8_t attempts);
void lora_app_link_chk_confirm(LoRaMacEventInfoStatus_t status, uint8_t num_gw,
                               uint8_t demod_margin);
void lora_node_mcps_request(struct os_mbuf *om);
void lora_node_mac_mcps_indicate(void);
int lora_node_join(uint8_t *dev_eui, uint8_t *app_eui, uint8_t *app_key,
                   uint8_t trials);
int lora_node_link_check(void);
int lora_node_mtu(void);
struct os_eventq *lora_node_mac_evq_get(void);
void lora_node_chk_txq(void);
bool lora_node_txq_empty(void);
bool lora_mac_srv_ack_requested(void);
uint8_t lora_mac_cmd_buffer_len(void);

/* Lora debug log */
#define LORA_NODE_DEBUG_LOG

#if defined(LORA_NODE_DEBUG_LOG)
struct lora_node_debug_log_entry
{
    uint8_t lnd_id;
    uint8_t lnd_p8;
    uint16_t lnd_p16;
    uint32_t lnd_p32;
    uint32_t lnd_cputime;
};

#define LORA_NODE_DEBUG_LOG_ENTRIES     (128)
void lora_node_log(uint8_t logid, uint8_t p8, uint16_t p16, uint32_t p32);

extern struct lora_node_debug_log_entry g_lnd_log[LORA_NODE_DEBUG_LOG_ENTRIES];
extern uint16_t g_lnd_log_index;

/* IDs */
#define LORA_NODE_LOG_UNUSED            (0)
#define LORA_NODE_LOG_TX_DONE           (10)
#define LORA_NODE_LOG_RX_WIN_SETUP      (20)
#define LORA_NODE_LOG_RX_TIMEOUT        (21)
#define LORA_NODE_LOG_RX_DONE           (22)
#define LORA_NODE_LOG_RX_SYNC_TIMEOUT   (23)
#define LORA_NODE_LOG_RADIO_TIMEOUT_IRQ (24)
#define LORA_NODE_LOG_RX_PORT           (25)
#define LORA_NODE_LOG_RX_WIN2           (26)
#define LORA_NODE_LOG_RX_WIN_SETUP_FAIL (27)
#define LORA_NODE_LOG_APP_TX            (40)
#define LORA_NODE_LOG_RX_ADR_REQ        (80)
#define LORA_NODE_LOG_PROC_MAC_CMD      (85)
#define LORA_NODE_LOG_LINK_CHK          (90)

#else
#define lora_node_log(a,b,c,d)
#endif

#endif
