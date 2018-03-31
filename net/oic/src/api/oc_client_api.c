/*
// Copyright (c) 2016 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#include "os/mynewt.h"

#include "oic/port/mynewt/config.h"
#include "oic/messaging/coap/coap.h"
#include "oic/messaging/coap/transactions.h"
#include "oic/oc_api.h"
#include "oic/oc_buffer.h"
#if MYNEWT_VAL(OC_TRANSPORT_IPV6) || MYNEWT_VAL(OC_TRANSPORT_IPV4)
#include "oic/port/mynewt/ip.h"
#endif

#ifdef OC_CLIENT
#define OC_CLIENT_CB_TIMEOUT_SECS COAP_RESPONSE_TIMEOUT

static struct os_mbuf *oc_c_message;
static struct os_mbuf *oc_c_rsp;
static coap_transaction_t *oc_c_transaction;
static coap_packet_t oc_c_request[1];

static bool
dispatch_coap_request(void)
{
    int response_length = oc_rep_finalize();

    if (response_length) {
        oc_c_request->payload_m = oc_c_rsp;
        oc_c_request->payload_len = response_length;
        coap_set_header_content_format(oc_c_request, APPLICATION_CBOR);
    } else {
        os_mbuf_free_chain(oc_c_rsp);
    }
    oc_c_rsp = NULL;

    if (!oc_c_transaction) {
        if (oc_c_message) {
            if (!coap_serialize_message(oc_c_request, oc_c_message)) {
                coap_send_message(oc_c_message, 0);
            } else {
                os_mbuf_free_chain(oc_c_message);
            }
            oc_c_message = NULL;
            return true;
        }
    } else {
        if (!coap_serialize_message(oc_c_request, oc_c_transaction->m)) {
            coap_send_transaction(oc_c_transaction);
        } else {
            coap_clear_transaction(oc_c_transaction);
        }
        oc_c_transaction = NULL;
        return true;
    }
    return false;
}

static bool
prepare_coap_request(oc_client_cb_t *cb, oc_string_t *query)
{
    coap_message_type_t type = COAP_TYPE_NON;

    oc_c_rsp = os_msys_get_pkthdr(0, 0);
    if (!oc_c_rsp) {
        return false;
    }
    if (cb->qos == HIGH_QOS) {
        type = COAP_TYPE_CON;
        oc_c_transaction = coap_new_transaction(cb->mid, &cb->server.endpoint);
        if (!oc_c_transaction) {
            goto free_rsp;
        }
    } else {
        oc_c_message = oc_allocate_mbuf(&cb->server.endpoint);
        if (!oc_c_message) {
            goto free_rsp;
        }
    }
    oc_rep_new(oc_c_rsp);

    coap_init_message(oc_c_request, type, cb->method, cb->mid);
    coap_set_header_accept(oc_c_request, APPLICATION_CBOR);
    coap_set_token(oc_c_request, cb->token, cb->token_len);
    coap_set_header_uri_path(oc_c_request, oc_string(cb->uri));
    if (cb->observe_seq != -1) {
        coap_set_header_observe(oc_c_request, cb->observe_seq);
    }
    if (query && oc_string_len(*query)) {
        coap_set_header_uri_query(oc_c_request, oc_string(*query));
    }
    if (cb->observe_seq == -1 && cb->qos == LOW_QOS) {
        os_callout_reset(&cb->callout,
          OC_CLIENT_CB_TIMEOUT_SECS * OS_TICKS_PER_SEC);
    }

    return true;
free_rsp:
    os_mbuf_free_chain(oc_c_rsp);
    oc_c_rsp = NULL;
    return false;
}

bool
oc_do_delete(const char *uri, oc_server_handle_t *server,
             oc_response_handler_t handler, oc_qos_t qos)
{
    oc_client_cb_t *cb;
    bool status = false;

    cb = oc_ri_alloc_client_cb(uri, server, OC_DELETE, handler, qos);
    if (!cb) {
        return false;
    }

    status = prepare_coap_request(cb, NULL);

    if (status) {
        status = dispatch_coap_request();
    }
    return status;
}

bool
oc_do_get(const char *uri, oc_server_handle_t *server, const char *query,
          oc_response_handler_t handler, oc_qos_t qos)
{
    oc_client_cb_t *cb;
    bool status = false;
    oc_string_t q;

    cb = oc_ri_alloc_client_cb(uri, server, OC_GET, handler, qos);
    if (!cb) {
        return false;
    }

    if (query && strlen(query)) {
        oc_concat_strings(&q, "?", query);
        status = prepare_coap_request(cb, &q);
        oc_free_string(&q);
    } else {
        status = prepare_coap_request(cb, NULL);
    }

    if (status) {
        status = dispatch_coap_request();
    }
    return status;
}

bool
oc_init_put(const char *uri, oc_server_handle_t *server, const char *query,
            oc_response_handler_t handler, oc_qos_t qos)
{
    oc_client_cb_t *cb;
    bool status = false;
    oc_string_t q;

    cb = oc_ri_alloc_client_cb(uri, server, OC_PUT, handler, qos);
    if (!cb) {
        return false;
    }

    if (query && strlen(query)) {
        oc_concat_strings(&q, "?", query);
        status = prepare_coap_request(cb, &q);
        oc_free_string(&q);
    } else {
        status = prepare_coap_request(cb, NULL);
    }

    return status;
}

bool
oc_init_post(const char *uri, oc_server_handle_t *server, const char *query,
             oc_response_handler_t handler, oc_qos_t qos)
{
    oc_client_cb_t *cb;
    bool status = false;
    oc_string_t q;

    cb = oc_ri_alloc_client_cb(uri, server, OC_POST, handler, qos);
    if (!cb) {
        return false;
    }

    if (query && strlen(query)) {
        oc_concat_strings(&q, "?", query);
        status = prepare_coap_request(cb, &q);
        oc_free_string(&q);
    } else {
        status = prepare_coap_request(cb, NULL);
    }

    return status;
}

bool
oc_do_put(void)
{
    return dispatch_coap_request();
}

bool
oc_do_post(void)
{
    return dispatch_coap_request();
}

bool
oc_do_observe(const char *uri, oc_server_handle_t *server, const char *query,
              oc_response_handler_t handler, oc_qos_t qos)
{
    oc_client_cb_t *cb;
    bool status = false;
    oc_string_t q;

    cb = oc_ri_alloc_client_cb(uri, server, OC_GET, handler, qos);
    if (!cb) {
        return false;
    }
    cb->observe_seq = 0;

    if (query && strlen(query)) {
        oc_concat_strings(&q, "?", query);
        status = prepare_coap_request(cb, &q);
        oc_free_string(&q);
    } else {
        status = prepare_coap_request(cb, NULL);
    }

    if (status) {
        status = dispatch_coap_request();
    }
    return status;
}

bool
oc_stop_observe(const char *uri, oc_server_handle_t *server)
{
    oc_client_cb_t *cb;
    bool status = false;

    cb = oc_ri_get_client_cb(uri, server, OC_GET);
    if (!cb) {
        return false;
    }
    cb->observe_seq = 1;


    status = prepare_coap_request(cb, NULL);
    if (status) {
        status = dispatch_coap_request();
    }
    return status;
}

#if MYNEWT_VAL(OC_TRANSPORT_IP)
static bool
oc_send_ip_discovery(oc_server_handle_t *handle, const char *rt,
                     oc_discovery_cb_t handler)
{
    oc_client_cb_t *cb;
    bool status = false;
    oc_string_t query;

    cb = oc_ri_alloc_client_cb("/oic/res", handle, OC_GET, handler, LOW_QOS);
    if (!cb) {
        return false;
    }
    cb->discovery = true;

    if (rt && strlen(rt) > 0) {
        oc_concat_strings(&query, "if=oic.if.ll&rt=", rt);
    } else {
        oc_new_string(&query, "if=oic.if.ll");
    }
    status = prepare_coap_request(cb, &query);
    oc_free_string(&query);

    if (status) {
        status = dispatch_coap_request();
    }
    return status;
}

#if MYNEWT_VAL(OC_TRANSPORT_IPV6)
bool
oc_do_ip6_discovery(const char *rt, oc_discovery_cb_t handler)
{
    oc_server_handle_t handle;

    oc_make_ip6_endpoint(mcast, OC_ENDPOINT_MULTICAST, 5683,
                       0xff, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xfd);
    memcpy(&handle.endpoint, &mcast, sizeof(mcast));

    return oc_send_ip_discovery(&handle, rt, handler);

}
#endif

#if MYNEWT_VAL(OC_TRANSPORT_IPV4)
bool
oc_do_ip4_discovery(const char *rt, oc_discovery_cb_t handler)
{
    oc_server_handle_t handle;

    oc_make_ip4_endpoint(mcast, OC_ENDPOINT_MULTICAST, 5683,
                         0xe0, 0, 0x01, 0xbb);
    memcpy(&handle.endpoint, &mcast, sizeof(mcast));

    return oc_send_ip_discovery(&handle, rt, handler);
}
#endif

bool
oc_do_ip_discovery(const char *rt, oc_discovery_cb_t handler)
{
    bool status = false;

#if (MYNEWT_VAL(OC_TRANSPORT_IP) == 1) && (MYNEWT_VAL(OC_TRANSPORT_IPV6) == 1)
    status = oc_do_ip6_discovery(rt, handler);
#endif
#if (MYNEWT_VAL(OC_TRANSPORT_IP) == 1) && (MYNEWT_VAL(OC_TRANSPORT_IPV4) == 1)
    status = oc_do_ip4_discovery(rt, handler);
#endif
    return status;
}
#endif

#endif /* OC_CLIENT */
