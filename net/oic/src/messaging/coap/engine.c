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
#include <stdlib.h>
#include <string.h>

#include "os/mynewt.h"

#include "oic/port/mynewt/config.h"
/* OIC Stack headers */
#include "oic/oc_buffer.h"
#include "oic/oc_ri.h"
#include "messaging/coap/engine.h"

#ifdef OC_CLIENT
#include "oic/oc_client_state.h"
#endif

/*---------------------------------------------------------------------------*/
/*- Internal API ------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
int
coap_receive(struct os_mbuf **mp)
{
    /* static declaration reduces stack peaks and program code size */
    /* this way the packet can be treated as pointer as usual */
    struct os_mbuf *m;
    static struct coap_packet_rx message[1];
    static struct coap_packet response[1];
    static coap_transaction_t *transaction = NULL;
    struct os_mbuf *rsp;
    struct oc_endpoint endpoint; /* XXX */

    erbium_status_code = NO_ERROR;

    OC_LOG_INFO("CoAP: received datalen=%u\n", OS_MBUF_PKTLEN(*mp));

    memcpy(&endpoint, OC_MBUF_ENDPOINT(*mp),
           oc_endpoint_size(OC_MBUF_ENDPOINT(*mp)));
    erbium_status_code = coap_parse_message(message, mp);
    if (erbium_status_code != NO_ERROR) {
        goto out;
    }

    m = *mp;
/*TODO duplicates suppression, if required by application */
    OC_LOG_DEBUG("  Parsed: CoAP version: %u, token: 0x%02X%02X, mid: %u\n",
                 message->version, message->token[0], message->token[1],
                 message->mid);
    switch (message->type) {
    case COAP_TYPE_CON:
        OC_LOG_DEBUG("  type: CON\n");
        break;
    case COAP_TYPE_NON:
        OC_LOG_DEBUG("  type: NON\n");
        break;
    case COAP_TYPE_ACK:
        OC_LOG_DEBUG("  type: ACK\n");
        break;
    case COAP_TYPE_RST:
        OC_LOG_DEBUG("  type: RST\n");
        break;
    default:
        break;
    }

    if (message->code >= COAP_GET && message->code <= COAP_DELETE) {
        /* handle requests */
        switch (message->code) {
        case COAP_GET:
            OC_LOG_DEBUG("  method: GET\n");
            break;
        case COAP_PUT:
            OC_LOG_DEBUG("  method: PUT\n");
            break;
        case COAP_POST:
            OC_LOG_DEBUG("  method: POST\n");
            break;
        case COAP_DELETE:
            OC_LOG_DEBUG("  method: DELETE\n");
            break;
        }

        OC_LOG_DEBUG("  Payload: %d bytes\n", message->payload_len);

        /* use transaction buffer for response to confirmable request */
        transaction = coap_new_transaction(message->mid, OC_MBUF_ENDPOINT(m));
        if (!transaction) {
            erbium_status_code = SERVICE_UNAVAILABLE_5_03;
            coap_error_message = "NoFreeTraBuffer";
            goto out;
        }

        uint32_t block_num = 0;
        uint16_t block_size = COAP_MAX_BLOCK_SIZE;
        uint32_t block_offset = 0;
        int32_t new_offset = 0;

        /* prepare response */
        if (message->type == COAP_TYPE_CON) {
            /* reliable CON requests are answered with an ACK */
            coap_init_message(response, COAP_TYPE_ACK, CONTENT_2_05,
                              message->mid);
        } else {
            /* unreliable NON requests are answered with a NON */
            coap_init_message(response, COAP_TYPE_NON, CONTENT_2_05,
                              coap_get_mid());
            /* mirror token */
        }
        if (message->token_len) {
            coap_set_token(response, message->token, message->token_len);
            /* get offset for blockwise transfers */
        }
        if (coap_get_header_block2(message, &block_num, NULL,
                                   &block_size, &block_offset)) {
            OC_LOG_DEBUG(" Blockwise: block request %u (%u/%u) @ %u bytes\n",
                         (unsigned int) block_num, block_size,
                         COAP_MAX_BLOCK_SIZE, (unsigned int) block_offset);
            block_size = MIN(block_size, COAP_MAX_BLOCK_SIZE);
            new_offset = block_offset;
        }

        if (oc_ri_invoke_coap_entity_handler(message, response, &new_offset,
                                             OC_MBUF_ENDPOINT(m))) {
            if (erbium_status_code == NO_ERROR) {
                /*
                 * TODO coap_handle_blockwise(request, response,
                 * start_offset, end_offset);
                 */

                /* resource is unaware of Block1 */
                if (IS_OPTION(message, COAP_OPTION_BLOCK1) &&
                  response->code < BAD_REQUEST_4_00 &&
                  !IS_OPTION(response, COAP_OPTION_BLOCK1)) {
                    OC_LOG_ERROR(" Block1 option NOT IMPLEMENTED\n");

                    erbium_status_code = NOT_IMPLEMENTED_5_01;
                    coap_error_message = "NoBlock1Support";

                    /* client requested Block2 transfer */
                } else if (IS_OPTION(message, COAP_OPTION_BLOCK2)) {

                    /*
                     * Unchanged new_offset indicates that resource is
                     * unaware of blockwise transfer
                     */
                    if (new_offset == block_offset) {
                        OC_LOG_DEBUG(" Block: unaware resource %u/%u\n",
                                     response->payload_len, block_size);
                        if (block_offset >= response->payload_len) {
                            response->code = BAD_OPTION_4_02;
                            rsp = os_msys_get_pkthdr(0, 0);
                            if (rsp) {
                                os_mbuf_copyinto(rsp, 0, "BlockOutOfScope", 15);
                                response->payload_m = rsp;
                                response->payload_len = 15;
                            }
                            /* a const char str[] and sizeof(str)
                               produces larger code size */
                        } else {
                            coap_set_header_block2(response, block_num,
                                         response->payload_len - block_offset >
                                           block_size, block_size);
                            response->payload_len = MIN(response->payload_len -
                                                        block_offset,
                                                        block_size);
                        } /* if(valid offset) */

                        /* resource provides chunk-wise data */
                    } else {
                        OC_LOG_DEBUG(" Block: aware resource, off %d\n",
                                     (int) new_offset);
                        coap_set_header_block2(response, block_num,
                                               new_offset != -1 ||
                                         response->payload_len > block_size,
                                               block_size);

                        if (response->payload_len > block_size) {
                            response->payload_len = block_size;
                        }
                    } /* if(resource aware of blockwise) */

                    /* Resource requested Block2 transfer */
                } else if (new_offset != 0) {
                    OC_LOG_DEBUG(" block: no block option, using block sz %u\n",
                                 COAP_MAX_BLOCK_SIZE);

                    coap_set_header_block2(response, 0, new_offset != -1,
                                           COAP_MAX_BLOCK_SIZE);
                    response->payload_len = MIN(response->payload_len,
                                                COAP_MAX_BLOCK_SIZE);
                } /* blockwise transfer handling */
            }   /* no errors/hooks */
            /* successful service callback */
            /* serialize response */
        }
        if (erbium_status_code == NO_ERROR) {
            if (coap_serialize_message(response, transaction->m)) {
                erbium_status_code = PACKET_SERIALIZATION_ERROR;
            }
            transaction->type = response->type;
        }
    } else { // Fix this
        /* handle responses */
        if (message->type == COAP_TYPE_CON) {
            erbium_status_code = EMPTY_ACK_RESPONSE;
        } else if (message->type == COAP_TYPE_ACK) {
            /* transactions are closed through lookup below */
        } else if (message->type == COAP_TYPE_RST) {
#ifdef OC_SERVER
            /* cancel possible subscriptions */
            coap_remove_observer_by_mid(OC_MBUF_ENDPOINT(m), message->mid);
#endif
        }

        /* Open transaction now cleared for ACK since mid matches */
        if ((transaction = coap_get_transaction_by_mid(message->mid))) {
            coap_clear_transaction(transaction);
        }
        /* if(ACKed transaction) */
        transaction = NULL;

#ifdef OC_CLIENT
        /*
         * ACKs and RSTs sent to oc_ri.. RSTs cleared, ACKs sent to
         * client.
         */
        oc_ri_invoke_client_cb(message, OC_MBUF_ENDPOINT(m));
#endif

    } /* request or response */

out:
    /* if(parsed correctly) */
    if (erbium_status_code == NO_ERROR) {
        if (transaction) { // Server transactions sent from here
            coap_send_transaction(transaction);
        }
    } else if (erbium_status_code == CLEAR_TRANSACTION) {
        OC_LOG_DEBUG(" Clearing transaction for manual response\n");
        /* used in server for separate response */
        coap_clear_transaction(transaction);
    }
#ifdef OC_CLIENT
    else if (erbium_status_code == EMPTY_ACK_RESPONSE) {
        coap_init_message(response, COAP_TYPE_ACK, 0, message->mid);
        struct os_mbuf *m_rsp = oc_allocate_mbuf(&endpoint);
        if (m_rsp) {
            if (!coap_serialize_message(response, m_rsp)) {
                coap_send_message(m_rsp, 0);
            } else {
                os_mbuf_free_chain(m_rsp);
            }
        }
    }
#endif /* OC_CLIENT */
#ifdef OC_SERVER
    else { // framework errors handled here
        coap_message_type_t reply_type = COAP_TYPE_RST;

        coap_clear_transaction(transaction);

        coap_init_message(response, reply_type, SERVICE_UNAVAILABLE_5_03,
                          message->mid);

        struct os_mbuf *m_rsp = oc_allocate_mbuf(&endpoint);
        if (m_rsp) {
            if (!coap_serialize_message(response, m_rsp)) {
                coap_send_message(m_rsp, 0);
            } else {
                os_mbuf_free_chain(m_rsp);
            }
        }
    }
#endif /* OC_SERVER */

    /* if(new data) */
    return erbium_status_code;
}

void
coap_engine_init(void)
{
    coap_init_connection();
    coap_transaction_init();
#ifdef OC_SERVER
#if MYNEWT_VAL(OC_SEPARATE_RESPONSES)
    coap_separate_init();
#endif
    coap_observe_init();
#endif
}
