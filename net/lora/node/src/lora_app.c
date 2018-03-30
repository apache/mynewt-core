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

#include <assert.h>
#include "os/mynewt.h"
#include "node/lora.h"
#include "node/lora_priv.h"

/* XXX: for now, auto-join not supported */
#if MYNEWT_VAL(LORA_APP_AUTO_JOIN)
#error "Auto-joining not supported"
#endif

/* Valid app port range */
#define LORA_APP_PORT_MAX_VAL   (223)

/* Port structure */
struct lora_app_port
{
    uint8_t port_num;
    uint8_t opened;
    uint8_t retries;
    uint8_t pad8;
    lora_rxd_func rxd_cb;
    lora_txd_func txd_cb;
};

/* Port memory */
static struct lora_app_port lora_app_ports[MYNEWT_VAL(LORA_APP_NUM_PORTS)];

/* Lora APP Receive queue and event */
struct os_mqueue lora_node_app_rx_q;
struct os_mqueue lora_node_app_txd_q;

/* Lora app event queue pointer */
static struct os_eventq *lora_node_app_evq;

/* Join and link check callbacks and events */
lora_join_cb lora_join_cb_func;
lora_link_chk_cb lora_link_chk_cb_func;
struct os_event lora_app_join_ev;
struct os_event lora_app_link_chk_ev;

struct lora_app_join_ev_obj
{
    uint8_t attempts;
    LoRaMacEventInfoStatus_t status;
};
static struct lora_app_join_ev_obj lora_app_join_ev_data;

struct lora_app_link_chk_ev_obj
{
    uint8_t num_gw;
    uint8_t demod_margin;
    LoRaMacEventInfoStatus_t status;
};
static struct lora_app_link_chk_ev_obj lora_app_link_chk_ev_data;

/* Internal protos */
static int lora_app_port_receive(struct os_mbuf *om);
static int lora_app_port_txd(struct os_mbuf *om);

/* Get the lora app event queue. */
static struct os_eventq *
lora_node_app_evq_get(void)
{
    return lora_node_app_evq;
}

/**
 * Find an open app port given its port number.
 *
 * @param port Port number
 *
 * @return struct lora_app_port*
 *      NULL if no open port found
 *      Pointer to port if opened port found.
 */
static struct lora_app_port *
lora_app_port_find_open(uint8_t port)
{
    int i;
    struct lora_app_port *lap;

    lap = NULL;
    for (i = 0; i < MYNEWT_VAL(LORA_APP_NUM_PORTS); ++i) {
        if ((lora_app_ports[i].opened != 0) &&
            (lora_app_ports[i].port_num == port)) {
            lap = &lora_app_ports[i];
            break;
        }
    }

    return lap;
}

/**
 * Process received packet event. This event is processed by the task
 * that handles the lora app.
 *
 * @param ev Pointer to event.
 */
static void
lora_node_proc_app_rxd_event(struct os_event *ev)
{
    struct os_mbuf *om;

    /* Go through packet queue and call rx callback for all */
    while ((om = os_mqueue_get(&lora_node_app_rx_q)) != NULL) {
        lora_app_port_receive(om);
    }
}

/**
 * Process transmit done event. This event is processed by the task
 * that handles the lora app.
 *
 * @param ev Pointer to event.
 */
static void
lora_node_proc_app_txd_event(struct os_event *ev)
{
    struct os_mbuf *om;

    /* Go through packet queue and call rx callback for all */
    while ((om = os_mqueue_get(&lora_node_app_txd_q)) != NULL) {
        lora_app_port_txd(om);
    }
}

/**
 * Open a lora application port. This function will allocate a lora port, set
 * port default values for datarate and retries, set the transmit done and
 * received data callbacks, and add port to list of open ports.
 *
 * @param port      Port number
 * @param txd_cb    Transmit done callback
 * @param rxd_cb    Receive data callback
 *
 * @return int A return code from set of lora return codes
 */
int
lora_app_port_open(uint8_t port, lora_txd_func txd_cb, lora_rxd_func rxd_cb)
{
    int rc;
    int i;
    int avail;

    /* Check valid range */
    if ((port == 0) || (port > LORA_APP_PORT_MAX_VAL)) {
        return LORA_APP_STATUS_INVALID_PORT;
    }

    /* Check other parameters */
    if ((txd_cb == NULL) || (rxd_cb == NULL)) {
        return LORA_APP_STATUS_INVALID_PARAM;
    }

    /* Make sure port is not opened */
    avail = -1;
    for (i = 0; i < MYNEWT_VAL(LORA_APP_NUM_PORTS); ++i) {
        /* If port not opened, remember first available */
        if (lora_app_ports[i].opened == 0) {
            if (avail < 0) {
                avail = i;
            }
        } else {
            /* Make sure port is not already opened */
            if (lora_app_ports[i].port_num == port) {
                return LORA_APP_STATUS_ALREADY_OPEN;
            }
        }
    }

    /* Open port if available */
    if (avail >= 0) {
        lora_app_ports[avail].port_num = port;
        lora_app_ports[avail].rxd_cb = rxd_cb;
        lora_app_ports[avail].txd_cb = txd_cb;
        lora_app_ports[avail].retries = 8;
        lora_app_ports[avail].opened = 1;
        rc = LORA_APP_STATUS_OK;
    } else {
        rc = LORA_APP_STATUS_ENOMEM;
    }

    return rc;
}

/**
 * Close an open lora port
 *
 * @param port Port number
 *
 * @return int A return code from set of lora return codes
 */
int
lora_app_port_close(uint8_t port)
{
    int rc;
    struct lora_app_port *lap;

    rc = LORA_APP_STATUS_NO_PORT;
    lap = lora_app_port_find_open(port);
    if (lap) {
        lap->opened = 0;
        rc = LORA_APP_STATUS_OK;
    }

    return rc;
}

/**
 * Configure an application port. This configures the number of retries for
 * confirmed packets.
 *
 * NOTE: The port must be opened or this will return an error
 *
 * @param port Port number
 * @param retries NUmmber of retries for confirmed packets
 *
 * @return int A return code from set of lora return codes
 */
int
lora_app_port_cfg(uint8_t port, uint8_t retries)
{
    int rc;
    struct lora_app_port *lap;

    /* Verify datarate and retries */
    if (retries > MAX_ACK_RETRIES) {
        return LORA_APP_STATUS_INVALID_PARAM;
    }

    rc = LORA_APP_STATUS_NO_PORT;
    lap = lora_app_port_find_open(port);
    if (lap) {
        lap->retries = retries;
        rc = LORA_APP_STATUS_OK;
    }

    return rc;
}

/**
 * Send a packet on a port. If this routine returns an error the "transmitted"
 * callback will NOT be called; it is the callers responsibility to handle the
 * packet appropriately (including freeing any memory if needed).
 *
 * @param port Port number
 * @param pkt_type Type of packet
 * @param om Pointer to packet
 *
 * @return int A return code from set of lora return codes
 */
int
lora_app_port_send(uint8_t port, Mcps_t pkt_type, struct os_mbuf *om)
{
    int rc;
    struct lora_app_port *lap;
    struct lora_pkt_info *lpkt;

    /* If no buffer to send, fine. */
    if ((om == NULL) || (OS_MBUF_PKTLEN(om) == 0)) {
        return LORA_APP_STATUS_INVALID_PARAM;
    }
    assert(OS_MBUF_USRHDR_LEN(om) >= sizeof(struct lora_pkt_info));

    /* Check valid packet type. Only confirmed and unconfirmed for now. */
    if ((pkt_type != MCPS_UNCONFIRMED) && (pkt_type != MCPS_CONFIRMED)) {
        return LORA_APP_STATUS_INVALID_PARAM;
    }

    lap = lora_app_port_find_open(port);
    if (lap) {
        /* Set packet information required by MAC */
        lpkt = LORA_PKT_INFO_PTR(om);
        lpkt->port = port;
        lpkt->pkt_type = pkt_type;
        lpkt->txdinfo.retries = lap->retries;
        lora_node_mcps_request(om);
        rc = LORA_APP_STATUS_OK;
    } else {
        rc = LORA_APP_STATUS_NO_PORT;
    }

    return rc;
}

/**
 * What's the maximum payload which can be sent on next frame
 *
 * @return int payload length, negative on error.
 */
int
lora_app_mtu(void)
{
    return lora_node_mtu();
}

/**
 * Called by lora task when a packet has been received for an application port.
 *
 * NOTE: This API is not to be called by an application; it is called
 * by the task handling lora events.
 *
 * @param om Pointer to received packet
 *
 * @return int A return code from set of lora return codes
 */
static int
lora_app_port_receive(struct os_mbuf *om)
{
    int rc;
    struct lora_app_port *lap;
    struct lora_pkt_info *lpkt;

    lpkt = LORA_PKT_INFO_PTR(om);
    lap = lora_app_port_find_open(lpkt->port);
    if (lap) {
        lap->rxd_cb(lpkt->port, lpkt->status, lpkt->pkt_type, om);
        rc = LORA_APP_STATUS_OK;
    } else {
        os_mbuf_free_chain(om);
        rc = LORA_APP_STATUS_NO_PORT;
    }

    return rc;
}

/**
 * Called by lora task when a packet has been transmitted.
 *
 * NOTE: This API is not to be called by an application; it is called
 * by the task handling lora events.
 *
 * @param om Pointer to transmited packet
 *
 * @return int A return code from set of lora return codes
 */
static int
lora_app_port_txd(struct os_mbuf *om)
{
    int rc;
    struct lora_app_port *lap;
    struct lora_pkt_info *lpkt;

    lpkt = LORA_PKT_INFO_PTR(om);
    lap = lora_app_port_find_open(lpkt->port);
    if (lap) {
        lap->txd_cb(lpkt->port, lpkt->status, lpkt->pkt_type, om);
        rc = LORA_APP_STATUS_OK;
    } else {
        os_mbuf_free_chain(om);
        rc = LORA_APP_STATUS_NO_PORT;
    }

    return rc;
}

/**
 * Called from lower layer when a packet has been received for an application
 * port.
 *
 * @param om Pointer to received packet
 */
void
lora_app_mcps_indicate(struct os_mbuf *om)
{
    int rc;

    rc = os_mqueue_put(&lora_node_app_rx_q, lora_node_app_evq_get(), om);
    assert(rc == 0);
}

/**
 * Interface between app and mac for MCPS confirmation. MAC layer calls
 * this function when a packet has been transmitted or an error occurred.
 *
 * @param om Pointer to transmitted packet
 */
void
lora_app_mcps_confirm(struct os_mbuf *om)
{
    int rc;

    rc = os_mqueue_put(&lora_node_app_txd_q, lora_node_app_evq_get(), om);
    assert(rc == 0);
}

#if !MYNEWT_VAL(LORA_APP_AUTO_JOIN)
/* XXX: personalization? */
/**
 *  Join a lora network. When called this function will attempt to join
 *  if the end device is not already joined. Join status (success, failure)
 *  will be reported through the callback. If this function returns an error
 *  no callback will occur.
 *
 * @param dev_eui   Pointer to device EUI
 * @param app_eui   Pointer to Application EUI
 * @param app_key   Pointer to application key
 * @param trials    Number of join attempts before failure
 *
 * @return int Lora app return code
 */
int
lora_app_join(uint8_t *dev_eui, uint8_t *app_eui, uint8_t *app_key,
              uint8_t trials)
{
    int rc;

    /* Make sure parameters are valid */
    if ((dev_eui == NULL) || (app_eui == NULL) || (app_key == NULL) ||
        (trials == 0)) {
        return LORA_APP_STATUS_INVALID_PARAM;
    }

    /* Tell device to start join procedure */
    rc = lora_node_join(dev_eui, app_eui, app_key, trials);
    return rc;
}

/* Performs a link check */
int
lora_app_link_check(void)
{
    int rc;

    rc = lora_node_link_check();
    return rc;
}
#endif

/**
 * Set the join callback. This will be called when joining succeeds or fails.
 *
 * @param join_cb Pointer to join callback function.
 *
 * @return int Lora return code
 */
int
lora_app_set_join_cb(lora_join_cb join_cb)
{
    assert(join_cb != NULL);
    lora_join_cb_func = join_cb;
    return LORA_APP_STATUS_OK;
}

/* Set the link check callback */
int
lora_app_set_link_check_cb(lora_link_chk_cb link_chk_cb)
{
    assert(link_chk_cb != NULL);
    lora_link_chk_cb_func = link_chk_cb;
    return LORA_APP_STATUS_OK;
}

void
lora_app_join_ev_cb(struct os_event *ev)
{
    struct lora_app_join_ev_obj *evdata;

    evdata = (struct lora_app_join_ev_obj *)ev->ev_arg;
    if (lora_join_cb_func) {
        lora_join_cb_func(evdata->status, evdata->attempts);
    }
}

void
lora_app_link_chk_ev_cb(struct os_event *ev)
{
    struct lora_app_link_chk_ev_obj *evdata;

    evdata = (struct lora_app_link_chk_ev_obj *)ev->ev_arg;
    if (lora_link_chk_cb_func) {
        lora_link_chk_cb_func(evdata->status, evdata->num_gw,
                              evdata->demod_margin);
    }
}

/**
 * Called by the lora mac task when a join confirm occurs. This function fills
 * out the event arguments and posts the appropriate event to the lora app task.
 *
 * @param status
 * @param attempts
 */
void
lora_app_join_confirm(LoRaMacEventInfoStatus_t status, uint8_t attempts)
{
    lora_app_join_ev_data.status = status;
    lora_app_join_ev_data.attempts = attempts;
    os_eventq_put(lora_node_app_evq, &lora_app_join_ev);
}

/**
 * Called by the lora mac task when a link check confirm occurs. This function
 * fills out the event arguments and posts the appropriate event to the lora app
 * task.
 *
 * @param status
 * @param num_gw
 * @param demod_margin
 */
void
lora_app_link_chk_confirm(LoRaMacEventInfoStatus_t status, uint8_t num_gw,
                          uint8_t demod_margin)
{
    lora_app_link_chk_ev_data.status = status;
    lora_app_link_chk_ev_data.num_gw = num_gw;
    lora_app_link_chk_ev_data.demod_margin = demod_margin;
    os_eventq_put(lora_node_app_evq, &lora_app_link_chk_ev);
}

void
lora_app_init(void)
{
    /* For now, the lora app event queue is the default event queue */
    lora_node_app_evq = os_eventq_dflt_get();

    /* Init join and link check events */
    lora_app_join_ev.ev_arg = &lora_app_join_ev_data;
    lora_app_join_ev.ev_cb = lora_app_join_ev_cb;
    lora_app_link_chk_ev.ev_arg = &lora_app_link_chk_ev_data;
    lora_app_link_chk_ev.ev_cb = lora_app_link_chk_ev_cb;

    /* Set up receive queue and event */
    os_mqueue_init(&lora_node_app_rx_q, lora_node_proc_app_rxd_event, NULL);

    /* Set up transmit done queue and event */
    os_mqueue_init(&lora_node_app_txd_q, lora_node_proc_app_txd_event, NULL);
}
