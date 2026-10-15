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
 * Minimal DHCP server (RFC 2131).
 *
 * Each server instance is bound to one network interface, so it never
 * answers on other interfaces (e.g. Ethernet LAN when serving USB).  Small
 * address pool keyed by client MAC address; no router or DNS options are
 * sent.  Runs in tcpip thread (UDP receive callback).
 */

#include <string.h>

#include <os/mynewt.h>

#include <lwip/udp.h>
#include <lwip/netif.h>
#include <lwip/pbuf.h>
#include <lwip/def.h>
#include <lwip/prot/dhcp.h>
#include <lwip/prot/iana.h>

#include <dhcp_server/dhcp_server.h>

#define DHCPS_MAX_INSTANCES MYNEWT_VAL(DHCP_SERVER_MAX_INSTANCES)
#define DHCPS_MAX_LEASES    MYNEWT_VAL(DHCP_SERVER_MAX_LEASES)

/* Offsets in DHCP message (RFC 2131, figure 1) */
#define DHCPS_OFF_OP        0
#define DHCPS_OFF_HTYPE     1
#define DHCPS_OFF_HLEN      2
#define DHCPS_OFF_XID       4
#define DHCPS_OFF_FLAGS     10
#define DHCPS_OFF_CIADDR    12
#define DHCPS_OFF_YIADDR    16
#define DHCPS_OFF_GIADDR    24
#define DHCPS_OFF_CHADDR    28
#define DHCPS_OFF_COOKIE    236
#define DHCPS_OFF_OPTIONS   240
#define DHCPS_CHADDR_LEN    16

/* Replies are padded to minimal BOOTP message size */
#define DHCPS_REPLY_LEN     300
/* Largest message a client has to accept, larger requests are truncated */
#define DHCPS_MSG_MAX       576

#define DHCPS_HTYPE_ETH     1
#define DHCPS_HLEN_ETH      6

struct dhcps_lease {
    uint8_t mac[DHCPS_HLEN_ETH];
    bool used;
};

struct dhcps {
    struct udp_pcb *pcb;
    struct netif *nif;
    const struct dhcp_server_cfg *cfg;
    struct dhcps_lease leases[DHCPS_MAX_LEASES];
    /* Lease reused when pool is exhausted */
    uint16_t next_evict;
};

static struct dhcps dhcps_instances[DHCPS_MAX_INSTANCES];

/* Received message, shared by all instances, used in tcpip thread only */
static uint8_t dhcps_msg[DHCPS_MSG_MAX];

static void
dhcps_put_be32(uint8_t *dst, uint32_t val)
{
    dst[0] = (uint8_t)(val >> 24);
    dst[1] = (uint8_t)(val >> 16);
    dst[2] = (uint8_t)(val >> 8);
    dst[3] = (uint8_t)val;
}

/* Address of lease idx, network byte order */
static uint32_t
dhcps_lease_addr(const struct dhcps *s, int idx)
{
    uint32_t net;

    net = ip4_addr_get_u32(netif_ip4_addr(s->nif)) &
          ip4_addr_get_u32(netif_ip4_netmask(s->nif));

    return net | lwip_htonl(s->cfg->pool_start + idx);
}

/* Returns lease index for client MAC, -1 if none and alloc is false */
static int
dhcps_lease_find(struct dhcps *s, const uint8_t *mac, bool alloc)
{
    int free_idx = -1;
    int i;

    for (i = 0; i < s->cfg->pool_size; ++i) {
        if (s->leases[i].used) {
            if (memcmp(s->leases[i].mac, mac, DHCPS_HLEN_ETH) == 0) {
                return i;
            }
        } else if (free_idx < 0) {
            free_idx = i;
        }
    }
    if (!alloc) {
        return -1;
    }
    if (free_idx < 0) {
        free_idx = s->next_evict;
        s->next_evict = (s->next_evict + 1) % s->cfg->pool_size;
    }
    memcpy(s->leases[free_idx].mac, mac, DHCPS_HLEN_ETH);
    s->leases[free_idx].used = true;

    return free_idx;
}

/* Returns pointer to data of option code with at least min_len bytes */
static const uint8_t *
dhcps_option(const uint8_t *opt, int len, uint8_t code, uint8_t min_len)
{
    int i = 0;

    while (i < len) {
        if (opt[i] == DHCP_OPTION_PAD) {
            i++;
            continue;
        }
        if (opt[i] == DHCP_OPTION_END || i + 1 >= len ||
            i + 2 + opt[i + 1] > len) {
            break;
        }
        if (opt[i] == code && opt[i + 1] >= min_len) {
            return &opt[i + 2];
        }
        i += 2 + opt[i + 1];
    }

    return NULL;
}

/* Sends OFFER, ACK (yiaddr != 0) or NAK for request req */
static void
dhcps_reply(const struct dhcps *s, uint8_t type, const uint8_t *req,
            uint32_t yiaddr)
{
    struct pbuf *p;
    ip_addr_t dst;
    uint32_t ciaddr;
    uint8_t *r;
    uint8_t *o;

    p = pbuf_alloc(PBUF_TRANSPORT, DHCPS_REPLY_LEN, PBUF_RAM);
    if (p == NULL) {
        return;
    }
    r = p->payload;
    memset(r, 0, DHCPS_REPLY_LEN);

    r[DHCPS_OFF_OP] = DHCP_BOOTREPLY;
    r[DHCPS_OFF_HTYPE] = DHCPS_HTYPE_ETH;
    r[DHCPS_OFF_HLEN] = DHCPS_HLEN_ETH;
    memcpy(r + DHCPS_OFF_XID, req + DHCPS_OFF_XID, 4);
    memcpy(r + DHCPS_OFF_FLAGS, req + DHCPS_OFF_FLAGS, 2);
    if (type == DHCP_ACK) {
        memcpy(r + DHCPS_OFF_CIADDR, req + DHCPS_OFF_CIADDR, 4);
    }
    memcpy(r + DHCPS_OFF_YIADDR, &yiaddr, 4);
    memcpy(r + DHCPS_OFF_GIADDR, req + DHCPS_OFF_GIADDR, 4);
    memcpy(r + DHCPS_OFF_CHADDR, req + DHCPS_OFF_CHADDR, DHCPS_CHADDR_LEN);
    dhcps_put_be32(r + DHCPS_OFF_COOKIE, DHCP_MAGIC_COOKIE);

    o = r + DHCPS_OFF_OPTIONS;
    *o++ = DHCP_OPTION_MESSAGE_TYPE;
    *o++ = 1;
    *o++ = type;
    *o++ = DHCP_OPTION_SERVER_ID;
    *o++ = 4;
    memcpy(o, &ip4_addr_get_u32(netif_ip4_addr(s->nif)), 4);
    o += 4;
    if (type != DHCP_NAK) {
        *o++ = DHCP_OPTION_LEASE_TIME;
        *o++ = 4;
        dhcps_put_be32(o, s->cfg->lease_time);
        o += 4;
        *o++ = DHCP_OPTION_SUBNET_MASK;
        *o++ = 4;
        memcpy(o, &ip4_addr_get_u32(netif_ip4_netmask(s->nif)), 4);
        o += 4;
    }
    *o = DHCP_OPTION_END;

    /* Configured client (renewal) gets unicast, others broadcast */
    memcpy(&ciaddr, req + DHCPS_OFF_CIADDR, 4);
    if (type == DHCP_NAK || ciaddr == 0) {
        ciaddr = IPADDR_BROADCAST;
    }
    ip_addr_set_ip4_u32_val(dst, ciaddr);

    udp_sendto_if(s->pcb, p, &dst, LWIP_IANA_PORT_DHCP_CLIENT, s->nif);
    pbuf_free(p);
}

static void
dhcps_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p,
           const ip_addr_t *addr, u16_t port)
{
    struct dhcps *s = arg;
    const uint8_t *m = dhcps_msg;
    const uint8_t *chaddr;
    const uint8_t *opt;
    uint32_t requested;
    uint32_t lease;
    uint16_t len;
    int opt_len;
    int idx;

    len = pbuf_copy_partial(p, dhcps_msg, sizeof(dhcps_msg), 0);
    pbuf_free(p);

    if (len < DHCPS_OFF_OPTIONS || m[DHCPS_OFF_OP] != DHCP_BOOTREQUEST ||
        m[DHCPS_OFF_HTYPE] != DHCPS_HTYPE_ETH ||
        m[DHCPS_OFF_HLEN] != DHCPS_HLEN_ETH ||
        m[DHCPS_OFF_COOKIE] != 0x63 || m[DHCPS_OFF_COOKIE + 1] != 0x82 ||
        m[DHCPS_OFF_COOKIE + 2] != 0x53 || m[DHCPS_OFF_COOKIE + 3] != 0x63) {
        return;
    }
    chaddr = m + DHCPS_OFF_CHADDR;
    opt_len = len - DHCPS_OFF_OPTIONS;

    opt = dhcps_option(m + DHCPS_OFF_OPTIONS, opt_len,
                       DHCP_OPTION_MESSAGE_TYPE, 1);
    if (opt == NULL) {
        return;
    }

    switch (opt[0]) {
    case DHCP_DISCOVER:
        idx = dhcps_lease_find(s, chaddr, true);
        dhcps_reply(s, DHCP_OFFER, m, dhcps_lease_addr(s, idx));
        break;

    case DHCP_REQUEST:
        opt = dhcps_option(m + DHCPS_OFF_OPTIONS, opt_len,
                           DHCP_OPTION_SERVER_ID, 4);
        if (opt != NULL &&
            memcmp(opt, &ip4_addr_get_u32(netif_ip4_addr(s->nif)), 4) != 0) {
            /* Client selected another server */
            break;
        }
        opt = dhcps_option(m + DHCPS_OFF_OPTIONS, opt_len,
                           DHCP_OPTION_REQUESTED_IP, 4);
        if (opt != NULL) {
            memcpy(&requested, opt, 4);
        } else {
            memcpy(&requested, m + DHCPS_OFF_CIADDR, 4);
        }
        idx = dhcps_lease_find(s, chaddr, true);
        lease = dhcps_lease_addr(s, idx);
        if (requested == lease) {
            dhcps_reply(s, DHCP_ACK, m, lease);
        } else {
            dhcps_reply(s, DHCP_NAK, m, 0);
        }
        break;

    case DHCP_DECLINE:
    case DHCP_RELEASE:
        idx = dhcps_lease_find(s, chaddr, false);
        if (idx >= 0) {
            s->leases[idx].used = false;
        }
        break;

    default:
        break;
    }
}

int
dhcp_server_start(struct netif *nif, const struct dhcp_server_cfg *cfg)
{
    struct dhcps *s = NULL;
    uint32_t host_max;
    uint32_t own;
    int i;

    if (cfg->pool_size == 0 || cfg->pool_size > DHCPS_MAX_LEASES) {
        return SYS_EINVAL;
    }

    /* Pool must fit in subnet and must not contain interface address */
    host_max = ~lwip_ntohl(ip4_addr_get_u32(netif_ip4_netmask(nif)));
    own = lwip_ntohl(ip4_addr_get_u32(netif_ip4_addr(nif))) & host_max;
    if (cfg->pool_start == 0 ||
        cfg->pool_start + cfg->pool_size - 1 >= host_max ||
        (own >= cfg->pool_start && own < cfg->pool_start + cfg->pool_size)) {
        return SYS_EINVAL;
    }

    for (i = 0; i < DHCPS_MAX_INSTANCES; ++i) {
        if (dhcps_instances[i].pcb == NULL) {
            s = &dhcps_instances[i];
            break;
        }
    }
    if (s == NULL) {
        return SYS_ENOMEM;
    }

    memset(s, 0, sizeof(*s));
    s->nif = nif;
    s->cfg = cfg;
    s->pcb = udp_new();
    if (s->pcb == NULL) {
        return SYS_ENOMEM;
    }
    udp_bind_netif(s->pcb, nif);
    if (udp_bind(s->pcb, IP4_ADDR_ANY, LWIP_IANA_PORT_DHCP_SERVER) != ERR_OK) {
        udp_remove(s->pcb);
        s->pcb = NULL;
        return SYS_EBUSY;
    }
    udp_recv(s->pcb, dhcps_recv, s);

    return 0;
}
