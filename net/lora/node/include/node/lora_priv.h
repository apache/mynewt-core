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
#include "node/lora_band.h"

/* Connection state machine flags. */
union lora_mac_flags {
    struct {
        uint8_t gw_ack_req: 1;
        uint8_t node_ack_req: 1;
        uint8_t last_tx_join: 1;
        uint8_t is_joined: 1;
        uint8_t is_joining: 1;
        uint8_t is_public_nwk: 1;
        uint8_t is_mcps_req: 1;
        uint8_t repeater_supp: 1;
    } lmfbit;
    uint8_t lmf_all;
} __attribute__((packed));

/*
 * Lora MAC data object
 */
struct lora_mac_obj
{
    /* To limit # of join attempts */
    uint8_t cur_join_attempt;
    uint8_t max_join_attempt;

    /* Current tx payload (N) */
    uint8_t cur_tx_pyld;

    /* current and last transmit channel used */
    uint8_t cur_chan;
    uint8_t last_tx_chan;

    /*!
     * Maximum duty cycle
     * \remark Possibility to shutdown the device.
     */
    uint8_t max_dc;

    /*!
     * Number of trials to get a frame acknowledged
     */
    uint8_t ack_timeout_retries;

    /*!
     * Number of trials to get a frame acknowledged
     */
    uint8_t ack_timeout_retries_cntr;

    /*!
     * Uplink messages repetitions counter
     */
    uint8_t nb_rep_cntr;

    /*!
     * Device nonce is a random value extracted by issuing a sequence of RSSI
     * measurements
     */
    uint16_t dev_nonce;

    /*!
     * Aggregated duty cycle management
     */
    uint16_t aggr_dc;
    uint32_t aggr_last_tx_done_time;
    uint32_t aggr_time_off;

    /*!
     * LoRaMac reception windows delay
     * \remark normal frame: RxWindowXDelay = ReceiveDelayX - RADIO_WAKEUP_TIME
     *         join frame  : RxWindowXDelay = JoinAcceptDelayX - RADIO_WAKEUP_TIME
     */
    uint32_t rx_win1_delay;
    uint32_t rx_win2_delay;

    /* uplink and downlink counters */
    uint32_t uplink_cntr;
    uint32_t downlink_cntr;

    /*!
     * Last transmission time on air (in msecs)
     */
    uint32_t tx_time_on_air;

    /* Counts the number of missed ADR acknowledgements */
    uint32_t adr_ack_cntr;

    /*!
     * Network ID ( 3 bytes )
     */
    uint32_t netid;

    /*!
     * Mote Address
     */
    uint32_t dev_addr;

    /*!
     * Holds the current rx window slot
     */
    LoRaMacRxSlot_t rx_slot;

    /* Task event queue */
    struct os_eventq lm_evq;

    /* Transmit queue */
    struct os_mqueue lm_txq;

    /* Join event */
    struct os_event lm_join_ev;

    /* Link check event */
    struct os_event lm_link_chk_ev;

#define LORA_DELTA_SHIFT        3
#define LORA_AVG_SHIFT	        4
    /* Averaging of RSSI/SNR for received frames */
    int16_t lm_rssi_avg;
    int16_t lm_snr_avg;

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

    /*!
     * Retransmission timer. This is used for confirmed frames on both class
     * A and C devices and for unconfirmed transmissions on class C devices
     * (so that a class C device will not transmit before listening on the
     * second receive window).
     */
    struct hal_timer rtx_timer;

    /*
     * Global lora rx packet info structure. Used when receiving and prior to
     * obtaining a mbuf.
     */
    uint16_t rxbufsize;
    uint8_t *rxbuf;
    struct lora_pkt_info rxpkt;

    /* Flags */
    union lora_mac_flags lmflags;

    /*!
     * Stores the time at LoRaMac initialization.
     *
     * NOTE: units of this variable are in os time ticks
     *
     * \remark Used for the BACKOFF_DC computation.
     */
    os_time_t init_time;
};

extern struct lora_mac_obj g_lora_mac_data;

/* Flag macros */
#define LM_F_GW_ACK_REQ()       (g_lora_mac_data.lmflags.lmfbit.gw_ack_req)
#define LM_F_NODE_ACK_REQ()     (g_lora_mac_data.lmflags.lmfbit.node_ack_req)
#define LM_F_IS_JOINED()        (g_lora_mac_data.lmflags.lmfbit.is_joined)
#define LM_F_IS_JOINING()       (g_lora_mac_data.lmflags.lmfbit.is_joining)
#define LM_F_IS_PUBLIC_NWK()    (g_lora_mac_data.lmflags.lmfbit.is_public_nwk)
#define LM_F_IS_MCPS_REQ()      (g_lora_mac_data.lmflags.lmfbit.is_mcps_req)
#define LM_F_REPEATER_SUPP()    (g_lora_mac_data.lmflags.lmfbit.repeater_supp)
#define LM_F_LAST_TX_IS_JOIN_REQ() (g_lora_mac_data.lmflags.lmfbit.last_tx_join)

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
void lora_node_qual_sample(int16_t rssi, int16_t snr);

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
#define LORA_NODE_LOG_TX_SETUP          (11)
#define LORA_NODE_LOG_TX_START          (12)
#define LORA_NODE_LOG_TX_DELAY          (15)
#define LORA_NODE_LOG_TX_PREP_FRAME     (16)
#define LORA_NODE_LOG_RX_WIN1_SETUP     (20)
#define LORA_NODE_LOG_RX_TIMEOUT        (21)
#define LORA_NODE_LOG_RX_DONE           (22)
#define LORA_NODE_LOG_RADIO_TIMEOUT_IRQ (24)
#define LORA_NODE_LOG_RX_PORT           (25)
#define LORA_NODE_LOG_RX_WIN2           (26)
#define LORA_NODE_LOG_RX_CFG            (27)
#define LORA_NODE_LOG_APP_TX            (40)
#define LORA_NODE_LOG_RTX_TIMEOUT       (50)
#define LORA_NODE_LOG_RX_ADR_REQ        (80)
#define LORA_NODE_LOG_PROC_MAC_CMD      (85)
#define LORA_NODE_LOG_LINK_CHK          (90)

#else
#define lora_node_log(a,b,c,d)
#endif

#endif
