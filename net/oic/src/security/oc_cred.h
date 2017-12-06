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

#ifndef OC_CRED_H_
#define OC_CRED_H_

#include "oc_ri.h"
#include "oc_uuid.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct oc_sec_cred {
    struct oc_sec_cred_s *next;
    int credid;
    int credtype;
    oc_uuid_t subjectuuid;
    uint8_t key[16]; // Supports only 128-bit keys
} oc_sec_cred_t;

void oc_sec_encode_cred(void);
bool oc_sec_decode_cred(oc_rep_t *rep, oc_sec_cred_t **owner);
oc_sec_cred_t *oc_sec_find_cred(oc_uuid_t *subjectuuid);
oc_sec_cred_t *oc_sec_get_cred(oc_uuid_t *subjectuuid);
void put_cred(oc_request_t *request, oc_interface_mask_t interface);
void post_cred(oc_request_t *request, oc_interface_mask_t interface);

#ifdef __cplusplus
}
#endif

#endif /* OC_CRED_H_ */
