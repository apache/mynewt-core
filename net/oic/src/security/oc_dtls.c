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

#ifdef OC_SECURITY

#include "oc_dtls.h"
#include "api/oc_events.h"
#include "config.h"
#include "oc_acl.h"
#include "oc_buffer.h"
#include "oc_core_res.h"
#include "oc_cred.h"
#include "oc_pstat.h"
#include "oc_svr.h"

OC_PROCESS(oc_dtls_handler, "DTLS Process");
OC_MEMB(dtls_peers_s, oc_sec_dtls_peer_t, MAX_DTLS_PEERS);
OC_LIST(dtls_peers);

static dtls_context_t *ocf_dtls_context;

oc_sec_dtls_peer_t *
oc_sec_dtls_get_peer(oc_endpoint_t *endpoint)
{
    oc_sec_dtls_peer_t *peer = oc_list_head(dtls_peers);

    while (peer != NULL) {
        if (memcmp(&peer->session.addr, endpoint,
            oc_endpoint_size(endpoint)) == 0)
            break;
        peer = oc_list_item_next(peer);
    }
    return peer;
}

void
oc_sec_dtls_remove_peer(oc_endpoint_t *endpoint)
{
    oc_sec_dtls_peer_t *peer = oc_sec_dtls_get_peer(endpoint);

    if (peer) {
        LOG("\n\noc_sec_dtls: removed peer\n\n");
        oc_list_remove(dtls_peers, peer);
        oc_memb_free(&dtls_peers_s, peer);
    }
}

oc_event_callback_retval_t
oc_sec_dtls_inactive(void *data)
{
    LOG("\n\noc_sec_dtls: DTLS inactivity callback\n\n");
    oc_sec_dtls_peer_t *peer = oc_sec_dtls_get_peer(data);
    if (peer) {
        oc_clock_time_t time = oc_clock_time();
        time -= peer->timestamp;
        if (time < DTLS_INACTIVITY_TIMEOUT * OC_CLOCK_SECOND) {
            LOG("\n\noc_sec_dtls: Resetting DTLS inactivity callback\n\n");
            return CONTINUE;
        } else if (time < 2 * DTLS_INACTIVITY_TIMEOUT * OC_CLOCK_SECOND) {
            LOG("\n\noc_sec_dtls: Initiating connection close\n\n");
            oc_sec_dtls_close_init(data);
            return CONTINUE;
        } else {
            LOG("\n\noc_sec_dtls: Completing connection close\n\n");
            oc_sec_dtls_close_finish(data);
        }
    } else {
        LOG("\n\noc_sec_dtls: Could not find peer\n\n");
        LOG("oc_sec_dtls: Num active peers %d\n", oc_list_length(dtls_peers));
    }
    LOG("\n\noc_sec_dtls: Terminating DTLS inactivity callback\n\n");
    return DONE;
}

oc_sec_dtls_peer_t *
oc_sec_dtls_add_peer(oc_endpoint_t *endpoint)
{
    oc_sec_dtls_peer_t *peer = oc_sec_dtls_get_peer(endpoint);

    if (!peer) {
        peer = oc_memb_alloc(&dtls_peers_s);
        if (peer) {
            LOG("\n\noc_sec_dtls: Allocating new DTLS peer\n\n");
            memcpy(&peer->session.addr, endpoint, sizeof(oc_endpoint_t));
            peer->session.size = sizeof(oc_endpoint_t);
            OC_LIST_STRUCT_INIT(peer, send_queue);
            peer->connected = false;
            oc_list_add(dtls_peers, peer);

            oc_ri_add_timed_event_callback_seconds(&peer->session.addr,
                         oc_sec_dtls_inactive, DTLS_INACTIVITY_TIMEOUT);
        }
    }
    return peer;
}

bool
oc_sec_dtls_connected(oc_endpoint_t *endpoint)
{
    oc_sec_dtls_peer_t *peer = oc_sec_dtls_get_peer(endpoint);

    if (peer) {
        return peer->connected;
    }
    return false;
}

oc_uuid_t *
oc_sec_dtls_get_peer_uuid(oc_endpoint_t *endpoint)
{
    oc_sec_dtls_peer_t *peer = oc_sec_dtls_get_peer(endpoint);

    if (peer) {
        return &peer->uuid;
    }
    return NULL;
}

/*
  Called back from DTLS state machine following decryption so
  application can read incoming message.
  Following function packages up incoming data into a messaage
  to forward up to CoAP
*/
static int
oc_sec_dtls_get_decrypted_message(struct dtls_context_t *ctx,
                                  session_t *session, uint8_t *buf, size_t len)
{
    oc_message_t *message = oc_allocate_message();
    if (message) {
        memcpy(&message->endpoint, &session->addr, sizeof(oc_endpoint_t));
        memcpy(message->data, buf, len);
        message->length = len;
        oc_recv_message(message);
    }
    return 0;
}

void
oc_sec_dtls_init_connection(oc_message_t *message)
{
    oc_sec_dtls_peer_t *peer = oc_sec_dtls_add_peer(&message->endpoint);

    if (peer) {
        LOG("\n\noc_dtls: Initializing DTLS connection\n\n");
        dtls_connect(ocf_dtls_context, &peer->session);
        oc_list_add(peer->send_queue, message);
    } else {
        oc_message_unref(message);
    }
}

/*
  Called from app layer via buffer.c to post OCF responses...
  Message routed to this function on spoting SECURE flag in
  endpoint structure. This would've already been set on receipt
  of the request (to which this the current message is the response)
  We call dtls_write(...) to feed response data through the
  DTLS state machine leading up to the encrypted send callback below.

  Message sent here may have been flagged to get freed OR
  may have been stored for retransmissions.
*/
int
oc_sec_dtls_send_message(oc_message_t *message)
{
    int ret = 0;
    oc_sec_dtls_peer_t *peer = oc_sec_dtls_get_peer(&message->endpoint);

    if (peer) {
        ret = dtls_write(ocf_dtls_context, &peer->session, message->data,
                         message->length);
    }
    oc_message_unref(message);
    return ret;
}

/*
  Called back from DTLS state machine when it is ready to send
  an encrypted response to the remote endpoint.
  Construct a new oc_message for this purpose and call oc_send_buffer
  to send this message over the wire.
*/
static int
oc_sec_dtls_send_encrypted_message(struct dtls_context_t *ctx,
                                   session_t *session, uint8_t *buf, size_t len)
{
    oc_message_t message;
    memcpy(&message.endpoint, &session->addr, sizeof(oc_endpoint_t));
    memcpy(message.data, buf, len);
    message.length = len;
    oc_send_buffer(&message);
    return len;
}

/*
  This is called once during the handshake process over normal
  operation.
  OwnerPSK woud've been generated previously during provisioning.
*/
static int
oc_sec_dtls_get_owner_psk(struct dtls_context_t *ctx, const session_t *session,
                          dtls_credentials_type_t type,
                          const unsigned char *desc, size_t desc_len,
                          unsigned char *result, size_t result_length)
{
    switch (type) {
    case DTLS_PSK_IDENTITY:
    case DTLS_PSK_HINT: {
        LOG("Identity\n");
        oc_uuid_t *uuid = oc_core_get_device_id(0);
        memcpy(result, uuid->id, 16);
        return 16;
    }
        break;
    case DTLS_PSK_KEY: {
        LOG("key\n");
        oc_sec_cred_t *cred = oc_sec_find_cred((oc_uuid_t *)desc);
        oc_sec_dtls_peer_t *peer =
          oc_sec_dtls_get_peer((oc_endpoint_t *)&session->addr);
        if (cred != NULL && peer != NULL) {
            memcpy(&peer->uuid, (oc_uuid_t *)desc, 16);
            memcpy(result, cred->key, 16);
            return 16;
        }
        return 0;
    }
        break;
    default:
        break;
    }
    return 0;
}

int
oc_sec_dtls_events(struct dtls_context_t *ctx, session_t *session,
                   dtls_alert_level_t level, unsigned short code)
{
    oc_sec_dtls_peer_t *peer = oc_sec_dtls_get_peer(&session->addr);

    if (peer && level == 0 && code == DTLS_EVENT_CONNECTED) {
        peer->connected = true;
        oc_message_t *m = oc_list_pop(peer->send_queue);
        while (m != NULL) {
            oc_sec_dtls_send_message(m);
            m = oc_list_pop(peer->send_queue);
        }
    } else if (level == 2) {
        oc_sec_dtls_close_finish(&session->addr);
    }
    return 0;
}

static dtls_handler_t dtls_cb = {.write = oc_sec_dtls_send_encrypted_message,
                                 .read = oc_sec_dtls_get_decrypted_message,
                                 .event = oc_sec_dtls_events,
                                 .get_psk_info = oc_sec_dtls_get_owner_psk };

void
oc_sec_derive_owner_psk(oc_endpoint_t *endpoint, const char *oxm,
                        const size_t oxm_len, const char *server_uuid,
                        const size_t server_uuid_len, const char *obt_uuid,
                        const size_t obt_uuid_len, uint8_t *key,
                        const size_t key_len)
{
    oc_sec_dtls_peer_t *peer = oc_sec_dtls_get_peer(endpoint);

    if (peer) {
        dtls_prf_with_current_keyblock(
          ocf_dtls_context, &peer->session, oxm, oxm_len, server_uuid,
            server_uuid_len, obt_uuid, obt_uuid_len, (uint8_t *)key, key_len);
    }
}

/*
  Message received from the wire, routed here via buffer.c
  based on examination of the 1st byte proving it is DTLS.
  Data sent to dtls_handle_message(...) for decryption.
*/
static void
oc_sec_dtls_recv_message(oc_message_t *message)
{
    oc_sec_dtls_peer_t *peer = oc_sec_dtls_add_peer(&message->endpoint);

    if (peer) {
        int ret = dtls_handle_message(ocf_dtls_context, &peer->session,
                                  message->data, message->length);
        if (ret != 0) {
            oc_sec_dtls_close_finish(&message->endpoint);
        } else {
            peer->timestamp = oc_clock_time();
        }
    }
    oc_message_unref(message);
}

/* If not owned, select anon_ECDH cipher and enter ready for OTM state */
/* If owned, enter ready for normal operation state */
/* Fetch persisted SVR from app by this time */

void
oc_sec_dtls_init_context(void)
{
    dtls_init();
    ocf_dtls_context = dtls_new_context(NULL);

    if (oc_sec_provisioned()) {
        LOG("\n\noc_sec_dtls: Device in normal operation state\n\n");
        dtls_select_cipher(ocf_dtls_context, TLS_PSK_WITH_AES_128_CCM_8);
    } else {
        LOG("\n\noc_sec_dtls: Device in ready for OTM state\n\n");
        dtls_enables_anon_ecdh(ocf_dtls_context, DTLS_CIPHER_ENABLE);
    }
    dtls_set_handler(ocf_dtls_context, &dtls_cb);
}

void
oc_sec_dtls_close_init(oc_endpoint_t *endpoint)
{
    oc_sec_dtls_peer_t *p = oc_sec_dtls_get_peer(endpoint);
    if (p) {
        dtls_peer_t *peer = dtls_get_peer(ocf_dtls_context, &p->session);
        if (peer) {
            dtls_close(ocf_dtls_context, &p->session);
            oc_message_t *m = oc_list_pop(p->send_queue);
            while (m != NULL) {
                LOG("\n\noc_sec_dtls: Freeing DTLS Peer send queue\n\n");
                oc_message_unref(m);
                m = oc_list_pop(p->send_queue);
            }
        }
    }
}

void
oc_sec_dtls_close_finish(oc_endpoint_t *endpoint)
{
    oc_sec_dtls_peer_t *p = oc_sec_dtls_get_peer(endpoint);
    if (p) {
        dtls_peer_t *peer = dtls_get_peer(ocf_dtls_context, &p->session);
        if (peer) {
            oc_list_remove(ocf_dtls_context->peers, peer);
            dtls_free_peer(peer);
        }
        oc_message_t *m = oc_list_pop(p->send_queue);
        while (m != NULL) {
            LOG("\n\noc_sec_dtls: Freeing DTLS Peer send queue\n\n");
            oc_message_unref(m);
            m = oc_list_pop(p->send_queue);
        }
        oc_sec_dtls_remove_peer(endpoint);
    }
}

OC_PROCESS_THREAD(oc_dtls_handler, ev, data)
{
    OC_PROCESS_BEGIN();

    while (1) {
        OC_PROCESS_YIELD();

        if (ev == oc_events[UDP_TO_DTLS_EVENT]) {
            oc_sec_dtls_recv_message(data);
        } else if (ev == oc_events[INIT_DTLS_CONN_EVENT]) {
            oc_sec_dtls_init_connection(data);
        } else if (ev == oc_events[RI_TO_DTLS_EVENT]) {
            oc_sec_dtls_send_message(data);
        }
    }

    OC_PROCESS_END();
}

#endif /* OC_SECURITY */
