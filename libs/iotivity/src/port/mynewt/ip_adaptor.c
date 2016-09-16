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

#include "../oc_network_events_mutex.h"
#include "../oc_connectivity.h"
#include "oc_buffer.h"

#ifdef OC_SECURITY
#error This implementation does not yet support security
#endif

#define COAP_PORT_UNSECURED (5683)
/* TODO use inet_pton when its available */
const struct mn_in6_addr coap_all_nodes_v6 = {
    .s_addr = {0xFF,0x02,0x00,0x00,0x00,0x00,0x00,0x00,
               0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFD}
};

/* need a task to process OCF messages */
#define OC_NET_TASK_STACK_SIZE          OS_STACK_ALIGN(300)
#define OC_NET_TASK_PRIORITY            (4)
struct os_task oc_task;
os_stack_t *oc_stack;

/* sockets to use for coap unicast and multicast */
struct mn_socket *mcast;
struct mn_socket *ucast;

/* to wake our task when stuff is ready */
struct os_sem oc_read_sem;
struct os_sem oc_write_sem;

/* logging data for this module. TODO, the application should
 * define the logging strategy for this module */
#define MAX_CBMEM_BUF   (600)
static uint32_t *cbmem_buf;
static struct cbmem cbmem;
struct log oc_log;

static int oc_log_init(void) {

    log_init();

    cbmem_buf = malloc(sizeof(uint32_t) * MAX_CBMEM_BUF);
    if (cbmem_buf == NULL) {
        return -1;
    }

    cbmem_init(&cbmem, cbmem_buf, MAX_CBMEM_BUF);
    log_register("iot", &oc_log, &log_cbmem_handler, &cbmem);

    LOG_INFO(&oc_log, LOG_MODULE_IOTIVITY, "OC Init");
    return 0;
}

/* not sure if these semaphores are necessary yet.  If we are running
 * all of this from one task, we may not need these */
static struct os_mutex oc_net_mutex;

void
oc_network_event_handler_mutex_init(void)
{
    os_error_t rc;
    rc = os_mutex_init(&oc_net_mutex);
    assert(rc == 0);
}

void
oc_network_event_handler_mutex_lock(void)
{
    os_mutex_pend(&oc_net_mutex, OS_TIMEOUT_NEVER);
}

void
oc_network_event_handler_mutex_unlock(void)
{
    os_mutex_release(&oc_net_mutex);
}

void
oc_send_buffer(oc_message_t *message)
{
    struct mn_sockaddr_in6 to;
    struct mn_socket * send_sock;
    struct os_mbuf m;
    int rc;

    while (1) {
        LOG_INFO(&oc_log, LOG_MODULE_IOTIVITY,
                 "attempt send buffer %u\n", message->length);

        to.msin6_len = sizeof(to);
        to.msin6_family = MN_AF_INET6;
        to.msin6_scope_id = message->endpoint.ipv6_addr.scope;
        to.msin6_port = htons(message->endpoint.ipv6_addr.port);
        memcpy(&to.msin6_addr, message->endpoint.ipv6_addr.address,
               sizeof(to.msin6_addr));
        send_sock = ucast;

        /* put on an mbuf header to make the socket happy */
        memset(&m,0, sizeof(m));
        m.om_data = message->data;
        m.om_len = message->length;

        rc = mn_sendto(send_sock, &m, (struct mn_sockaddr *) &to);
        /* TODO what to do if this fails, we can't keep the buffer */
        if (rc != 0) {
            LOG_ERROR(&oc_log, LOG_MODULE_IOTIVITY,
                      "Failed sending buffer %u\n", message->length);
        } else {
            break;
        }
        /* if we failed to write, wait around until we can */
        os_sem_pend(&oc_write_sem, OS_TIMEOUT_NEVER);
    }
}

oc_message_t *
oc_attempt_rx(struct mn_socket * rxsock) {
    int rc;
    struct os_mbuf *m = NULL;
    struct os_mbuf_pkthdr *pkt;
    oc_message_t *message = NULL;
    struct mn_sockaddr_in6 from;

    LOG_DEBUG(&oc_log, LOG_MODULE_IOTIVITY, "attempt rx from %u\n", rxsock);

    rc= mn_recvfrom(rxsock, &m, (struct mn_sockaddr *) &from);

    if ( rc != 0) {
        return NULL;
    }

    if (!OS_MBUF_IS_PKTHDR(m)) {
        goto rx_attempt_err;
    }

    pkt = OS_MBUF_PKTHDR(m);

    LOG_DEBUG(&oc_log, LOG_MODULE_IOTIVITY,
              "rx from %u %p-%u\n", rxsock, pkt, pkt->omp_len);

    message = oc_allocate_message();
    if (NULL == message) {
        /* TODO log an error that we dropped a frame */
        goto rx_attempt_err;
    }

    if (pkt->omp_len > MAX_PAYLOAD_SIZE) {
        /* TODO what do we do with this */
        goto rx_attempt_err;
    }
    /* copy to message from mbuf chain */
    rc = os_mbuf_copydata(m, 0, pkt->omp_len, message->data);
    if (rc != 0) {
        /* TODO what do we do with this */
        goto rx_attempt_err;
    }

    os_mbuf_free_chain(m);

    message->endpoint.flags = IP;
    message->length = pkt->omp_len;
    memcpy(&message->endpoint.ipv6_addr.address, &from.msin6_addr,
             sizeof(message->endpoint.ipv6_addr.address));
    message->endpoint.ipv6_addr.scope = from.msin6_scope_id;
    message->endpoint.ipv6_addr.port = ntohs(from.msin6_port);

    LOG_INFO(&oc_log, LOG_MODULE_IOTIVITY, "rx from %u len %u\n",
             rxsock, message->length);

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
oc_attempt_rx_mcast(void) {
    return oc_attempt_rx(mcast);
}

oc_message_t *
oc_attempt_rx_ucast(void) {
    return oc_attempt_rx(ucast);
}

static void oc_socks_readable(void *cb_arg, int err);
static void oc_socks_writable(void *cb_arg, int err);

union mn_socket_cb oc_sock_cbs = {
    .socket.readable = oc_socks_readable,
    .socket.writable = oc_socks_writable
};

void
oc_socks_readable(void *cb_arg, int err)
{
    os_sem_release(&oc_read_sem);
}

void
oc_socks_writable(void *cb_arg, int err)
{
    os_sem_release(&oc_write_sem);
}

void
oc_task_handler(void *arg) {
    while (1) {
        oc_message_t *pmsg;
        os_sem_pend(&oc_read_sem, OS_TIMEOUT_NEVER);
        pmsg = oc_attempt_rx_ucast();
        if (pmsg) {
            oc_network_event(pmsg);
        }

        pmsg = oc_attempt_rx_mcast();
        if (pmsg) {
            oc_network_event(pmsg);
        }
    }
}

static int
oc_init_net_task(void) {
    int rc;

    /* start this thing running to check right away */
    rc = os_sem_init(&oc_read_sem, 1);
    if (0 != rc) {
        LOG_ERROR(&oc_log, LOG_MODULE_IOTIVITY,
                  "Could not initialize oc read sem\n");
        return rc;
    }

    rc = os_sem_init(&oc_write_sem, 1);
    if (0 != rc) {
        LOG_ERROR(&oc_log, LOG_MODULE_IOTIVITY,
                  "Could not initialize oc write sem\n");
        return rc;
    }

    oc_stack = (os_stack_t*) malloc(sizeof(os_stack_t)*OC_NET_TASK_STACK_SIZE);
    if (NULL == oc_stack) {
        LOG_ERROR(&oc_log, LOG_MODULE_IOTIVITY,
                  "Could not malloc oc stack\n");
        return -1;
    }

    rc = os_task_init(&oc_task, "oc", oc_task_handler, NULL,
            OC_NET_TASK_PRIORITY, OS_WAIT_FOREVER,
            oc_stack, OC_NET_TASK_STACK_SIZE);

    if (rc != 0) {
        LOG_ERROR(&oc_log, LOG_MODULE_IOTIVITY, "Could not start oc task\n");
        free(oc_stack);
    }

    return rc;
}

void
oc_connectivity_shutdown(void)
{
    LOG_INFO(&oc_log, LOG_MODULE_IOTIVITY, "OC shutdown");

    if (ucast) {
        mn_close(ucast);
    }

    if (mcast) {
        mn_close(mcast);
    }
}

int
oc_connectivity_init(void)
{
    int rc;
    struct mn_sockaddr_in6 sin;
    struct mn_itf itf;

    memset(&itf, 0, sizeof(itf));

    rc = oc_log_init();
    if ( rc != 0) {
        LOG_ERROR(&oc_log, LOG_MODULE_IOTIVITY,
                  "Could not create oc logging\n");
        return rc;    }

    rc = mn_socket(&ucast, MN_PF_INET6, MN_SOCK_DGRAM, 0);
    if ( rc != 0 || !ucast ) {
        LOG_ERROR(&oc_log, LOG_MODULE_IOTIVITY,
                  "Could not create oc unicast socket\n");
        return rc;
    }
    rc = mn_socket(&mcast, MN_PF_INET6, MN_SOCK_DGRAM, 0);
    if ( rc != 0 || !mcast ) {
        mn_close(ucast);
        LOG_ERROR(&oc_log, LOG_MODULE_IOTIVITY,
                  "Could not create oc multicast socket\n");
        return rc;
    }
    mn_socket_set_cbs(ucast, ucast, &oc_sock_cbs);
    mn_socket_set_cbs(mcast, mcast, &oc_sock_cbs);

    sin.msin6_len = sizeof(sin);
    sin.msin6_family = MN_AF_INET6;
    sin.msin6_port = 0;
    sin.msin6_flowinfo = 0;
    memcpy(&sin.msin6_addr, nm_in6addr_any, sizeof(sin.msin6_addr));

    rc = mn_bind(ucast, (struct mn_sockaddr *)&sin);
    if (rc != 0) {
        LOG_ERROR(&oc_log, LOG_MODULE_IOTIVITY,
                  "Could not bind oc unicast socket\n");
        goto oc_connectivity_init_err;
    }

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
            LOG_ERROR(&oc_log, LOG_MODULE_IOTIVITY,
                 "Could not join multicast group on %s\n", itf.mif_name);
            continue;
        }

        LOG_INFO(&oc_log, LOG_MODULE_IOTIVITY,
                  "Joined Coap multicast grop on %s\n", itf.mif_name);
    }

    sin.msin6_port = htons(COAP_PORT_UNSECURED);
    rc = mn_bind(mcast, (struct mn_sockaddr *)&sin);
    if (rc != 0) {
        LOG_ERROR(&oc_log, LOG_MODULE_IOTIVITY,
                  "Could not bind oc multicast socket\n");
        goto oc_connectivity_init_err;
    }

    rc = oc_init_net_task();
    if (rc != 0) {
        goto oc_connectivity_init_err;
    }

    return 0;

oc_connectivity_init_err:
    oc_connectivity_shutdown();
    return rc;
}

void oc_send_multicast_message(oc_message_t *message)
{
    oc_send_buffer(message);
}