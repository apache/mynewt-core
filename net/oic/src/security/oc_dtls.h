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

#ifndef OC_DTLS_H_
#define OC_DTLS_H_

#include "deps/tinydtls/dtls.h"
#include "oc_uuid.h"
#include "port/oc_connectivity.h"
#include "util/oc_process.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

OC_PROCESS_NAME(oc_dtls_handler);

void oc_sec_dtls_close_init(oc_endpoint_t *endpoint);
void oc_sec_dtls_close_finish(oc_endpoint_t *endpoint);
void oc_sec_derive_owner_psk(oc_endpoint_t *endpoint, const char *oxm,
                             const size_t oxm_len, const char *server_uuid,
                             const size_t server_uuid_len, const char *obt_uuid,
                             const size_t obt_uuid_len, uint8_t *key,
                             const size_t key_len);
void oc_sec_dtls_init_context(void);
int oc_sec_dtls_send_message(oc_message_t *message);
oc_uuid_t *oc_sec_dtls_get_peer_uuid(oc_endpoint_t *endpoint);
bool oc_sec_dtls_connected(oc_endpoint_t *endpoint);

typedef struct oc_sec_dtls_peer {
    struct oc_sec_dtls_peer_s *next;
    OC_LIST_STRUCT(send_queue);
    session_t session;
    oc_uuid_t uuid;
    bool connected;
    oc_clock_time_t timestamp;
} oc_sec_dtls_peer_t;

#ifdef __cplusplus
}
#endif

#endif /* OC_DTLS_H_ */
