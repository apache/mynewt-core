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

#if defined(OC_SERVER) && MYNEWT_VAL(OC_SEPARATE_RESPONSES)

#include "oic/oc_buffer.h"
#include "oic/messaging/coap/separate.h"
#include "oic/messaging/coap/transactions.h"

static struct os_mempool coap_separate_pool;
static uint8_t coap_separate_area[OS_MEMPOOL_BYTES(MAX_NUM_CONCURRENT_REQUESTS,
      sizeof(coap_separate_t))];

/*---------------------------------------------------------------------------*/
/*- Separate Response API ---------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/**
 * \brief Initiate a separate response with an empty ACK
 * \param request The request to accept
 * \param separate_store A pointer to the data structure that will store the
 *   relevant information for the response
 *
 * When the server does not have enough resources left to store the information
 * for a separate response or otherwise cannot execute the resource handler,
 * this function will respond with 5.03 Service Unavailable. The client can
 * then retry later.
 */
int
coap_separate_accept(struct coap_packet_rx *coap_req,
                     oc_separate_response_t *separate_response,
                     oc_endpoint_t *endpoint, int observe)
{
    coap_separate_t *item;
    coap_separate_t *separate_store;

    if (separate_response->active == 0) {
        SLIST_INIT(&separate_response->requests);
    }

    SLIST_FOREACH(item, &separate_response->requests, next) {
        if (item->token_len == coap_req->token_len &&
          memcmp(item->token, coap_req->token, item->token_len) == 0) {
            return 0;
        }
    }

    separate_store = os_memblock_get(&coap_separate_pool);

    if (!separate_store) {
        return 0;
    }

    SLIST_INSERT_HEAD(&separate_response->requests, separate_store, next);

    erbium_status_code = CLEAR_TRANSACTION;
    /* send separate ACK for CON */
    if (coap_req->type == COAP_TYPE_CON) {
        OC_LOG_DEBUG("Sending ACK for separate response\n");
        coap_packet_t ack[1];
        /* ACK with empty code (0) */
        coap_init_message(ack, COAP_TYPE_ACK, 0, coap_req->mid);
        if (observe < 2) {
            coap_set_header_observe(ack, observe);
        }
        coap_set_token(ack, coap_req->token, coap_req->token_len);
        struct os_mbuf *message = oc_allocate_mbuf(endpoint); /* XXXX? */
        if (message != NULL && coap_serialize_message(ack, message) == 0) {
            coap_send_message(message, 0);
        } else {
            coap_separate_clear(separate_response, separate_store);
            erbium_status_code = SERVICE_UNAVAILABLE_5_03;
            return 0;
        }
    }
    memcpy(&separate_store->endpoint, endpoint, oc_endpoint_size(endpoint));

    /* store correct response type */
    separate_store->type = COAP_TYPE_NON;

    memcpy(separate_store->token, coap_req->token, coap_req->token_len);
    separate_store->token_len = coap_req->token_len;

    separate_store->block1_num = coap_req->block1_num;
    separate_store->block1_size = coap_req->block1_size;

    separate_store->block2_num = coap_req->block2_num;
    separate_store->block2_size =
      coap_req->block2_size > 0 ?
      MIN(COAP_MAX_BLOCK_SIZE, coap_req->block2_size) : COAP_MAX_BLOCK_SIZE;

    separate_store->observe = observe;
    return 1;
}
/*----------------------------------------------------------------------------*/
void
coap_separate_resume(coap_packet_t *response, coap_separate_t *separate_store,
                     uint8_t code, uint16_t mid)
{
    coap_init_message(response, separate_store->type, code, mid);
    if (separate_store->token_len) {
        coap_set_token(response, separate_store->token,
          separate_store->token_len);
    }
    if (separate_store->block1_size) {
        coap_set_header_block1(response, separate_store->block1_num, 0,
          separate_store->block1_size);
    }
}
/*---------------------------------------------------------------------------*/
void
coap_separate_clear(oc_separate_response_t *separate_response,
                    coap_separate_t *separate_store)
{
    SLIST_REMOVE(&separate_response->requests, separate_store, coap_separate,
      next);
    os_memblock_put(&coap_separate_pool, separate_store);
}

void
coap_separate_init(void)
{
    os_mempool_init(&coap_separate_pool, MAX_NUM_CONCURRENT_REQUESTS,
      sizeof(coap_separate_t), coap_separate_area, "coap_sep");
}
#endif /* OC_SERVER && OC_SEPARATE_RESPONSES */
