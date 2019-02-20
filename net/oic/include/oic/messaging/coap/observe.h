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

#ifndef OBSERVE_H
#define OBSERVE_H

#include "coap.h"
#include "transactions.h"

#ifdef __cplusplus
extern "C" {
#endif

/* OIC stack headers */
#include "oic/oc_ri.h"

#define COAP_OBSERVER_URL_LEN 20

typedef struct coap_observer {
  SLIST_ENTRY(coap_observer) next;

  oc_resource_t *resource;

  char url[COAP_OBSERVER_URL_LEN];
  oc_endpoint_t endpoint;
  uint8_t token_len;
  uint8_t token[COAP_TOKEN_LEN];
  uint16_t last_mid;

  int32_t obs_counter;

  uint8_t retrans_counter;
} coap_observer_t;

void coap_remove_observer(coap_observer_t *o);
int coap_remove_observer_by_client(oc_endpoint_t *endpoint);
int coap_remove_observer_by_token(oc_endpoint_t *endpoint, uint8_t *token,
                                  size_t token_len);
int coap_remove_observer_by_uri(oc_endpoint_t *endpoint, const char *uri);
int coap_remove_observer_by_mid(oc_endpoint_t *endpoint, uint16_t mid);

int coap_notify_observers(oc_resource_t *resource,
                          struct oc_response_buffer *response_buf,
                          oc_endpoint_t *endpoint);
// int coap_notify_observers_sub(oc_resource_t *resource, const char *subpath);

int coap_observe_handler(struct coap_packet_rx *req, coap_packet_t *response,
                         oc_resource_t *resource, oc_endpoint_t *endpoint);

void coap_observer_walk(int (*walk_func)(struct coap_observer *, void *),
                        void *arg);
void coap_observe_init(void);

#ifdef __cplusplus
}
#endif

#endif /* OBSERVE_H */
