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

#include "oc_doxm.h"
#include "oc_api.h"
#include "oc_core_res.h"
#include <stddef.h>
#include <strings.h>

static oc_sec_doxm_t doxm;

// Fix.. multiple devices.. how many doxms, when we retrieve
// credentials, how do we correlate between creds and devices?
void
oc_sec_doxm_default(void)
{
    oc_uuid_t *deviceuuid;

    doxm.oxmsel = 0;
    doxm.sct = 1;
    doxm.owned = false;
    doxm.dpc = false;
    deviceuuid = oc_core_get_device_id(0);
    oc_gen_uuid(deviceuuid);
    memcpy(&doxm.deviceuuid, deviceuuid, sizeof(oc_uuid_t));
    memset(doxm.devowneruuid.id, 0, 16);
    memset(doxm.rowneruuid.id, 0, 16);
}

void
oc_sec_encode_doxm(void)
{
    int oxms[1] = { 0 };
    char uuid[37];

    oc_rep_start_root_object();
    oc_process_baseline_interface(oc_core_get_resource_by_index(OCF_SEC_DOXM));
    oc_rep_set_int_array(root, oxms, oxms, 1);
    oc_rep_set_int(root, oxmsel, doxm.oxmsel);
    oc_rep_set_int(root, sct, doxm.sct);
    oc_rep_set_boolean(root, owned, doxm.owned);
    oc_uuid_to_str(&doxm.deviceuuid, uuid, 37);
    oc_rep_set_text_string(root, deviceuuid, uuid);
    oc_uuid_to_str(&doxm.devowneruuid, uuid, 37);
    oc_rep_set_text_string(root, devowneruuid, uuid);
    oc_uuid_to_str(&doxm.rowneruuid, uuid, 37);
    oc_rep_set_text_string(root, rowneruuid, uuid);
    oc_rep_end_root_object();
}

oc_sec_doxm_t *
oc_sec_get_doxm(void)
{
    return &doxm;
}

void
get_doxm(oc_request_t *request, oc_interface_mask_t interface)
{
    switch (interface) {
    case OC_IF_BASELINE: {
        char *q;
        int ql = oc_get_query_value(request, "owned", &q);

        if (ql && ((doxm.owned == 1 && strncasecmp(q, "false", 5) == 0) ||
                   (doxm.owned == 0 && strncasecmp(q, "true", 4) == 0))) {
            oc_ignore_request(request);
        } else {
            oc_sec_encode_doxm();
            oc_send_response(request, OC_STATUS_OK);
        }
    }
        break;
    default:
        break;
    }
}

void
oc_sec_decode_doxm(oc_rep_t *rep)
{
    while (rep != NULL) {
        switch (rep->type) {
        case BOOL:
            if (strncmp(oc_string(rep->name), "owned", 5) == 0) {
                doxm.owned = rep->value_boolean;
            } else if (strncmp(oc_string(rep->name), "dpc", 3) == 0) {
                doxm.dpc = rep->value_boolean;
            }
            break;
        case INT:
            if (strncmp(oc_string(rep->name), "oxmsel", 6) == 0) {
                doxm.oxmsel = rep->value_int;
            } else if (strncmp(oc_string(rep->name), "sct", 3) == 0) {
                doxm.sct = rep->value_int;
            }
            break;
        case STRING:
            if (strncmp(oc_string(rep->name), "deviceuuid", 10) == 0) {
                oc_str_to_uuid(oc_string(rep->value_string), &doxm.deviceuuid);
            } else if (strncmp(oc_string(rep->name), "devowneruuid", 12) == 0) {
                oc_str_to_uuid(oc_string(rep->value_string),
                               &doxm.devowneruuid);
            } else if (strncmp(oc_string(rep->name), "rowneruuid", 10) == 0) {
                oc_str_to_uuid(oc_string(rep->value_string), &doxm.rowneruuid);
            }
            break;
        default:
            break;
        }
        rep = rep->next;
    }
}

void
post_doxm(oc_request_t *request, oc_interface_mask_t interface)
{
    oc_sec_decode_doxm(request->request_payload);
    oc_send_response(request, OC_STATUS_CHANGED);
}

#endif /* OC_SECURITY */
