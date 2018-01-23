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

#ifndef TRANSACTIONS_H
#define TRANSACTIONS_H

#include "coap.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Modulo mask (thus +1) for a random number to get the tick number for the
 * random
 * retransmission time between COAP_RESPONSE_TIMEOUT and
 * COAP_RESPONSE_TIMEOUT*COAP_RESPONSE_RANDOM_FACTOR.
 */
#define COAP_RESPONSE_TIMEOUT_TICKS (OS_TICKS_PER_SEC * COAP_RESPONSE_TIMEOUT)
#define COAP_RESPONSE_TIMEOUT_BACKOFF_MASK                              \
    (long)((OS_TICKS_PER_SEC * COAP_RESPONSE_TIMEOUT *                  \
        ((float)COAP_RESPONSE_RANDOM_FACTOR - 1.0)) +                   \
      0.5) +                                                            \
    1

/* container for transactions with message buffer and retransmission info */
typedef struct coap_transaction {
    SLIST_ENTRY(coap_transaction) next;

    uint16_t mid;
    uint8_t retrans_counter;
    coap_message_type_t type;
    uint32_t retrans_tmo;
    struct os_callout retrans_timer;
    struct os_mbuf *m;
} coap_transaction_t;

void coap_register_as_transaction_handler(void);

coap_transaction_t *coap_new_transaction(uint16_t mid, oc_endpoint_t *);

void coap_send_transaction(coap_transaction_t *t);
void coap_clear_transaction(coap_transaction_t *t);
coap_transaction_t *coap_get_transaction_by_mid(uint16_t mid);

void coap_check_transactions(void);

void coap_transaction_init(void);

#ifdef __cplusplus
}
#endif

#endif /* TRANSACTIONS_H */
