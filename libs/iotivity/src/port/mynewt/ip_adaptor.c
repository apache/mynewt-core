/**
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
#include <os/os.h>
#include <os/endian.h>
#include <string.h>
#include <log/log.h>
#include <mn_socket/mn_socket.h>

#include "../oc_connectivity.h"
#include "oc_buffer.h"
#include "../oc_log.h"
#include "adaptor.h"

#ifdef OC_TRANSPORT_IP

struct os_event oc_sock_read_event = {
    .ev_type = OC_ADATOR_EVENT_IP,
};

#ifdef OC_SECURITY
#error This implementation does not yet support security
#endif


#define COAP_PORT_UNSECURED (5683)
/* TODO use inet_pton when its available */
const struct mn_in6_addr coap_all_nodes_v6 = {
    .s_addr = {0xFF,0x02,0x00,0x00,0x00,0x00,0x00,0x00,
               0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFD}
};


/* sockets to use for coap unicast and multicast */
struct mn_socket *ucast;

#ifdef OC_SERVER
struct mn_socket *mcast;
#endif

static void
oc_send_buffer_ip_int(oc_message_t *message, int mcast)
{
    struct mn_sockaddr_in6 to;
    struct os_mbuf m;
    int rc;

    LOG("oc_transport_ip attempt send buffer %lu\n", message->length);

    to.msin6_len = sizeof(to);
    to.msin6_family = MN_AF_INET6;

    to.msin6_port = htons(message->endpoint.ipv6_addr.port);
    memcpy(&to.msin6_addr, message->endpoint.ipv6_addr.address,
           sizeof(to.msin6_addr));

    /* put on an mbuf header to make the socket happy */
    memset(&m,0, sizeof(m));
    m.om_data = message->data;
    m.om_len = message->length;
    to.msin6_scope_id = message->endpoint.ipv6_addr.scope;

    if (mcast) {
        struct mn_itf itf;
        memset(&itf, 0, sizeof(itf));

        while (1) {
            rc = mn_itf_getnext(&itf);
            if (rc) {
                break;
            }

            if (0 == (itf.mif_flags & MN_ITF_F_UP)) {
                continue;
            }

            to.msin6_scope_id = itf.mif_idx;

            rc = mn_sendto(ucast, &m, (struct mn_sockaddr *) &to);
            if (rc != 0) {
                ERROR("Failed sending buffer %lu on itf %d\n",
                      message->length, to.msin6_scope_id);
            }
        }
    } else {
        rc = mn_sendto(ucast, &m, (struct mn_sockaddr *) &to);
        if (rc != 0) {
            ERROR("Failed sending buffer %lu on itf %d\n",
                  message->length, to.msin6_scope_id);
        }
    }
    oc_message_unref(message);
}

void
oc_send_buffer_ip(oc_message_t *message) {
    oc_send_buffer_ip_int(message, 0);
}
void
oc_send_buffer_ip_mcast(oc_message_t *message) {
    oc_send_buffer_ip_int(message, 1);
}

oc_message_t *
oc_attempt_rx_ip_sock(struct mn_socket * rxsock) {
    int rc;
    struct os_mbuf *m = NULL;
    struct os_mbuf_pkthdr *pkt;
    oc_message_t *message = NULL;
    struct mn_sockaddr_in6 from;

    LOG("oc_transport_ip attempt rx from %p\n", rxsock);

    rc= mn_recvfrom(rxsock, &m, (struct mn_sockaddr *) &from);

    if ( rc != 0) {
        return NULL;
    }

    if (!OS_MBUF_IS_PKTHDR(m)) {
        goto rx_attempt_err;
    }

    pkt = OS_MBUF_PKTHDR(m);

    LOG("rx from %p %p-%u\n", rxsock, pkt, pkt->omp_len);

    message = oc_allocate_message();
    if (NULL == message) {
        ERROR("Could not allocate OC message buffer\n");
        goto rx_attempt_err;
    }

    if (pkt->omp_len > MAX_PAYLOAD_SIZE) {
        ERROR("Message to large for OC message buffer\n");
        goto rx_attempt_err;
    }
    /* copy to message from mbuf chain */
    rc = os_mbuf_copydata(m, 0, pkt->omp_len, message->data);
    if (rc != 0) {
        ERROR("Failed to copy message from mbuf to OC message buffer \n");
        goto rx_attempt_err;
    }

    os_mbuf_free_chain(m);

    message->endpoint.flags = IP;
    message->length = pkt->omp_len;
    memcpy(&message->endpoint.ipv6_addr.address, &from.msin6_addr,
             sizeof(message->endpoint.ipv6_addr.address));
    message->endpoint.ipv6_addr.scope = from.msin6_scope_id;
    message->endpoint.ipv6_addr.port = ntohs(from.msin6_port);

    LOG("Successfully rx from %p len %lu\n", rxsock, message->length);
    return message;

    /* add the addr info to the message */
rx_attempt_err:
    if (m) {
        os_mbuf_free_chain(m);
    }

    if (message) {
        oc_message_unref(message);
    }

    return NULL;
}

oc_message_t *
oc_attempt_rx_ip(void) {
    oc_message_t *pmsg;
    pmsg = oc_attempt_rx_ip_sock(ucast);
#ifdef OC_SERVER
    if (pmsg == NULL ) {
        pmsg = oc_attempt_rx_ip_sock(mcast);
    }
#endif
    return pmsg;
}

static void oc_socks_readable(void *cb_arg, int err);

union mn_socket_cb oc_sock_cbs = {
    .socket.readable = oc_socks_readable,
    .socket.writable = NULL
};

void
oc_socks_readable(void *cb_arg, int err)
{
    os_eventq_put(&oc_event_q, &oc_sock_read_event);
}

void
oc_connectivity_shutdown_ip(void)
{
    LOG("OC shutdown IP\n");

    if (ucast) {
        mn_close(ucast);
    }

#ifdef OC_SERVER
    if (mcast) {
        mn_close(mcast);
    }
#endif

}

int
oc_connectivity_init_ip(void)
{
    int rc;
    struct mn_sockaddr_in6 sin;
    struct mn_itf itf;

    LOG("OC transport init IP\n");
    memset(&itf, 0, sizeof(itf));

    rc = oc_log_init();
    if ( rc != 0) {
        ERROR("Could not create oc logging\n");
        return rc;    }

    rc = mn_socket(&ucast, MN_PF_INET6, MN_SOCK_DGRAM, 0);
    if ( rc != 0 || !ucast ) {
        ERROR("Could not create oc unicast socket\n");
        return rc;
    }
    mn_socket_set_cbs(ucast, ucast, &oc_sock_cbs);

#ifdef OC_SERVER
    rc = mn_socket(&mcast, MN_PF_INET6, MN_SOCK_DGRAM, 0);
    if ( rc != 0 || !mcast ) {
        mn_close(ucast);
        ERROR("Could not create oc multicast socket\n");
        return rc;
    }
    mn_socket_set_cbs(mcast, mcast, &oc_sock_cbs);
#endif

    sin.msin6_len = sizeof(sin);
    sin.msin6_family = MN_AF_INET6;
    sin.msin6_port = 0;
    sin.msin6_flowinfo = 0;
    memcpy(&sin.msin6_addr, nm_in6addr_any, sizeof(sin.msin6_addr));

    rc = mn_bind(ucast, (struct mn_sockaddr *)&sin);
    if (rc != 0) {
        ERROR("Could not bind oc unicast socket\n");
        goto oc_connectivity_init_err;
    }

#ifdef OC_SERVER
    /* Set socket option to join multicast group on all valid interfaces */
    while (1) {
        struct mn_mreq join;

        rc = mn_itf_getnext(&itf);
        if (rc) {
            break;
        }

        if (0 == (itf.mif_flags & MN_ITF_F_UP)) {
            continue;
        }

        join.mm_addr.v6 = coap_all_nodes_v6;
        join.mm_idx = itf.mif_idx;
        join.mm_family = MN_AF_INET6;

        rc = mn_setsockopt(mcast, MN_SO_LEVEL, MN_MCAST_JOIN_GROUP, &join);
        if (rc != 0) {
            ERROR("Could not join multicast group on %s\n", itf.mif_name);
            continue;
        }

        LOG("Joined Coap multicast grop on %s\n", itf.mif_name);
    }

    sin.msin6_port = htons(COAP_PORT_UNSECURED);
    rc = mn_bind(mcast, (struct mn_sockaddr *)&sin);
    if (rc != 0) {
        ERROR("Could not bind oc multicast socket\n");
        goto oc_connectivity_init_err;
    }
#endif

    return 0;

oc_connectivity_init_err:
    oc_connectivity_shutdown();
    return rc;
}

#endif