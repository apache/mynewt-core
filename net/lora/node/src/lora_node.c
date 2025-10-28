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

#include <string.h>
#include "os/mynewt.h"
#include "node/lora.h"
#include "node/lora_priv.h"
#include "node/lora_band.h"

STATS_SECT_DECL(lora_mac_stats) lora_mac_stats;
STATS_NAME_START(lora_mac_stats)
    STATS_NAME(lora_mac_stats, join_req_tx)
    STATS_NAME(lora_mac_stats, join_accept_rx)
    STATS_NAME(lora_mac_stats, link_chk_tx)
    STATS_NAME(lora_mac_stats, link_chk_ans_rxd)
    STATS_NAME(lora_mac_stats, join_failures)
    STATS_NAME(lora_mac_stats, joins)
    STATS_NAME(lora_mac_stats, tx_timeouts)
    STATS_NAME(lora_mac_stats, unconfirmed_tx)
    STATS_NAME(lora_mac_stats, confirmed_tx_fail)
    STATS_NAME(lora_mac_stats, confirmed_tx_good)
    STATS_NAME(lora_mac_stats, tx_mac_flush)
    STATS_NAME(lora_mac_stats, rx_errors)
    STATS_NAME(lora_mac_stats, rx_frames)
    STATS_NAME(lora_mac_stats, rx_mic_failures)
    STATS_NAME(lora_mac_stats, rx_mlme)
    STATS_NAME(lora_mac_stats, rx_mcps)
    STATS_NAME(lora_mac_stats, rx_dups)
    STATS_NAME(lora_mac_stats, rx_invalid)
    STATS_NAME(lora_mac_stats, no_bufs)
    STATS_NAME(lora_mac_stats, already_joined)
STATS_NAME_END(lora_mac_stats)

/* Device EUI */
uint8_t g_lora_dev_eui[LORA_EUI_LEN];

/* Application EUI */
uint8_t g_lora_app_eui[LORA_EUI_LEN];

/* Application Key */
uint8_t g_lora_app_key[LORA_KEY_LEN];

/* Flag to denote if we last sent a mac command */
uint8_t g_lora_node_last_tx_mac_cmd;

/* Used for LoRaMacInitialization(); */
#if !MYNEWT_VAL(LORA_NODE_CLI)
    LoRaMacCallback_t lora_cb;
#endif

/* MAC task */
#define LORA_MAC_STACK_SIZE   (256)
struct os_task g_lora_mac_task;
os_stack_t g_lora_mac_stack[LORA_MAC_STACK_SIZE];

/*
 * Lora MAC data object
 */
struct lora_mac_obj g_lora_mac_data;

/* The join event argument */
struct lm_join_ev_arg_obj
{
    uint8_t trials;
    uint8_t *dev_eui;
    uint8_t *app_eui;
    uint8_t *app_key;
};
struct lm_join_ev_arg_obj g_lm_join_ev_arg;

/* Debug log */
#if defined(LORA_NODE_DEBUG_LOG)
struct lora_node_debug_log_entry g_lnd_log[LORA_NODE_DEBUG_LOG_ENTRIES];
uint16_t g_lnd_log_index;

/* Low power callback functions */
extern void lora_bsp_enable_mac_timer(void);

void
lora_node_log(uint8_t logid, uint8_t p8, uint16_t p16, uint32_t p32)
{
    os_sr_t sr;

    OS_ENTER_CRITICAL(sr);
    g_lnd_log[g_lnd_log_index].lnd_id = logid;
    g_lnd_log[g_lnd_log_index].lnd_p8 = p8;
    g_lnd_log[g_lnd_log_index].lnd_p16 = p16;
    g_lnd_log[g_lnd_log_index].lnd_p32 = p32;
    g_lnd_log[g_lnd_log_index].lnd_cputime = TimerGetCurrentTime();

    ++g_lnd_log_index;
    if (g_lnd_log_index == LORA_NODE_DEBUG_LOG_ENTRIES) {
        g_lnd_log_index = 0;
    }
    OS_EXIT_CRITICAL(sr);
}
#endif  /* if defined(LORA_NODE_DEBUG_LOG) */

/* Allocate a packet for lora transmission. This returns a packet header mbuf */
struct os_mbuf *
lora_pkt_alloc(void)
{
    struct os_mbuf *p;

    /* XXX: For now just allocate 255 bytes */
    p = os_msys_get_pkthdr(255, sizeof(struct lora_pkt_info));
    return p;
}

/**
 * This is the application to mac layer transmit interface.
 *
 * @param om Pointer to transmit packet
 */
void
lora_node_mcps_request(struct os_mbuf *om)
{
    int rc;

    lora_node_log(LORA_NODE_LOG_APP_TX, 0, OS_MBUF_PKTLEN(om), (uint32_t)om);
    rc = os_mqueue_put(&g_lora_mac_data.lm_txq, &g_lora_mac_data.lm_evq, om);
    assert(rc == 0);
}

/**
 * What's the maximum payload which can be sent on next frame
 *
 * @return int payload length, negative on error.
 */
int
lora_node_mtu(void)
{
    struct sLoRaMacTxInfo info;
    int rc;

    rc = LoRaMacQueryTxPossible(0, &info);
    if (rc != LORAMAC_STATUS_MAC_CMD_LENGTH_ERROR) {
        return info.MaxPossiblePayload;
    }
    return -1;
}

#if !MYNEWT_VAL(LORA_NODE_CLI)
static void
lora_node_reset_txq_timer(void)
{
    /* XXX: For now, just reset timer to fire off in one second */
    os_callout_reset(&g_lora_mac_data.lm_txq_timer, OS_TICKS_PER_SEC);
}

/**
 * Posts an event to the lora mac task to check the transmit queue for
 * more packets.
 */
void
lora_node_chk_txq(void)
{
    os_eventq_put(&g_lora_mac_data.lm_evq, &g_lora_mac_data.lm_txq.mq_ev);
}

bool
lora_node_txq_empty(void)
{
    bool rc;
    struct os_mbuf_pkthdr *mp;

    mp = STAILQ_FIRST(&g_lora_mac_data.lm_txq.mq_head);
    if (mp == NULL) {
        rc = true;
    } else {
        rc = false;
    }

    return rc;
}

/**
 * lora node mac mcps indicate
 *
 * MAC indication handler
 */
void
lora_node_mac_mcps_indicate(void)
{
    int rc;
    struct os_mbuf *om;
    struct lora_pkt_info *lpkt;

    /*
     * Not sure if this is possible, but port 0 is not a valid application port.
     * If the port is 0 do not send indicate
     */
    if (g_lora_mac_data.rxpkt.port == 0) {
        /* XXX: count a statistic? */
        return;
    }

    om = lora_pkt_alloc();
    if (om) {
        /* Copy data into mbuf */
        rc = os_mbuf_copyinto(om, 0, g_lora_mac_data.rxbuf,
                              g_lora_mac_data.rxbufsize);
        if (rc) {
            os_mbuf_free_chain(om);
            return;
        }

        /* Set lora packet info */
        lpkt = LORA_PKT_INFO_PTR(om);
        memcpy(lpkt, &g_lora_mac_data.rxpkt, sizeof(struct lora_pkt_info));
        lora_app_mcps_indicate(om);
    } else {
        /* XXX: cant do anything until the lower stack gets modified */
        STATS_INC(lora_mac_stats, no_bufs);
    }
}

static uint8_t
lora_node_get_batt_status(void)
{
    /* NOTE: 0 means connected to external power supply. */
    return 0;
}


/**
 * Process transmit enqueued event
 *
 * @param ev Pointer to transmit enqueue event
 */
static void
lora_mac_proc_tx_q_event(struct os_event *ev)
{
    LoRaMacStatus_t rc;
    LoRaMacEventInfoStatus_t evstatus;
    LoRaMacTxInfo_t txinfo;
    struct lora_pkt_info *lpkt;
    struct os_mbuf *om;
    struct os_mbuf_pkthdr *mp;

    /* Stop the transmit callback because something was just queued */
    os_callout_stop(&g_lora_mac_data.lm_txq_timer);

    /* If busy just leave */
    if (lora_mac_tx_state() == LORAMAC_STATUS_BUSY) {
        /* XXX: this should not be needed */
        lora_node_reset_txq_timer();
        return;
    }

    /*
     * Check if possible to send frame. If a MAC command length error we
     * need to send an empty, unconfirmed frame to flush mac commands.
     */
    lpkt = NULL;
    while (1) {
        mp = STAILQ_FIRST(&g_lora_mac_data.lm_txq.mq_head);
        if (mp == NULL) {
            /* If an ack has been requested, send one */
            if (lora_mac_srv_ack_requested()) {
                g_lora_node_last_tx_mac_cmd = 0;
                goto send_empty_msg;
            } else {
                /* Check for any mac commands */
                if (lora_mac_cmd_buffer_len() != 0) {
                    g_lora_node_last_tx_mac_cmd = 1;
                    goto send_empty_msg;
                }
            }
            break;
        }

        rc = LoRaMacQueryTxPossible(mp->omp_len, &txinfo);
        if (rc == LORAMAC_STATUS_MAC_CMD_LENGTH_ERROR) {
            /*
             * XXX: an ugly hack for now. If the server decides to send MAC
             * commands all the time, it could be that we never send the
             * data packet enqueued as we cannot add more data to it. For now,
             * just alternate between sending the packet on the queue and
             * a mac command. Yes, ugly.
             */
            if (g_lora_node_last_tx_mac_cmd) {
                rc = LORAMAC_STATUS_OK;
                goto send_from_txq;
            }

            g_lora_node_last_tx_mac_cmd = 1;

            /* Need to flush MAC commands. Send empty unconfirmed frame */
            STATS_INC(lora_mac_stats, tx_mac_flush);
            /* NOTE: no need to get a mbuf. */
send_empty_msg:
            lpkt = &g_lora_mac_data.txpkt;
            g_lora_mac_data.curtx = lpkt;
            om = NULL;
            memset(lpkt, 0, sizeof(struct lora_pkt_info));
            lpkt->pkt_type = MCPS_UNCONFIRMED;
            rc = LORAMAC_STATUS_OK;
        } else {
send_from_txq:
            om = os_mqueue_get(&g_lora_mac_data.lm_txq);
            assert(om != NULL);
            lpkt = LORA_PKT_INFO_PTR(om);
            g_lora_mac_data.curtx = lpkt;
            g_lora_node_last_tx_mac_cmd = 0;
        }

        g_lora_mac_data.cur_tx_mbuf = om;

        if (rc != LORAMAC_STATUS_OK) {
            /* Check if length error or mac command error */
            if (rc == LORAMAC_STATUS_LENGTH_ERROR) {
                evstatus = LORAMAC_EVENT_INFO_STATUS_TX_DR_PAYLOAD_SIZE_ERROR;
            } else {
                evstatus = LORAMAC_EVENT_INFO_STATUS_ERROR;
            }
            goto proc_txq_om_done;
        }

        /* Form MCPS request */
        switch (lpkt->pkt_type) {
        case MCPS_UNCONFIRMED:
            evstatus = LORAMAC_EVENT_INFO_STATUS_OK;
            break;
        case MCPS_CONFIRMED:
            evstatus = LORAMAC_EVENT_INFO_STATUS_OK;
            break;
        case MCPS_PROPRIETARY:
            /* XXX: not allowed */
            evstatus = LORAMAC_EVENT_INFO_STATUS_ERROR;
            break;
        default:
            /* XXX: this is an error */
            evstatus = LORAMAC_EVENT_INFO_STATUS_ERROR;
            break;
        }

        if (evstatus == LORAMAC_EVENT_INFO_STATUS_OK) {
            rc = LoRaMacMcpsRequest(om, lpkt);
            switch (rc) {
            case LORAMAC_STATUS_OK:
                /* Transmission started. */
                evstatus = LORAMAC_EVENT_INFO_STATUS_OK;
                break;
            case LORAMAC_STATUS_NO_NETWORK_JOINED:
                evstatus = LORAMAC_EVENT_INFO_STATUS_NO_NETWORK_JOINED;
                break;
            case LORAMAC_STATUS_LENGTH_ERROR:
                evstatus = LORAMAC_EVENT_INFO_STATUS_TX_DR_PAYLOAD_SIZE_ERROR;
                break;
            /* XXX: handle BUSY differently here I think. Need to requeue
               at head */
            default:
                evstatus = LORAMAC_EVENT_INFO_STATUS_ERROR;
                break;
            }

            if (evstatus == LORAMAC_EVENT_INFO_STATUS_OK) {
                return;
            }
        }

        /*
         * If we get here there was an error sending. Send confirm and
         * continue processing transmit queue.
         */
proc_txq_om_done:
        lpkt->status = evstatus;
        lora_app_mcps_confirm(om);
    }
}

static void
lora_mac_txq_timer_cb(struct os_event *ev)
{
    lora_mac_proc_tx_q_event(NULL);
}

/**
 * The LoRa mac task
 *
 * @param arg Pointer to arg passed to task (currently NULL).
 */
void
lora_mac_task(void *arg)
{
    /* Process events */
    while (1) {
        os_eventq_run(&g_lora_mac_data.lm_evq);
    }
}
#endif

#if !MYNEWT_VAL(LORA_APP_AUTO_JOIN)
/**
 * Called to check if this device is joined to a network.
 *
 *
 * @return int  LORA_APP_STATUS_ALREADY_JOINED if joined.
 *              LORA_APP_STATUS_NO_NETWORK if not joined
 */
int
lora_node_chk_if_joined(void)
{
    int rc;
    MibRequestConfirm_t mibReq;

    /* Check to see if we are already joined. If so, return an error */
    mibReq.Type = MIB_NETWORK_JOINED;
    LoRaMacMibGetRequestConfirm(&mibReq);

    if (mibReq.Param.IsNetworkJoined == true) {
        rc = LORA_APP_STATUS_ALREADY_JOINED;
    } else {
        rc = LORA_APP_STATUS_NO_NETWORK;
    }
    return rc;
}

/**
 * Called when the application wants to perform the join process.
 *
 * @return int A lora app return code
 */
int
lora_node_join(uint8_t *dev_eui, uint8_t *app_eui, uint8_t *app_key,
               uint8_t trials)
{
    int rc;

    rc = lora_node_chk_if_joined();
    if (rc != LORA_APP_STATUS_ALREADY_JOINED) {
        /* Send event to MAC */
        g_lm_join_ev_arg.dev_eui = dev_eui;
        g_lm_join_ev_arg.app_eui = app_eui;
        g_lm_join_ev_arg.app_key = app_key;
        g_lm_join_ev_arg.trials = trials;
        os_eventq_put(&g_lora_mac_data.lm_evq, &g_lora_mac_data.lm_join_ev);
        rc = LORA_APP_STATUS_OK;
    }

    return rc;
}

/**
 * Called when the application wants to perform a link check
 *
 * @return int  A lora app return code
 */
int
lora_node_link_check(void)
{
    int rc;

    rc = lora_node_chk_if_joined();
    if (rc == LORA_APP_STATUS_ALREADY_JOINED) {
        os_eventq_put(&g_lora_mac_data.lm_evq, &g_lora_mac_data.lm_link_chk_ev);
        rc = LORA_APP_STATUS_OK;
    }

    return rc;
}

#if !MYNEWT_VAL(LORA_NODE_CLI)
static void
lora_mac_join_event(struct os_event *ev)
{
    MlmeReq_t mlmeReq;
    LoRaMacStatus_t rc;
    LoRaMacEventInfoStatus_t status;
    struct lm_join_ev_arg_obj *lmj;

    /* XXX: should we check if we are joined here too? Could we have
       joined in meantime? */
    lmj = (struct lm_join_ev_arg_obj *)ev->ev_arg;
    mlmeReq.Type = MLME_JOIN;
    mlmeReq.Req.Join.DevEui = lmj->dev_eui;
    mlmeReq.Req.Join.AppEui = lmj->app_eui;
    mlmeReq.Req.Join.AppKey = lmj->app_key;
    mlmeReq.Req.Join.NbTrials = lmj->trials;

    rc = LoRaMacMlmeRequest(&mlmeReq);
    switch (rc) {
    case LORAMAC_STATUS_OK:
        status = LORAMAC_EVENT_INFO_STATUS_OK;
        break;
    /* XXX: for now, just report this generic error. */
    default:
        status = LORAMAC_EVENT_INFO_STATUS_ERROR;
        break;
    }

    if (status != LORAMAC_EVENT_INFO_STATUS_OK) {
        if (lora_join_cb_func) {
            lora_join_cb_func(status, 0);
        }
    }
}

/**
 * lora mac link chk event
 *
 * Called from the Lora MAC task when a link check event has been posted
 * to it. This event gets posted when link check API gets called.
 *
 *
 * @param ev
 */
static void
lora_mac_link_chk_event(struct os_event *ev)
{
    MlmeReq_t mlmeReq;
    LoRaMacStatus_t rc;
    LoRaMacEventInfoStatus_t status;

    mlmeReq.Type = MLME_LINK_CHECK;
    rc = LoRaMacMlmeRequest(&mlmeReq);
    switch (rc) {
    case LORAMAC_STATUS_OK:
        status = LORAMAC_EVENT_INFO_STATUS_OK;
        break;
    /* XXX: for now, just report this generic error. */
    default:
        status = LORAMAC_EVENT_INFO_STATUS_ERROR;
        break;
    }

    lora_node_log(LORA_NODE_LOG_LINK_CHK, 0, 0, status);

    /* If status is OK */
    if (status != LORAMAC_EVENT_INFO_STATUS_OK) {
        if (lora_link_chk_cb_func) {
            lora_link_chk_cb_func(status, 0, 0);
        }
    } else {
        /*
         * If nothing on transmit queue, we need to send event so that link
         * check gets sent.
         */
        if (lora_node_txq_empty()) {
            lora_node_chk_txq();
        }
    }
}
#endif
#endif

/**
 * Helper routine to track measurement averages.
 *
 * @param orig State variable
 * @param sample Latest sample to add to average.
 */
static void
lora_node_calc_avg(int16_t *orig, uint16_t sample)
{
    int16_t tmp;

    tmp = *orig;
    if (tmp) {
	/*
	 * The following magic is equivalent to algorithm
         * avg = sample/16 + avg*15/16 in fixed point.
	 */
	tmp += (sample << LORA_DELTA_SHIFT) - (tmp >> LORA_AVG_SHIFT);
	*orig = tmp;
    } else {
	/*
	 * No measurement yet.
	 */
	*orig = sample << (LORA_AVG_SHIFT + LORA_DELTA_SHIFT);
    }
}

void
lora_node_qual_sample(int16_t rssi, int16_t snr)
{
    lora_node_calc_avg(&g_lora_mac_data.lm_rssi_avg, rssi);
    lora_node_calc_avg(&g_lora_mac_data.lm_snr_avg, snr);
}

/**
 * Report tracked RSSI/SNR averages
 *
 * @param rssi Pointer to where to store RSSI average.
 * @param snr Pointer to where to store SNR average.
 *
 * @return 0 if returned data is valid, non-zero otherwise
 */
int
lora_node_link_qual(int16_t *rssi, int16_t *snr)
{
    struct lora_mac_obj *lmo = &g_lora_mac_data;

    if (lmo->lm_rssi_avg || lmo->lm_snr_avg) {
        /*
         * Rounds down. XXX
         */
        *rssi = g_lora_mac_data.lm_rssi_avg >> (LORA_AVG_SHIFT +
                                                LORA_DELTA_SHIFT);
        *snr = g_lora_mac_data.lm_snr_avg >> (LORA_AVG_SHIFT +
                                              LORA_DELTA_SHIFT);
        return 0;
    } else {
        return -1;
    }
}

struct os_eventq *
lora_node_mac_evq_get(void)
{
    return &g_lora_mac_data.lm_evq;
}

void
lora_node_init(void)
{
    int rc;
#if !MYNEWT_VAL(LORA_NODE_CLI)
    LoRaMacStatus_t lms;
#endif

    rc = stats_init_and_reg(
        STATS_HDR(lora_mac_stats),
        STATS_SIZE_INIT_PARMS(lora_mac_stats, STATS_SIZE_32),
        STATS_NAME_INIT_PARMS(lora_mac_stats), "lora_mac");
    SYSINIT_PANIC_ASSERT(rc == 0);

#if !MYNEWT_VAL(LORA_NODE_CLI)
    /* Init app */
    lora_app_init();

    /*--- MAC INIT ---*/
    /* Initialize eventq */
    os_eventq_init(&g_lora_mac_data.lm_evq);

    /* Set up transmit done queue and event */
    os_mqueue_init(&g_lora_mac_data.lm_txq, lora_mac_proc_tx_q_event, NULL);

    /* Create the mac task */
    os_task_init(&g_lora_mac_task, "loramac", lora_mac_task, NULL,
                 MYNEWT_VAL(LORA_MAC_PRIO), OS_WAIT_FOREVER, g_lora_mac_stack,
                 LORA_MAC_STACK_SIZE);

    /* Initialize join event */
    g_lora_mac_data.lm_join_ev.ev_cb = lora_mac_join_event;
    g_lora_mac_data.lm_join_ev.ev_arg = &g_lm_join_ev_arg;

    /* Initialize link check event */
    g_lora_mac_data.lm_link_chk_ev.ev_cb = lora_mac_link_chk_event;

    /* Initialize the transmit queue timer */
    os_callout_init(&g_lora_mac_data.lm_txq_timer,
                    &g_lora_mac_data.lm_evq, lora_mac_txq_timer_cb, NULL);

    /* Initialize the LoRa mac */
    lora_cb.GetBatteryLevel = lora_node_get_batt_status;

    lms = LoRaMacInitialization(&lora_cb, LORA_NODE_REGION);
    assert(lms == LORAMAC_STATUS_OK);
#endif
}

static bool low_power_active = true;

void lora_enter_low_power(void)
{
    if (!low_power_active) {
        low_power_active = true;
        hal_timer_deinit(MYNEWT_VAL(LORA_MAC_TIMER_NUM));
        lora_node_log(LORA_NODE_LOG_LP_ENTER, 0, 0, 0);
    }
}

void lora_exit_low_power(void)
{
    if (low_power_active) {
        low_power_active = false;
        lora_bsp_enable_mac_timer();
        lora_node_log(LORA_NODE_LOG_LP_EXIT, 0, 0, 0);
        lora_config_peripherals();
    }
}
