/*
 * Copyright (c) 2016 Intel Corporation
 *
 * Copyright (c) 2013, Institute for Pervasive Computing, ETH Zurich
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 */

#include <stdio.h>
#include <string.h>

#include "os/mynewt.h"

#include "oic/port/mynewt/config.h"

#ifdef OC_SERVER

#include "oic/messaging/coap/observe.h"
#include "oic/messaging/coap/oc_coap.h"
#include "oic/oc_rep.h"
#include "oic/oc_ri.h"

/*-------------------*/
uint64_t observe_counter = 3;
/*---------------------------------------------------------------------------*/
static SLIST_HEAD(, coap_observer) oc_observers;

static struct os_mempool coap_observer_pool;
static uint8_t coap_observer_area[OS_MEMPOOL_BYTES(COAP_MAX_OBSERVERS,
      sizeof(coap_observer_t))];

/*---------------------------------------------------------------------------*/
/*- Internal API ------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
static int
add_observer(oc_resource_t *resource, oc_endpoint_t *endpoint,
             const uint8_t *token, size_t token_len, const char *uri,
             int uri_len)
{
    /* Remove existing observe relationship, if any. */
    int dup = coap_remove_observer_by_uri(endpoint, uri);

    coap_observer_t *o = os_memblock_get(&coap_observer_pool);

    if (o) {
        int max = sizeof(o->url) - 1;
        if (max > uri_len) {
            max = uri_len;
        }
        memcpy(o->url, uri, max);
        o->url[max] = 0;
        memcpy(&o->endpoint, endpoint, oc_endpoint_size(endpoint));
        o->token_len = token_len;
        memcpy(o->token, token, token_len);
        o->last_mid = 0;
        o->obs_counter = observe_counter;
        o->resource = resource;
        resource->num_observers++;
        OC_LOG_DEBUG("Adding observer (%u/%u) for /%s [0x%02X%02X]\n",
          coap_observer_pool.mp_num_blocks - coap_observer_pool.mp_num_free,
          coap_observer_pool.mp_num_blocks, o->url, o->token[0], o->token[1]);
        SLIST_INSERT_HEAD(&oc_observers, o, next);
        return dup;
    }
    return -1;
}
/*---------------------------------------------------------------------------*/
/*- Removal -----------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
void
coap_remove_observer(coap_observer_t *o)
{
    OC_LOG_DEBUG("Removing observer for /%s [0x%02X%02X]\n",
                 o->url, o->token[0], o->token[1]);
    SLIST_REMOVE(&oc_observers, o, coap_observer, next);
    os_memblock_put(&coap_observer_pool, o);
}
/*---------------------------------------------------------------------------*/
int
coap_remove_observer_by_client(oc_endpoint_t *endpoint)
{
    int removed = 0;
    coap_observer_t *obs, *next;

    obs = SLIST_FIRST(&oc_observers);
    while (obs) {
        next = SLIST_NEXT(obs, next);
        if (memcmp(&obs->endpoint, endpoint, oc_endpoint_size(endpoint)) == 0) {
            obs->resource->num_observers--;
            coap_remove_observer(obs);
            removed++;
        }
        obs = next;
    }
    return removed;
}
/*---------------------------------------------------------------------------*/
int
coap_remove_observer_by_token(oc_endpoint_t *endpoint, uint8_t *token,
                              size_t token_len)
{
    int removed = 0;
    coap_observer_t *obs, *next;

    obs = SLIST_FIRST(&oc_observers);
    while (obs) {
        next = SLIST_NEXT(obs, next);
        if (memcmp(&obs->endpoint, endpoint, oc_endpoint_size(endpoint)) == 0 &&
          obs->token_len == token_len &&
          memcmp(obs->token, token, token_len) == 0) {
            obs->resource->num_observers--;
            coap_remove_observer(obs);
            removed++;
            break;
        }
        obs = next;
    }
    return removed;
}
/*---------------------------------------------------------------------------*/
int
coap_remove_observer_by_uri(oc_endpoint_t *endpoint, const char *uri)
{
    int removed = 0;
    coap_observer_t *obs, *next;

    obs = SLIST_FIRST(&oc_observers);
    while (obs) {
        next = SLIST_NEXT(obs, next);
        if (((memcmp(&obs->endpoint, endpoint,
                     oc_endpoint_size(endpoint)) == 0)) &&
          (obs->url == uri || memcmp(obs->url, uri, strlen(obs->url)) == 0)) {
            obs->resource->num_observers--;
            coap_remove_observer(obs);
            removed++;
        }
        obs = next;
    }
    return removed;
}
/*---------------------------------------------------------------------------*/
int
coap_remove_observer_by_mid(oc_endpoint_t *endpoint, uint16_t mid)
{
    int removed = 0;
    coap_observer_t *obs, *next;

    obs = SLIST_FIRST(&oc_observers);
    while (obs) {
        next = SLIST_NEXT(obs, next);
        if (memcmp(&obs->endpoint, endpoint, oc_endpoint_size(endpoint)) == 0 &&
          obs->last_mid == mid) {
            obs->resource->num_observers--;
            coap_remove_observer(obs);
            removed++;
            break;
        }
        obs = next;
    }
    return removed;
}
/*---------------------------------------------------------------------------*/
/*- Notification ------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
int
coap_notify_observers(oc_resource_t *resource,
                      oc_response_buffer_t *response_buf,
                      oc_endpoint_t *endpoint)
{
    int num_observers = 0;
    oc_request_t request = {};
    oc_response_t response = {};
    oc_response_buffer_t response_buffer;
    struct os_mbuf *m = NULL;

    if (resource) {
        if (!resource->num_observers) {
            OC_LOG_DEBUG("coap_notify_observers: no observers left\n");
            return 0;
        }
        num_observers = resource->num_observers;
    }
    response.separate_response = 0;
    if (!response_buf && resource) {
        OC_LOG_DEBUG("coap_notify_observers: Issue GET request to resource\n");
        /* performing GET on the resource */
        m = os_msys_get_pkthdr(0, 0);
        if (!m) {
            /* XXX count */
            return num_observers;
        }
        response_buffer.buffer = m;
        response_buffer.block_offset = NULL;
        response.response_buffer = &response_buffer;
        request.resource = resource;
        request.response = &response;
        oc_rep_new(m);
        resource->get_handler(&request, resource->default_interface);
        response_buf = &response_buffer;
        if (response_buf->code == OC_IGNORE) {
            OC_LOG_ERROR("coap_notify_observers: Resource ignored request\n");
            os_mbuf_free_chain(m);
            return num_observers;
        }
    }

    coap_observer_t *obs = NULL;
    /* iterate over observers */
    for (obs = SLIST_FIRST(&oc_observers); obs; obs = SLIST_NEXT(obs, next)) {
        /* skip if neither resource nor endpoint match */
        if ((resource && resource != obs->resource) ||
            (endpoint && memcmp(&obs->endpoint, endpoint,
                                oc_endpoint_size(endpoint)) != 0)) {
            continue;
        }

        num_observers = obs->resource->num_observers;
#if MYNEWT_VAL(OC_SEPARATE_RESPONSES)
        if (response.separate_response != NULL &&
          response_buf->code == oc_status_code(OC_STATUS_OK)) {
            struct coap_packet_rx req[1];

            req->block1_num = 0;
            req->block1_size = 0;
            req->block2_num = 0;
            req->block2_size = 0;

            req->type = COAP_TYPE_NON;
            req->code = CONTENT_2_05;
            req->mid = 0;
            memcpy(req->token, obs->token, obs->token_len);
            req->token_len = obs->token_len;
            OC_LOG_DEBUG("Resource is SLOW; creating separate response\n");
            if (coap_separate_accept(req, response.separate_response,
                &obs->endpoint, 0) == 1) {
                response.separate_response->active = 1;
            }
        } else {
#endif /* OC_SEPARATE_RESPONSES */
            OC_LOG_DEBUG("coap_notify_observers: notifying observer\n");
            coap_transaction_t *transaction = NULL;
            if (response_buf && (transaction = coap_new_transaction(
                  coap_get_mid(), &obs->endpoint))) {

                /* update last MID for RST matching */
                obs->last_mid = transaction->mid;

                /* prepare response */
                /* build notification */
                coap_packet_t notification[1];
                /* this way the packet can be treated as pointer as usual */
                coap_init_message(notification, COAP_TYPE_NON, CONTENT_2_05, 0);

                notification->mid = transaction->mid;
                if (!oc_endpoint_use_tcp(&obs->endpoint) &&
                    obs->obs_counter % COAP_OBSERVE_REFRESH_INTERVAL == 0) {
                    OC_LOG_DEBUG("coap_observe_notify: forcing CON "
                                 "notification to check for client liveness\n");
                    notification->type = COAP_TYPE_CON;
                }
                coap_set_payload(notification, response_buf->buffer,
                                 OS_MBUF_PKTLEN(response_buf->buffer));
                coap_set_status_code(notification, response_buf->code);
                coap_set_header_content_format(notification, APPLICATION_CBOR);
                if (notification->code < BAD_REQUEST_4_00 &&
                  obs->resource->num_observers) {
                    coap_set_header_observe(notification, (obs->obs_counter)++);
                    observe_counter++;
                } else {
                    coap_set_header_observe(notification, 1);
                }
                coap_set_token(notification, obs->token, obs->token_len);

                if (!coap_serialize_message(notification, transaction->m)) {
                    transaction->type = notification->type;
                    coap_send_transaction(transaction);
                } else {
                    coap_clear_transaction(transaction);
                }
            }
#if MYNEWT_VAL(OC_SEPARATE_RESPONSES)
        }
#endif
    }
    if (m) {
        os_mbuf_free_chain(m);
    }
    return num_observers;
}
/*---------------------------------------------------------------------------*/
int
coap_observe_handler(struct coap_packet_rx *coap_req, coap_packet_t *coap_res,
                     oc_resource_t *resource, oc_endpoint_t *endpoint)
{
    int dup = -1;

    if (coap_req->code == COAP_GET &&
      coap_res->code < 128) { /* GET request and response without error code */
        if (IS_OPTION(coap_req, COAP_OPTION_OBSERVE)) {
            if (coap_req->observe == 0) {
                char uri[COAP_MAX_URI];
                int uri_len;

                uri_len = coap_get_header_uri_path(coap_req, uri, sizeof(uri));
                dup = add_observer(resource, endpoint, coap_req->token,
                                   coap_req->token_len, uri, uri_len);
            } else if (coap_req->observe == 1) {
                /* remove client if it is currently observe */
                dup = coap_remove_observer_by_token(endpoint, coap_req->token,
                                                    coap_req->token_len);
            }
        }
    }
    return dup;
}
/*---------------------------------------------------------------------------*/

void
coap_observe_init(void)
{
    os_mempool_init(&coap_observer_pool, COAP_MAX_OBSERVERS,
      sizeof(coap_observer_t), coap_observer_area, "coap_obs");
}
#endif /* OC_SERVER */
