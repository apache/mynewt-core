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

#ifndef OC_COAP_H
#define OC_COAP_H

#include "oic/messaging/coap/separate.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct oc_separate_response {
    SLIST_HEAD(, coap_separate) requests;
    int active;
    struct os_mbuf *buffer;
} oc_separate_response_t;

typedef struct oc_response_buffer {
    struct os_mbuf *buffer;
    int32_t *block_offset;
    uint16_t response_length;
    int code;
} oc_response_buffer_t;

#ifdef __cplusplus
}
#endif

#endif /* OC_COAP_H */
