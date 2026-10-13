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

/*
 * lwIP network interface on top of TinyUSB network function.
 *
 * TinyUSB network drivers are not thread safe and all their callbacks run
 * in USB task.  lwIP runs in tcpip thread (or any task holding the core
 * lock).  Therefore:
 *  - received frames are copied to pbufs in USB task and passed to lwIP
 *    with tcpip_input(),
 *  - frames from lwIP are queued and sent from USB task by a function
 *    deferred with usbd_defer_func(); USB events reschedule it when
 *    transmission was blocked,
 *  - link state follows USB state and is passed to lwIP with a tcpip
 *    callback message.
 */

#include <assert.h>
#include <string.h>

#include <os/mynewt.h>
#include <hal/hal_bsp.h>

#include <lwip/tcpip.h>
#include <lwip/netif.h>
#include <lwip/pbuf.h>
#include <lwip/etharp.h>
#include <lwip/ethip6.h>
#include <netif/ethernet.h>

#include <tusb.h>
#include <device/dcd.h>
#include <device/usbd_pvt.h>

#include <net_lwip/net_lwip.h>
#if MYNEWT_VAL(USBD_NET_DHCP_SERVER)
#include <dhcp_server/dhcp_server.h>
#endif

#define USB_NET_TXQ_LEN     MYNEWT_VAL(USBD_NET_TX_QUEUE_LEN)
/* ECM opens data endpoints only when host selects data alternate setting */
#define USB_NET_IS_ECM      MYNEWT_VAL_CHOICE(USBD_NET, ECM)

struct usb_net {
    struct netif nif;
    /* Device side MAC address */
    uint8_t mac[ETHARP_HWADDR_LEN];
    /* Frames queued by lwIP, sent from USB task */
    struct pbuf *txq[USB_NET_TXQ_LEN];
    uint8_t txq_head;
    volatile uint8_t txq_count;
    /* Deferred service function is queued in USB task */
    volatile bool service_pending;
    /* Bus reset or disconnect seen since last service run */
    volatile bool reset_seen;
    /* ECM data interface activated by host since last reset */
    volatile bool ecm_active;
    /* Data path usable, changed by USB task only */
    volatile bool data_up;
    /* Link state change message for tcpip thread */
    struct tcpip_callback_msg *link_msg;
    uint32_t rx_drops;
    uint32_t tx_drops;
};

static struct usb_net usb_net;

/* Host side MAC address, reported to host by TinyUSB network function */
uint8_t tud_network_mac_address[6];

#if defined(MYNEWT_VAL_USBD_NET_MAC_ADDR)
#define _Args(...)              __VA_ARGS__
#define STRIP_PARENS(X)         X
#define UNMANGLE_MYNEWT_VAL(X)  STRIP_PARENS(_Args X)

static const uint8_t usb_net_cfg_mac[ETHARP_HWADDR_LEN] = {
    UNMANGLE_MYNEWT_VAL(MYNEWT_VAL_USBD_NET_MAC_ADDR)
};
#endif

static void usb_net_service(void *arg);

#if MYNEWT_VAL(USBD_NET_DHCP_SERVER)
#if MYNEWT_VAL(USBD_NET_DHCP_POOL_SIZE) > MYNEWT_VAL(DHCP_SERVER_MAX_LEASES)
#error "USBD_NET_DHCP_POOL_SIZE exceeds DHCP_SERVER_MAX_LEASES"
#endif

static const struct dhcp_server_cfg usb_net_dhcps_cfg = {
    .pool_start = MYNEWT_VAL(USBD_NET_DHCP_POOL_START),
    .pool_size = MYNEWT_VAL(USBD_NET_DHCP_POOL_SIZE),
    .lease_time = MYNEWT_VAL(USBD_NET_DHCP_LEASE_TIME),
};
#endif

struct netif *
usb_net_lwip_netif(void)
{
    return &usb_net.nif;
}

/*
 * Device and host MAC addresses: from syscfg, or locally administered
 * unicast address derived from hardware id so each board is different.
 * Host address differs from device address in last bit.
 */
static void
usb_net_mac_init(void)
{
#if defined(MYNEWT_VAL_USBD_NET_MAC_ADDR)
    memcpy(usb_net.mac, usb_net_cfg_mac, ETHARP_HWADDR_LEN);
#else
    uint8_t id[32];
    uint32_t hash = 2166136261u;
    int len;
    int i;

    /* FNV-1a hash of hardware id */
    len = hal_bsp_hw_id(id, sizeof(id));
    for (i = 0; i < len; ++i) {
        hash = (hash ^ id[i]) * 16777619u;
    }
    usb_net.mac[0] = 0x02;
    usb_net.mac[1] = (uint8_t)(hash >> 24);
    usb_net.mac[2] = (uint8_t)(hash >> 16);
    usb_net.mac[3] = (uint8_t)(hash >> 8);
    usb_net.mac[4] = (uint8_t)hash;
    hash = (hash ^ 0xA5) * 16777619u;
    usb_net.mac[5] = (uint8_t)(hash >> 24);
#endif
    memcpy(tud_network_mac_address, usb_net.mac, ETHARP_HWADDR_LEN);
    tud_network_mac_address[5] ^= 1;
}

/*
 * Requests usb_net_service() to run in USB task.  At most one request is
 * queued so USB task queue is never flooded.
 */
static void
usb_net_schedule(bool in_isr)
{
    os_sr_t sr;
    bool pending;

    OS_ENTER_CRITICAL(sr);
    pending = usb_net.service_pending;
    usb_net.service_pending = true;
    OS_EXIT_CRITICAL(sr);

    if (!pending) {
        usbd_defer_func(usb_net_service, NULL, in_isr);
    }
}

static struct pbuf *
usb_net_txq_peek(void)
{
    struct pbuf *p = NULL;
    os_sr_t sr;

    OS_ENTER_CRITICAL(sr);
    if (usb_net.txq_count) {
        p = usb_net.txq[usb_net.txq_head];
    }
    OS_EXIT_CRITICAL(sr);

    return p;
}

static void
usb_net_txq_pop(void)
{
    os_sr_t sr;

    OS_ENTER_CRITICAL(sr);
    usb_net.txq_head = (usb_net.txq_head + 1) % USB_NET_TXQ_LEN;
    usb_net.txq_count--;
    OS_EXIT_CRITICAL(sr);
}

static void
usb_net_txq_flush(void)
{
    struct pbuf *p;

    while ((p = usb_net_txq_peek()) != NULL) {
        usb_net_txq_pop();
        pbuf_free(p);
    }
}

/* Runs in tcpip thread, applies data path state to lwIP link state */
static void
usb_net_link_cb(void *arg)
{
    if (usb_net.data_up) {
        netif_set_link_up(&usb_net.nif);
    } else {
        netif_set_link_down(&usb_net.nif);
    }
}

/* Runs in USB task */
static void
usb_net_service(void *arg)
{
    struct pbuf *p;
    os_sr_t sr;
    bool up;

    OS_ENTER_CRITICAL(sr);
    usb_net.service_pending = false;
    if (usb_net.reset_seen) {
        usb_net.reset_seen = false;
        usb_net.ecm_active = false;
    }
    OS_EXIT_CRITICAL(sr);

    up = tud_mounted() && !tud_suspended();
#if USB_NET_IS_ECM
    up = up && usb_net.ecm_active;
#endif
    if (up != usb_net.data_up) {
        usb_net.data_up = up;
        if (!up) {
            usb_net_txq_flush();
        }
        if (tcpip_callbackmsg_trycallback(usb_net.link_msg) != ERR_OK) {
            /* tcpip mailbox full, wait for space */
            tcpip_callback(usb_net_link_cb, NULL);
        }
    }

    while (usb_net.data_up) {
        p = usb_net_txq_peek();
        if (p == NULL) {
            break;
        }
        if (!tud_network_can_xmit(p->tot_len)) {
            /* Retried when transfer completes, see tud_event_hook_cb() */
            break;
        }
        /* Frame is copied by tud_network_xmit_cb(), pbuf can be released */
        tud_network_xmit(p, 0);
        usb_net_txq_pop();
        pbuf_free(p);
    }
}

/*
 * Called by TinyUSB for each event put in USB task queue, possibly from
 * interrupt.  Events that may change USB state or free transmit buffer
 * schedule service function.
 */
void
tud_event_hook_cb(uint8_t rhport, uint32_t eventid, bool in_isr)
{
    switch (eventid) {
    case DCD_EVENT_BUS_RESET:
    case DCD_EVENT_UNPLUGGED:
        usb_net.reset_seen = true;
        usb_net_schedule(in_isr);
        break;
    case DCD_EVENT_SUSPEND:
    case DCD_EVENT_RESUME:
    case DCD_EVENT_SETUP_RECEIVED:
        usb_net_schedule(in_isr);
        break;
    case DCD_EVENT_XFER_COMPLETE:
        if (usb_net.txq_count) {
            usb_net_schedule(in_isr);
        }
        break;
    default:
        /* SOF and deferred function calls are ignored */
        break;
    }
}

/*
 * Host sets packet filter after it activated network function (ECM, NCM).
 * Runs in USB task.
 */
void
tud_network_set_packet_filter_cb(uint16_t packet_filter)
{
    usb_net.ecm_active = true;
    usb_net_schedule(false);
}

/* Frame received from host, runs in USB task */
bool
tud_network_recv_cb(const uint8_t *src, uint16_t size)
{
    struct pbuf *p;

    if (size > 0) {
#if USB_NET_IS_ECM
        if (!usb_net.ecm_active) {
            /* Data received so data interface is active */
            usb_net.ecm_active = true;
            usb_net_schedule(false);
        }
#endif
        p = pbuf_alloc(PBUF_RAW, size, PBUF_POOL);
        if (p == NULL) {
            usb_net.rx_drops++;
        } else {
            pbuf_take(p, src, size);
            if (usb_net.nif.input(p, &usb_net.nif) != ERR_OK) {
                usb_net.rx_drops++;
                pbuf_free(p);
            }
        }
    }
    /*
     * Frame was copied, let driver receive next one.  This is needed also
     * for empty frames, otherwise reception stops.
     */
    tud_network_recv_renew();

    return true;
}

/* Copies frame to TinyUSB buffer, called from tud_network_xmit() */
uint16_t
tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg)
{
    struct pbuf *p = ref;

    return pbuf_copy_partial(p, dst, p->tot_len, 0);
}

/* lwIP output, called with tcpip core lock held */
static err_t
usb_net_linkoutput(struct netif *nif, struct pbuf *p)
{
    os_sr_t sr;
    uint8_t idx;

    if (!usb_net.data_up) {
        return ERR_IF;
    }
    if (p->tot_len > CFG_TUD_NET_MTU) {
        usb_net.tx_drops++;
        return ERR_IF;
    }

    pbuf_ref(p);
    OS_ENTER_CRITICAL(sr);
    if (usb_net.txq_count >= USB_NET_TXQ_LEN) {
        OS_EXIT_CRITICAL(sr);
        pbuf_free(p);
        usb_net.tx_drops++;
        return ERR_MEM;
    }
    idx = (usb_net.txq_head + usb_net.txq_count) % USB_NET_TXQ_LEN;
    usb_net.txq[idx] = p;
    usb_net.txq_count++;
    OS_EXIT_CRITICAL(sr);

    usb_net_schedule(false);

    return ERR_OK;
}

static err_t
usb_net_if_init(struct netif *nif)
{
    nif->name[0] = 'u';
    nif->name[1] = 's';
    nif->output = etharp_output;
#if LWIP_IPV6
    nif->output_ip6 = ethip6_output;
#endif
    nif->linkoutput = usb_net_linkoutput;
    nif->mtu = CFG_TUD_NET_MTU - SIZEOF_ETH_HDR;
    nif->hwaddr_len = ETHARP_HWADDR_LEN;
    memcpy(nif->hwaddr, usb_net.mac, ETHARP_HWADDR_LEN);
    nif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP |
                 NETIF_FLAG_ETHERNET;
#if LWIP_IGMP
    nif->flags |= NETIF_FLAG_IGMP;
#endif
#if LWIP_IPV6 && LWIP_IPV6_MLD
    nif->flags |= NETIF_FLAG_MLD6;
#endif

    return ERR_OK;
}

void
usb_net_lwip_init(void)
{
    ip4_addr_t addr;
    ip4_addr_t mask;
    struct netif *nif;
    int rc;

    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    usb_net_mac_init();

    rc = ip4addr_aton(MYNEWT_VAL(USBD_NET_IP_ADDR), &addr);
    SYSINIT_PANIC_ASSERT(rc);
    rc = ip4addr_aton(MYNEWT_VAL(USBD_NET_NETMASK), &mask);
    SYSINIT_PANIC_ASSERT(rc);

    usb_net.link_msg = tcpip_callbackmsg_new(usb_net_link_cb, NULL);
    SYSINIT_PANIC_ASSERT(usb_net.link_msg != NULL);

    LOCK_TCPIP_CORE();
    nif = netif_add(&usb_net.nif, &addr, &mask, IP4_ADDR_ANY4, NULL,
                    usb_net_if_init, tcpip_input);
    if (nif != NULL) {
        /* Interface is up, link follows USB state */
        netif_set_up(nif);
#if LWIP_IPV6
        nif->ip6_autoconfig_enabled = 1;
        netif_create_ip6_linklocal_address(nif, 1);
#endif
        /* Other interfaces (e.g. Ethernet) keep default route */
        if (netif_default == NULL) {
            netif_set_default(nif);
        }
#if MYNEWT_VAL(USBD_NET_DHCP_SERVER)
        rc = dhcp_server_start(nif, &usb_net_dhcps_cfg);
#endif
    }
    UNLOCK_TCPIP_CORE();
    SYSINIT_PANIC_ASSERT(nif != NULL);
#if MYNEWT_VAL(USBD_NET_DHCP_SERVER)
    /* Check USBD_NET_DHCP_POOL_* against interface address */
    SYSINIT_PANIC_ASSERT(rc == 0);
#endif
}
