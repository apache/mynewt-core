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
#include "sysinit/sysinit.h"
#include "syscfg/syscfg.h"
#include "os/os.h"
#include "node/lora.h"
#include "node/lora_priv.h"
#include "node/mac/LoRaMac.h"

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
    STATS_NAME(lora_mac_stats, rx_errors)
    STATS_NAME(lora_mac_stats, rx_frames)
    STATS_NAME(lora_mac_stats, rx_mic_failures)
    STATS_NAME(lora_mac_stats, rx_mlme)
    STATS_NAME(lora_mac_stats, rx_mcps)
STATS_NAME_END(lora_mac_stats)

STATS_SECT_DECL(lora_stats) lora_stats;
STATS_NAME_START(lora_stats)
    STATS_NAME(lora_stats, rx_error)
    STATS_NAME(lora_stats, rx_success)
    STATS_NAME(lora_stats, rx_timeout)
    STATS_NAME(lora_stats, tx_success)
    STATS_NAME(lora_stats, tx_timeout)
STATS_NAME_END(lora_stats)

/* Device EUI */
uint8_t g_lora_dev_eui[LORA_EUI_LEN];

/* Application EUI */
uint8_t g_lora_app_eui[LORA_EUI_LEN];

/* Application Key */
uint8_t g_lora_app_key[LORA_KEY_LEN];

#if !MYNEWT_VAL(LORA_NODE_CLI)
LoRaMacPrimitives_t g_lora_primitives;
#endif

/* MAC task */
#define LORA_MAC_STACK_SIZE   (256)
struct os_task g_lora_mac_task;
os_stack_t g_lora_mac_stack[LORA_MAC_STACK_SIZE];

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
};

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
struct lora_node_debug_log_entry
{
    uint8_t lnd_id;
    uint8_t lnd_p8;
    uint16_t lnd_p16;
    uint32_t lnd_p32;
    uint32_t lnd_cputime;
};

static struct lora_node_debug_log_entry g_lnd_log[LORA_NODE_DEBUG_LOG_ENTRIES];
static uint8_t g_lnd_log_index;

void
lora_node_log(uint8_t logid, uint8_t p8, uint16_t p16, uint32_t p32)
{
    os_sr_t sr;

    OS_ENTER_CRITICAL(sr);
    g_lnd_log[g_lnd_log_index].lnd_id = logid;
    g_lnd_log[g_lnd_log_index].lnd_p8 = p8;
    g_lnd_log[g_lnd_log_index].lnd_p16 = p16;
    g_lnd_log[g_lnd_log_index].lnd_p32 = p32;
    g_lnd_log[g_lnd_log_index].lnd_cputime =
        hal_timer_read(MYNEWT_VAL(LORA_MAC_TIMER_NUM));

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

#if !MYNEWT_VAL(LORA_NODE_CLI)
static struct os_mbuf *
lora_node_alloc_empty_pkt(void)
{
    struct os_mbuf *om;
    struct lora_pkt_info *lpkt;

    om = lora_pkt_alloc();
    if (om) {
        /* XXX: what to do for datarate here? */
        lpkt = LORA_PKT_INFO_PTR(om);
        lpkt->port = 0;
        lpkt->pkt_type = MCPS_UNCONFIRMED;
        lpkt->txdinfo.datarate = DR_0;
    }
    return om;
}
#endif

/**
 * This is the application to mac layer transmit interface.
 *
 * @param om Pointer to transmit packet
 */
void
lora_node_mcps_request(struct os_mbuf *om)
{
    int rc;

    rc = os_mqueue_put(&g_lora_mac_data.lm_txq, &g_lora_mac_data.lm_evq, om);
    assert(rc == 0);
}

#if !MYNEWT_VAL(LORA_NODE_CLI)
void
lora_node_reset_txq_timer(void)
{
    /* XXX: For now, just reset timer to fire off in one second */
    os_callout_reset(&g_lora_mac_data.lm_txq_timer, OS_TICKS_PER_SEC);
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

/* MAC MCPS-Confirm primitive */
static void
lora_node_mac_mcps_confirm(McpsConfirm_t *confirm)
{
    struct os_mbuf *om;
    struct lora_pkt_info *lpkt;

    /*
     * XXX: note that datarate and retries were set when the
     * request was made. They could have changed. Do we need
     * to preserve original values?
     */

    /* Copy confirmation info into lora packet info */
    om = confirm->om;
    lpkt = LORA_PKT_INFO_PTR(om);
    lpkt->status = confirm->Status;
    lpkt->txdinfo.datarate = confirm->Datarate;
    lpkt->txdinfo.txpower = confirm->TxPower;
    lpkt->txdinfo.ack_rxd = confirm->AckReceived;
    lpkt->txdinfo.retries = confirm->NbRetries;
    lpkt->txdinfo.tx_time_on_air = confirm->TxTimeOnAir;
    lpkt->txdinfo.uplink_cntr = confirm->UpLinkCounter;
    lpkt->txdinfo.uplink_freq = confirm->UpLinkFrequency;
    lora_app_mcps_confirm(om);
}

/* MAC MCPS-Indicate primitive  */
static void
lora_node_mac_mcps_indicate(McpsIndication_t *ind)
{
    int rc;
    struct os_mbuf *om;
    struct lora_pkt_info *lpkt;

    /*
     * Not sure if this is possible, but port 0 is not a valid application port.
     * If the port is 0 do not send indicate
     */
    if (ind->Port == 0) {
        /* XXX: count a statistic? */
        return;
    }

    om = lora_pkt_alloc();
    if (om) {
        /* Copy data into mbuf */
        rc = os_mbuf_copyinto(om, 0, ind->Buffer, ind->BufferSize);
        if (rc) {
            os_mbuf_free_chain(om);
            return;
        }

        /* Set lora packet info */
        lpkt = LORA_PKT_INFO_PTR(om);
        lpkt->status = ind->Status;
        lpkt->pkt_type = ind->McpsIndication;
        lpkt->port = ind->Port;
        lpkt->rxdinfo.rxdatarate = ind->RxDatarate;
        lpkt->rxdinfo.rxdata = ind->RxData;
        lpkt->rxdinfo.snr = ind->Snr;
        lpkt->rxdinfo.rssi = ind->Rssi;
        lpkt->rxdinfo.frame_pending = ind->FramePending == 0 ? 0 : 1;
        lpkt->rxdinfo.ack_rxd = ind->AckReceived;
        lpkt->rxdinfo.downlink_cntr = ind->DownLinkCounter;
        lpkt->rxdinfo.rxslot = ind->RxSlot;
        lora_app_mcps_indicate(om);
    } else {
        /* XXX: cant do anything until the lower stack gets modified */
    }
}

/**
 * This is the LoRaMac stack Confirm primitive. It is called by the LoRaMac
 * stack upon completion of a MLME service.
 *
 * @param confirm
 */
static void
lora_node_mac_mlme_confirm(MlmeConfirm_t *confirm)
{
    /* XXX: do we need the time on air for MLME packets? */
    switch (confirm->MlmeRequest) {
    case MLME_JOIN:
        lora_app_join_confirm(confirm->Status, confirm->NbRetries);
        break;
    case MLME_LINK_CHECK:
        lora_app_link_chk_confirm(confirm->Status, confirm->NbGateways,
                                  confirm->DemodMargin);
        break;
    default:
        /* Nothing to do here */
        break;
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
    McpsReq_t req;
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
        lora_node_reset_txq_timer();
        return;
    }

    /*
     * XXX: there is a disconnect in the stack here.
     * The LoRaMacQueryTxPossible() routine uses the Default channel rate
     * to determine if the frame can be sent if ADR is not enabled. However,
     * the stack can set the data it wants to send in the request. If these
     * two are different there will be problems. Not sure why the app is even
     * specifying data rate. But I guess we can leave it for now and hope
     * that the app does not use a datarate that is different than the
     * default.
     */

    /*
     * Check if possible to send frame. If a MAC command length error we
     * need to send an empty, unconfirmed frame to flush mac commands.
     */
    while (1) {
        mp = STAILQ_FIRST(&g_lora_mac_data.lm_txq.mq_head);
        if (mp == NULL) {
            break;
        }

        rc = LoRaMacQueryTxPossible(mp->omp_len, &txinfo);
        if (rc == LORAMAC_STATUS_MAC_CMD_LENGTH_ERROR) {
            /* Need to flush MAC commands. Send empty unconfirmed frame */
            om = lora_node_alloc_empty_pkt();
            if (!om) {
                lora_node_reset_txq_timer();
                return;
            }
        } else {
            om = os_mqueue_get(&g_lora_mac_data.lm_txq);
            assert(om != NULL);
        }
        lpkt = LORA_PKT_INFO_PTR(om);

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
        req.om = om;
        req.Type = lpkt->pkt_type;
        switch (req.Type) {
        case MCPS_UNCONFIRMED:
            req.Req.Unconfirmed.fPort = lpkt->port;
            req.Req.Unconfirmed.fBuffer = om->om_data;
            req.Req.Unconfirmed.fBufferSize = OS_MBUF_PKTLEN(om);
            req.Req.Unconfirmed.Datarate = lpkt->txdinfo.datarate;
            evstatus = LORAMAC_EVENT_INFO_STATUS_OK;
            break;
        case MCPS_CONFIRMED:
            req.Req.Confirmed.fPort = lpkt->port;
            req.Req.Confirmed.fBuffer = om->om_data;
            req.Req.Confirmed.fBufferSize = OS_MBUF_PKTLEN(om);
            req.Req.Confirmed.Datarate = lpkt->txdinfo.datarate;
            req.Req.Confirmed.NbTrials = lpkt->txdinfo.retries;
            evstatus = LORAMAC_EVENT_INFO_STATUS_OK;
            break;
        case MCPS_MULTICAST:
            /* XXX: implement */
            evstatus = LORAMAC_EVENT_INFO_STATUS_ERROR;
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
            rc = LoRaMacMcpsRequest(&req);
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
            default:
                evstatus = LORAMAC_EVENT_INFO_STATUS_ERROR;
                break;
            }

            if (evstatus == LORAMAC_EVENT_INFO_STATUS_OK) {
                break;
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
    if (!lora_node_txq_empty()) {
        lora_mac_proc_tx_q_event(NULL);
    }
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
static int
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

static void
lora_mac_link_chk_event(struct os_event *ev)
{
    int err;
    MlmeReq_t mlmeReq;
    LoRaMacStatus_t rc;
    LoRaMacEventInfoStatus_t status;
    struct os_mbuf *om;

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

    /* If status is OK */
    if (status != LORAMAC_EVENT_INFO_STATUS_OK) {
        if (lora_link_chk_cb_func) {
            lora_link_chk_cb_func(status, 0, 0);
        }
    } else {
        /*
         * If nothing on transmit queue this will sit around until the
         * application decides to send its next packet. In this case, we
         * create a dummy frame to send the command.
         */
        if (lora_node_txq_empty()) {
            om = lora_node_alloc_empty_pkt();
            if (om) {
                err = os_mqueue_put(&g_lora_mac_data.lm_txq,
                                 &g_lora_mac_data.lm_evq, om);
                assert(err == 0);
            }
        }
    }
}
#endif
#endif

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
    LoRaMacCallback_t lora_cb;
#endif

    rc = stats_init_and_reg(
        STATS_HDR(lora_stats),
        STATS_SIZE_INIT_PARMS(lora_stats, STATS_SIZE_32),
        STATS_NAME_INIT_PARMS(lora_stats), "lora");
    SYSINIT_PANIC_ASSERT(rc == 0);

    rc = stats_init_and_reg(
        STATS_HDR(lora_mac_stats),
        STATS_SIZE_INIT_PARMS(lora_mac_stats, STATS_SIZE_32),
        STATS_NAME_INIT_PARMS(lora_mac_stats), "lora_mac");
    SYSINIT_PANIC_ASSERT(rc == 0);

#if MYNEWT_VAL(LORA_NODE_CLI)
    lora_cli_init();
#else
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
    g_lora_primitives.MacMcpsConfirm = lora_node_mac_mcps_confirm;
    g_lora_primitives.MacMcpsIndication = lora_node_mac_mcps_indicate;
    g_lora_primitives.MacMlmeConfirm = lora_node_mac_mlme_confirm;
    lora_cb.GetBatteryLevel = lora_node_get_batt_status;

    lms = LoRaMacInitialization(&g_lora_primitives, &lora_cb);
    assert(lms == LORAMAC_STATUS_OK);
#endif
}
