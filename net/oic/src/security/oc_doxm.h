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

#ifndef OC_DOXM_H_
#define OC_DOXM_H_

#include "oc_uuid.h"
#include "port/oc_log.h"
#include "util/oc_list.h"
#include "util/oc_memb.h"

#include "oc_ri.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct oc_sec_doxm {
    int oxmsel;
    int sct;
    bool owned;
    bool dpc;
    oc_uuid_t deviceuuid;
    oc_uuid_t devowneruuid;
    oc_uuid_t rowneruuid;
} oc_sec_doxm_t;

void oc_sec_decode_doxm(oc_rep_t *rep);
void oc_sec_encode_doxm(void);
oc_sec_doxm_t *oc_sec_get_doxm(void);
void oc_sec_doxm_default(void);
void get_doxm(oc_request_t *request, oc_interface_mask_t interface);
void post_doxm(oc_request_t *request, oc_interface_mask_t interface);
#ifdef __cplusplus
}
#endif

#endif /* OC_DOXM_H_ */
