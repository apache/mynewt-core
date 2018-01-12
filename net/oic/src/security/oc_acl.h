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

#ifndef OC_ACL_H_
#define OC_ACL_H_

#include "oc_ri.h"
#include "oc_uuid.h"
#include "port/oc_log.h"
#include "util/oc_list.h"
#include "util/oc_memb.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum oc_sec_acl_permissions_mask {
    OC_PERM_CREATE = (1 << 0),
    OC_PERM_RETRIEVE = (1 << 1),
    OC_PERM_UPDATE = (1 << 2),
    OC_PERM_DELETE = (1 << 3),
    OC_PERM_NOTIFY = (1 << 4)
} oc_sec_acl_permissions_mask_t;

typedef struct oc_sec_acl {
    OC_LIST_STRUCT(subjects);
    oc_uuid_t rowneruuid;
} oc_sec_acl_t;

typedef struct oc_sec_acl_res {
    struct oc_sec_acl_res_s *next;
    oc_resource_t *resource;
    uint16_t permissions;
} oc_sec_acl_res_t;

typedef struct oc_sec_ace {
    struct oc_sec_ace_s *next;
    OC_LIST_STRUCT(resources);
    oc_uuid_t subjectuuid;
} oc_sec_ace_t;

void oc_sec_acl_default(void);
void oc_sec_encode_acl(void);
bool oc_sec_decode_acl(oc_rep_t *rep);
void oc_sec_acl_init(void);
void post_acl(oc_request_t *request, oc_interface_mask_t interface);
bool oc_sec_check_acl(oc_method_t method, oc_resource_t *resource,
                      oc_endpoint_t *endpoint);

#ifdef __cplusplus
}
#endif

#endif /* OC_ACL_H_ */
