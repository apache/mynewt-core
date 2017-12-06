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

#ifndef OC_PSTAT_H_
#define OC_PSTAT_H_

#include "oc_ri.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct oc_sec_pstat {
    bool isop;
    int cm;
    int tm;
    int om;
    int sm;
} oc_sec_pstat_t;

bool oc_sec_provisioned(void);
void oc_sec_decode_pstat(oc_rep_t *rep);
void oc_sec_encode_pstat(void);
oc_sec_pstat_t *oc_sec_get_pstat(void);
void oc_sec_pstat_default(void);
void get_pstat(oc_request_t *request, oc_interface_mask_t interface);
void post_pstat(oc_request_t *request, oc_interface_mask_t interface);

#ifdef __cplusplus
}
#endif

#endif /* OC_PSTAT_H_ */
