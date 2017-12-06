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

#include "oc_acl.h"
#include "config.h"
#include "oc_api.h"
#include "oc_core_res.h"
#include "oc_dtls.h"
#include "oc_rep.h"
#include <stddef.h>
#include <strings.h>

#define MAX_NUM_RES_PERM_PAIRS                                                 \
  (NUM_OC_CORE_RESOURCES + (MAX_NUM_SUBJECTS + 1) * (MAX_APP_RESOURCES))
OC_MEMB(ace_l, oc_sec_ace_t, MAX_NUM_SUBJECTS + 1);
OC_MEMB(res_l, oc_sec_acl_res_t, MAX_NUM_RES_PERM_PAIRS);
static oc_uuid_t WILDCARD = {.id = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                     0, 0 } };
static oc_sec_acl_t ac_list = { 0 };

static void
get_sub_perm_groups(oc_sec_ace_t *ace, uint16_t *groups, int *n)
{
    int i = 0, j;
    oc_sec_acl_res_t *res = oc_list_head(ace->resources);

    while (res != NULL) {
        groups[i++] = res->permissions;
        res = res->next;
    }
    for (i = 0; i < (*n - 1); i++) {
        for (j = (i + 1); j < *n; j++) {
            if (groups[i] > groups[j]) {
                uint16_t t = groups[i];
                groups[i] = groups[j];
                groups[j] = t;
            }
        }
    }
    j = 0;
    for (i = 1; i < *n; i++) {
        if (groups[j] != groups[i])
            groups[++j] = groups[i];
    }
    *n = j + 1;
}

void
oc_sec_encode_acl(void)
{
    int i, n;
    char uuid[37];

    oc_rep_start_root_object();
    oc_process_baseline_interface(oc_core_get_resource_by_index(OCF_SEC_ACL));
    oc_rep_set_object(root, aclist);
    oc_rep_set_array(aclist, aces);
    oc_sec_ace_t *sub = oc_list_head(ac_list.subjects);
    while (sub != NULL) {
        if (strncmp(sub->subjectuuid.id, WILDCARD.id, 16) == 0) {
            uuid[0] = '*';
            uuid[1] = '\0';
        } else {
            oc_uuid_to_str(&sub->subjectuuid, uuid, 37);
        }
        LOG("oc_sec_acl_encode: subject %s\n", uuid);
        n = oc_list_length(sub->resources);
        uint16_t groups[n];
        get_sub_perm_groups(sub, groups, &n);
        for (i = 0; i < n; i++) {
            oc_rep_object_array_start_item(aces);
            oc_rep_set_text_string(aces, subjectuuid, uuid);
            oc_rep_set_uint(aces, permission, groups[i]);
            oc_rep_set_array(aces, resources);
            oc_sec_acl_res_t *res = oc_list_head(sub->resources);
            while (res != NULL) {
                if (res->permissions == groups[i]) {
                    LOG("oc_sec_acl_encode: adding resource %s\n",
                      oc_string(res->resource->uri));
                    oc_rep_object_array_start_item(resources);
                    oc_rep_set_text_string(resources, href,
                                           oc_string(res->resource->uri));
                    oc_rep_set_text_string(resources, rel, "");
                    oc_rep_set_text_string(resources, rt, "");
                    oc_rep_set_text_string(resources, if, "");
                    oc_rep_object_array_end_item(resources);
                }
                res = res->next;
            }
            oc_rep_close_array(aces, resources);
            oc_rep_object_array_end_item(aces);
        }
        sub = sub->next;
    }
    oc_rep_close_array(aclist, aces);
    oc_rep_close_object(root, aclist);
    oc_uuid_to_str(&ac_list.rowneruuid, uuid, 37);
    oc_rep_set_text_string(root, rowneruuid, uuid);
    oc_rep_end_root_object();
}

static oc_sec_acl_res_t *
oc_sec_acl_get_ace(oc_uuid_t *subjectuuid, oc_resource_t *resource, bool create)
{
    oc_sec_ace_t *ace = (oc_sec_ace_t *)oc_list_head(ac_list.subjects);
    oc_sec_acl_res_t *res = NULL;
#ifdef DEBUG
    char uuid[37];

    oc_uuid_to_str(subjectuuid, uuid, 37);
#endif

    while (ace != NULL) {
        if (strncmp(ace->subjectuuid.id, subjectuuid->id, 16) == 0) {
            goto got_ace;
        }
        ace = oc_list_item_next(ace);
    }

    if (create) {
        goto new_ace;
    }
    LOG("Could not find ACE for subject %s\n", uuid);

    goto done;

got_ace:
    LOG("Found ACE for subject %s\n", uuid);
    res = (oc_sec_acl_res_t *)oc_list_head(ace->resources);

    while (res != NULL) {
        if (res->resource == resource) {
            LOG("Found permissions mask for resource %s in ACE\n",
                oc_string(res->resource->uri));
            goto done;
        }
        res = oc_list_item_next(res);
    }

    if (create) {
        goto new_res;
    }

    goto done;

new_ace:
    ace = oc_memb_alloc(&ace_l);

    if (!ace) {
        goto done;
    }

    LOG("Created new ACE for subject %s\n", uuid);

    OC_LIST_STRUCT_INIT(ace, resources);
    strncpy(ace->subjectuuid.id, subjectuuid->id, 16);
    oc_list_add(ac_list.subjects, ace);

new_res:
    res = oc_memb_alloc(&res_l);
    if (res) {
        res->resource = resource;
        LOG("Adding new resource %s to ACE\n", oc_string(res->resource->uri));
        oc_list_add(ace->resources, res);
    }

done:
    return res;
}

static bool
oc_sec_update_acl(oc_uuid_t *subjectuuid, oc_resource_t *resource,
                  uint16_t permissions)
{
    oc_sec_acl_res_t *res = oc_sec_acl_get_ace(subjectuuid, resource, true);

    if (!res) {
        return false;
    }

    res->permissions = permissions;

    LOG("Added resource with permissions: %d\n", res->permissions);

    return true;
}

void
oc_sec_acl_init(void)
{
    OC_LIST_STRUCT_INIT(&ac_list, subjects);
}

void
oc_sec_acl_default(void)
{
    bool success = true;
    int i;
    oc_resource_t *resource;
    oc_uuid_t *device;

    for (i = 0; i < NUM_OC_CORE_RESOURCES; i++) {
        resource = oc_core_get_resource_by_index(i);
        if (i < OCF_SEC_DOXM || i > OCF_SEC_CRED) {
            success &= oc_sec_update_acl(&WILDCARD, resource, 2);
        } else {
            success &= oc_sec_update_acl(&WILDCARD, resource, 6);
        }
    }
    LOG("ACL for core resources initialized %d\n", success);
    device = oc_core_get_device_id(0);
    memcpy(&ac_list.rowneruuid, device, sizeof(oc_uuid_t));
}

bool
oc_sec_check_acl(oc_method_t method, oc_resource_t *resource,
                 oc_endpoint_t *endpoint)
{
    bool granted = false;
    oc_sec_acl_res_t *res = NULL;
    oc_uuid_t *identity = (oc_uuid_t *)oc_sec_dtls_get_peer_uuid(endpoint);

    if (identity) {
        res = oc_sec_acl_get_ace(identity, resource, false);
    }

    if (!res) { // Try Anonymous
        res = oc_sec_acl_get_ace(&WILDCARD, resource, false);
    }

    if (!res) {
        return granted;
    }
    LOG("Got permissions mask %d\n", res->permissions);

    if (res->permissions & OC_PERM_CREATE ||
        res->permissions & OC_PERM_UPDATE) {
        switch (method) {
        case OC_PUT:
        case OC_POST:
            granted = true;
            break;
        default:
            break;
        }
    }

    if (res->permissions & OC_PERM_RETRIEVE ||
        res->permissions & OC_PERM_NOTIFY) {
        switch (method) {
        case OC_GET:
            granted = true;
            break;
        default:
            break;
        }
    }

    if (res->permissions & OC_PERM_DELETE) {
        switch (method) {
        case OC_DELETE:
            granted = true;
            break;
        default:
            break;
        }
    }

    return granted;
}

bool
oc_sec_decode_acl(oc_rep_t *rep)
{
    uint16_t permissions = 0;
    oc_uuid_t subjectuuid;
    oc_rep_t *resources = 0;
    int len = 0;

    while (rep != NULL) {
        len = oc_string_len(rep->name);
        switch (rep->type) {
        case STRING:
            if (len == 10 &&
                strncmp(oc_string(rep->name), "rowneruuid", 10) == 0) {
                oc_str_to_uuid(oc_string(rep->value_string),
                               &ac_list.rowneruuid);
            }
            break;
        case OBJECT: {
            oc_rep_t *aclist = rep->value_object;

            while (aclist != NULL) {
                switch (aclist->type) {
                case OBJECT_ARRAY: {
                    oc_rep_t *aces = aclist->value_object_array;
                    while (aces != NULL) {
                        oc_rep_t *ace = aces->value_object;
                        while (ace != NULL) {
                            len = oc_string_len(ace->name);
                            switch (ace->type) {
                            case STRING:
                                if (len == 11 &&
                                    strncmp(oc_string(ace->name), "subjectuuid",
                                          11) == 0) {
                                    if (strncmp(oc_string(ace->value_string),
                                                "*", 1) == 0) {
                                        strncpy(subjectuuid.id, WILDCARD.id,
                                                16);
                                    } else {
                                        oc_str_to_uuid(oc_string(
                                            ace->value_string), &subjectuuid);
                                    }
                                }
                                break;
                            case INT:
                                if (len == 10 &&
                                    strncmp(oc_string(ace->name), "permission",
                                          10) == 0) {
                                    permissions = ace->value_int;
                                }
                                break;
                            case OBJECT_ARRAY:
                                if (len == 9 &&
                                    strncmp(oc_string(ace->name), "resources",
                                          9) == 0) {
                                    resources = ace->value_object_array;
                                }
                                break;
                            default:
                                break;
                            }
                            ace = ace->next;
                        }

                        while (resources != NULL) {
                            oc_rep_t *resource = resources->value_object;
                            while (resource != NULL) {
                                switch (resource->type) {
                                case STRING:
                                    if (oc_string_len(resource->name) == 4 &&
                                        strncasecmp(oc_string(resource->name),
                                                    "href", 4) == 0) {
                                        oc_resource_t *res;
                                        res = oc_core_get_resource_by_uri(
                                          oc_string(resource->value_string));

#ifdef OC_SERVER
                                        if (!res) {
                                            res = oc_ri_get_app_resource_by_uri(
                                             oc_string(resource->value_string));
                                        }
#endif /* OC_SERVER */
                                        if (!res) {
                                            LOG("\n\noc_sec_acl_decode: could "
                                                "not find resource %s\n\n",
                                             oc_string(resource->value_string));
                                            return false;
                                        }

                                        if (!oc_sec_update_acl(&subjectuuid,
                                                               res,
                                                               permissions)) {
                                            LOG("\n\noc_sec_acl_decode: could "
                                                "not update ACE with "
                                                "resource %s permissions\n\n",
                                              oc_string(res->uri));
                                            return false;
                                        }
                                    }
                                    break;
                                default:
                                    break;
                                }
                                resource = resource->next;
                            }
                            resources = resources->next;
                        }
                        aces = aces->next;
                    }
                } break;
                default:
                    break;
                }
                aclist = aclist->next;
            }
        }
            break;
        default:
            break;
        }
        rep = rep->next;
    }
    return true;
}

/*
  {
  "aclist":
  {
  "aces":
  [
  {
  "subjectuuid": "61646d69-6e44-6576-6963-655575696430",
  "resources":
  [
  {"href": "/led/1", "rel": "", "rt": "", "if": ""},
  {"href": "/switch/1", "rel": "", "rt": "", "if": ""}
  ],
  "permission": 31
  }
  ]
  },
  "rowneruuid": "5cdf40b1-c12e-432b-67a2-aa79a3f08c59"
  }
*/
void
post_acl(oc_request_t *request, oc_interface_mask_t interface)
{
    if (oc_sec_decode_acl(request->request_payload)) {
        oc_send_response(request, OC_STATUS_CREATED);
    } else {
        oc_send_response(request, OC_STATUS_INTERNAL_SERVER_ERROR);
    }
}

#endif /* OC_SECURITY */
