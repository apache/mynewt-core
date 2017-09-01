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

#ifndef H_LORA_
#define H_LORA_

#include "stats/stats.h"
#include "syscfg/syscfg.h"
#include "node/mac/LoRaMac.h"

STATS_SECT_START(lora_mac_stats)
    STATS_SECT_ENTRY(join_req_tx)
    STATS_SECT_ENTRY(join_accept_rx)
    STATS_SECT_ENTRY(link_chk_tx)
    STATS_SECT_ENTRY(link_chk_ans_rxd)
    STATS_SECT_ENTRY(join_failures)
    STATS_SECT_ENTRY(joins)
    STATS_SECT_ENTRY(tx_timeouts)
    STATS_SECT_ENTRY(unconfirmed_tx)
    STATS_SECT_ENTRY(confirmed_tx_fail)
    STATS_SECT_ENTRY(confirmed_tx_good)
    STATS_SECT_ENTRY(rx_errors)
    STATS_SECT_ENTRY(rx_frames)
    STATS_SECT_ENTRY(rx_mic_failures)
    STATS_SECT_ENTRY(rx_mlme)
    STATS_SECT_ENTRY(rx_mcps)
STATS_SECT_END
extern STATS_SECT_DECL(lora_mac_stats) lora_mac_stats;

STATS_SECT_START(lora_stats)
    STATS_SECT_ENTRY(rx_error)
    STATS_SECT_ENTRY(rx_success)
    STATS_SECT_ENTRY(rx_timeout)
    STATS_SECT_ENTRY(tx_success)
    STATS_SECT_ENTRY(tx_timeout)
STATS_SECT_END
extern STATS_SECT_DECL(lora_stats) lora_stats;

/* XXX: for now. Maybe have api to set these? */
#define LORA_EUI_LEN        (8)
#define LORA_KEY_LEN        (16)
extern uint8_t g_lora_dev_eui[LORA_EUI_LEN];
extern uint8_t g_lora_app_eui[LORA_EUI_LEN];
extern uint8_t g_lora_app_key[LORA_KEY_LEN];

/* Received packet information */
struct lora_rx_info
{
    /*!
     * Downlink datarate
     */
    uint8_t rxdatarate;

    /*!
     * Snr of the received packet
     */
    uint8_t snr;

    /*!
     * Frame pending status
     */
    uint8_t frame_pending: 1;

    /*!
     * Receive window
     *
     * [0: Rx window 1, 1: Rx window 2]
     */
    uint8_t rxslot: 1;

    /*!
     * Set if an acknowledgement was received
     */
    uint8_t ack_rxd: 1;

    /*!
     * Indicates, if data is available
     */
    uint8_t rxdata: 1;

    /*!
     * Multicast
     */
    uint8_t multicast: 1;

    /*!
     * Rssi of the received packet
     */
    int16_t rssi;

    /*!
     * The downlink counter value for the received frame
     */
    uint32_t downlink_cntr;
};

/* Transmitted packet information */
struct lora_txd_info
{
    /*!
     * Uplink datarate
     */
    uint8_t datarate;

    /*!
     * Transmission power
     */
    int8_t txpower;

    /*!
     * Provides the number of retransmissions
     */
    uint8_t retries;

    /*!
     * Set if an acknowledgement was received
     */
    uint8_t ack_rxd: 1;

    /*!
     * The transmission time on air of the frame
     */
    uint32_t tx_time_on_air;

    /*!
     * The uplink counter value related to the frame
     */
    uint32_t uplink_cntr;

    /*!
     * The uplink frequency related to the frame
     */
    uint32_t uplink_freq;
};

/*
 * Lora packet header information. This is the user header portion of a lora
 * packet.
 */
struct lora_pkt_info
{
    uint8_t port;
    Mcps_t pkt_type;
    LoRaMacEventInfoStatus_t status;

    union {
        struct lora_rx_info rxdinfo;
        struct lora_txd_info txdinfo;
    };
};

/* Allocate a packet for lora transmission. This returns a packet header mbuf */
struct os_mbuf *lora_pkt_alloc(void);

/* Given a pointer to a packet header mbuf chain, obtain pointer to lora info */
#define LORA_PKT_INFO_PTR(om)            \
    (struct lora_pkt_info *)((uint8_t *)om + sizeof(struct os_mbuf) + \
                            sizeof(struct os_mbuf_pkthdr))

/* Port API */
typedef void (*lora_txd_func)(uint8_t port, LoRaMacEventInfoStatus_t status,
                              Mcps_t pkt_type, struct os_mbuf *om);

/* Received data callback. Mbuf must be freed by this function */
typedef void (*lora_rxd_func)(uint8_t port, LoRaMacEventInfoStatus_t status,
                              Mcps_t pkt_type, struct os_mbuf *om);

/**
 * Open a lora application port. This function will allocate a lora port, set
 * port default values for datarate and retries, set the transmit done and
 * received data callbacks, and add port to list of open ports.
 *
 * @param port      Port number. Valid range: 1 to 223, inclusive.
 * @param txd_cb    Transmit done callback
 * @param rxd_cb    Receive data callback
 *
 * @return int A return code from set of lora return codes
 */
int lora_app_port_open(uint8_t port, lora_txd_func txd_cb, lora_rxd_func rxd_cb);

/**
 * Close an open lora port
 *
 * @param port Port number
 *
 * @return int A return code from set of lora return codes
 */
int lora_app_port_close(uint8_t port);

/**
 * Configure an application port. This configures the datarate for uplink
 * packets and the number of retries for confirmed packets.
 *
 * @param port Port number
 * @param datarate Data rate for uplink packets
 * @param retries NUmmber of retries for confirmed packets
 *
 * @return int A return code from set of lora return codes
 */
int lora_app_port_cfg(uint8_t port, uint8_t datarate, uint8_t retries);

/**
 * Send a packet on a port.
 *
 * @param port Port number
 * @param pkt_type Type of packet
 * @param om Pointer to packet
 *
 * @return int A return code from set of lora return codes
 */
int lora_app_port_send(uint8_t port, Mcps_t pkt_type, struct os_mbuf *om);

/* Join callback proto */
typedef void (*lora_join_cb)(LoRaMacEventInfoStatus_t status, uint8_t attempts);
extern lora_join_cb lora_join_cb_func;

/* Link check callback proto */
typedef void (*lora_link_chk_cb)(LoRaMacEventInfoStatus_t status,
                                 uint8_t num_gw, uint8_t demod_margin);
extern lora_link_chk_cb lora_link_chk_cb_func;

/*
 * The following API are available if the system configuration variable
 * LORA_APP_AUTO_JOIN is set to 0. If LORA_APP_AUTO_JOIN is set to 1,
 * joining will be handled by the stack and not the application.
 */
#if !MYNEWT_VAL(LORA_APP_AUTO_JOIN)
/**
 *  Join a lora network. When called this function will attempt to join
 *  if the end device is not already joined. Join status (success, failure)
 *  will be reported through the callback.
 *
 * @param dev_eui   Pointer to device EUI
 * @param app_eui   Pointer to Application EUI
 * @param app_key   Pointer to application key
 * @param trials    Number of join attempts before failure
 *
 * @return int Lora return code
 */
int lora_app_join(uint8_t *dev_eui, uint8_t *app_eui, uint8_t *app_key,
                  uint8_t trials);

/* Performs a link check */
int lora_app_link_check(void);

#endif

/**
 * Set the join callback. This will be called when joining succeeds or fails.
 *
 * @param join_cb Pointer to join callback function.
 *
 * @return int Lora return code
 */
int lora_app_set_join_cb(lora_join_cb join_cb);

/**
 * set the link check callback.
 *
 * @param link_chk_cb
 *
 * @return int
 */
int lora_app_set_link_check_cb(lora_link_chk_cb link_chk_cb);

/* Lora app return codes */
#define LORA_APP_STATUS_OK              (0)
#define LORA_APP_STATUS_NO_PORT         (1)
#define LORA_APP_STATUS_INVALID_PARAM   (2)
#define LORA_APP_STATUS_ENOMEM          (3)
#define LORA_APP_STATUS_INVALID_PORT    (4)
#define LORA_APP_STATUS_ALREADY_OPEN    (5)
#define LORA_APP_STATUS_JOIN_FAILURE    (6)
#define LORA_APP_STATUS_ALREADY_JOINED  (7)
#define LORA_APP_STATUS_NO_NETWORK      (8)

#endif
