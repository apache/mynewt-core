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

#ifdef OC_SECURITY

#include "oc_pstat.h"
#include "oc_api.h"
#include "oc_core_res.h"
#include "oc_doxm.h"

static oc_sec_pstat_t pstat;

oc_sec_pstat_t *
oc_sec_get_pstat(void)
{
    return &pstat;
}

bool
oc_sec_provisioned(void)
{
    return pstat.isop;
}

void
oc_sec_pstat_default(void)
{
    pstat.isop = false;
    pstat.cm = 2;
    pstat.tm = 0;
    pstat.om = 3;
    pstat.sm = 3;
}

void
oc_sec_encode_pstat(void)
{
    char uuid[37];
    oc_sec_doxm_t *doxm = oc_sec_get_doxm();

    oc_rep_start_root_object();
    oc_process_baseline_interface(oc_core_get_resource_by_index(OCF_SEC_PSTAT));
    oc_rep_set_uint(root, cm, pstat.cm);
    oc_rep_set_uint(root, tm, pstat.tm);
    oc_rep_set_int(root, om, pstat.om);
    oc_rep_set_int(root, sm, pstat.sm);
    oc_rep_set_boolean(root, isop, pstat.isop);
    oc_uuid_to_str(&doxm->deviceuuid, uuid, 37);
    oc_rep_set_text_string(root, deviceuuid, uuid);
    oc_uuid_to_str(&doxm->rowneruuid, uuid, 37);
    oc_rep_set_text_string(root, rowneruuid, uuid);
    oc_rep_end_root_object();
}

void
oc_sec_decode_pstat(oc_rep_t *rep)
{
    oc_sec_doxm_t *doxm = oc_sec_get_doxm();

    while (rep != NULL) {
        switch (rep->type) {
        case BOOL:
            pstat.isop = rep->value_boolean;
            break;
        case INT:
            if (strncmp(oc_string(rep->name), "cm", 2) == 0) {
                pstat.cm = rep->value_int;
            } else if (strncmp(oc_string(rep->name), "tm", 2) == 0) {
                pstat.tm = rep->value_int;
            } else if (strncmp(oc_string(rep->name), "om", 2) == 0) {
                pstat.om = rep->value_int;
            } else if (strncmp(oc_string(rep->name), "sm", 2) == 0) {
                pstat.sm = rep->value_int;
            }
            break;
        case STRING:
            if (strncmp(oc_string(rep->name), "deviceuuid", 10) == 0) {
                oc_str_to_uuid(oc_string(rep->value_string), &doxm->deviceuuid);
            } else if (strncmp(oc_string(rep->name), "rowneruuid", 10) == 0) {
                oc_str_to_uuid(oc_string(rep->value_string), &doxm->rowneruuid);
            }
            break;
        default:
            break;
        }
        rep = rep->next;
    }
}

void
get_pstat(oc_request_t *request, oc_interface_mask_t interface)
{
    switch (interface) {
    case OC_IF_BASELINE: {
        oc_sec_encode_pstat();
        oc_send_response(request, OC_STATUS_OK);
    } break;
    default:
        break;
    }
}

void
post_pstat(oc_request_t *request, oc_interface_mask_t interface)
{
    oc_sec_decode_pstat(request->request_payload);
    oc_send_response(request, OC_STATUS_CHANGED);
}

#endif /* OC_SECURITY */
