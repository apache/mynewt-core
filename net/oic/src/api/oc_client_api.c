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

#include "messaging/coap/coap.h"
#include "messaging/coap/transactions.h"
#include "oc_api.h"

#ifdef OC_CLIENT
#define OC_CLIENT_CB_TIMEOUT_SECS COAP_RESPONSE_TIMEOUT

static oc_message_t *message;
static coap_transaction_t *transaction;
coap_packet_t request[1];

static bool
dispatch_coap_request(void)
{
  int response_length = oc_rep_finalize();
  if (!transaction) {
    if (message) {
      if (response_length) {
        coap_set_payload(request, message->data + COAP_MAX_HEADER_SIZE,
                         response_length);
        coap_set_header_content_format(request, APPLICATION_CBOR);
      }
      message->length = coap_serialize_message(request, message->data);
      coap_send_message(message);
      message = 0;
      return true;
    }
  } else {
    if (response_length) {
      coap_set_payload(request,
                       transaction->message->data + COAP_MAX_HEADER_SIZE,
                       response_length);
      coap_set_header_content_format(request, APPLICATION_CBOR);
    }
    transaction->message->length =
      coap_serialize_message(request, transaction->message->data);
    coap_send_transaction(transaction);
    transaction = 0;
    return true;
  }
  return false;
}

static bool
prepare_coap_request(oc_client_cb_t *cb, oc_string_t *query)
{
  coap_message_type_t type = COAP_TYPE_NON;

  if (cb->qos == HIGH_QOS) {
    type = COAP_TYPE_CON;
    transaction = coap_new_transaction(cb->mid, &cb->server.endpoint);
    if (!transaction)
      return false;
    oc_rep_new(transaction->message->data + COAP_MAX_HEADER_SIZE,
               COAP_MAX_BLOCK_SIZE);
  } else {
    message = oc_allocate_message();
    if (!message)
      return false;
    memcpy(&message->endpoint, &cb->server.endpoint, sizeof(oc_endpoint_t));
    oc_rep_new(message->data + COAP_MAX_HEADER_SIZE, COAP_MAX_BLOCK_SIZE);
  }

  coap_init_message(request, type, cb->method, cb->mid);

  coap_set_header_accept(request, APPLICATION_CBOR);

  coap_set_token(request, cb->token, cb->token_len);

  coap_set_header_uri_path(request, oc_string(cb->uri));

  if (cb->observe_seq != -1)
    coap_set_header_observe(request, cb->observe_seq);

  if (query && oc_string_len(*query))
    coap_set_header_uri_query(request, oc_string(*query));

  if (cb->observe_seq == -1 && cb->qos == LOW_QOS) {
    extern oc_event_callback_retval_t oc_ri_remove_client_cb(void *data);

    oc_set_delayed_callback(cb, &oc_ri_remove_client_cb,
                            OC_CLIENT_CB_TIMEOUT_SECS);
  }

  return true;
}

bool
oc_do_delete(const char *uri, oc_server_handle_t *server,
             oc_response_handler_t handler, oc_qos_t qos)
{
  oc_client_cb_t *cb =
    oc_ri_alloc_client_cb(uri, server, OC_DELETE, handler, qos);
  if (!cb)
    return false;

  bool status = false;

  status = prepare_coap_request(cb, NULL);

  if (status)
    status = dispatch_coap_request();

  return status;
}

bool
oc_do_get(const char *uri, oc_server_handle_t *server, const char *query,
          oc_response_handler_t handler, oc_qos_t qos)
{
  oc_client_cb_t *cb = oc_ri_alloc_client_cb(uri, server, OC_GET, handler, qos);
  if (!cb)
    return false;

  bool status = false;

  if (query && strlen(query)) {
    oc_string_t q;
    oc_concat_strings(&q, "?", query);
    status = prepare_coap_request(cb, &q);
    oc_free_string(&q);
  } else {
    status = prepare_coap_request(cb, NULL);
  }

  if (status)
    status = dispatch_coap_request();

  return status;
}

bool
oc_init_put(const char *uri, oc_server_handle_t *server, const char *query,
            oc_response_handler_t handler, oc_qos_t qos)
{
  oc_client_cb_t *cb = oc_ri_alloc_client_cb(uri, server, OC_PUT, handler, qos);
  if (!cb)
    return false;

  bool status = false;

  if (query && strlen(query)) {
    oc_string_t q;
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
  oc_client_cb_t *cb =
    oc_ri_alloc_client_cb(uri, server, OC_POST, handler, qos);
  if (!cb)
    return false;

  bool status = false;

  if (query && strlen(query)) {
    oc_string_t q;
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
  oc_client_cb_t *cb = oc_ri_alloc_client_cb(uri, server, OC_GET, handler, qos);
  if (!cb)
    return false;

  cb->observe_seq = 0;

  bool status = false;

  if (query && strlen(query)) {
    oc_string_t q;
    oc_concat_strings(&q, "?", query);
    status = prepare_coap_request(cb, &q);
    oc_free_string(&q);
  } else {
    status = prepare_coap_request(cb, NULL);
  }

  if (status)
    status = dispatch_coap_request();

  return status;
}

bool
oc_stop_observe(const char *uri, oc_server_handle_t *server)
{
  oc_client_cb_t *cb = oc_ri_get_client_cb(uri, server, OC_GET);

  if (!cb)
    return false;

  cb->observe_seq = 1;

  bool status = false;

  status = prepare_coap_request(cb, NULL);

  if (status)
    status = dispatch_coap_request();

  return status;
}

bool
oc_do_ip_discovery(const char *rt, oc_discovery_cb_t handler)
{
  oc_make_ip_endpoint(mcast, IP | MULTICAST, 5683, 0xff, 0x02, 0, 0, 0, 0, 0, 0,
                      0, 0, 0, 0, 0, 0, 0, 0xfd);
  mcast.ipv6_addr.scope = 0;

  oc_server_handle_t handle;
  memcpy(&handle.endpoint, &mcast, sizeof(oc_endpoint_t));

  oc_client_cb_t *cb =
    oc_ri_alloc_client_cb("/oic/res", &handle, OC_GET, handler, LOW_QOS);

  if (!cb)
    return false;

  cb->discovery = true;

  bool status = false;

  oc_string_t query;

  if (rt && strlen(rt) > 0) {
    oc_concat_strings(&query, "if=oic.if.ll&rt=", rt);
  } else {
    oc_new_string(&query, "if=oic.if.ll");
  }
  status = prepare_coap_request(cb, &query);
  oc_free_string(&query);

  if (status)
    status = dispatch_coap_request();

  return status;
}
#endif /* OC_CLIENT */
