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

#include "os/mynewt.h"

#include "oic/port/mynewt/config.h"
#include "oic/oc_core_res.h"
#include "oic/messaging/coap/oc_coap.h"
#include "oic/oc_rep.h"
#include "oic/oc_ri.h"

#ifdef OC_SECURITY
#include "security/oc_pstat.h"
#endif /* OC_SECURITY */

static oc_resource_t core_resources[NUM_OC_CORE_RESOURCES];
static struct oc_device_info {
    oc_uuid_t uuid;
    oc_string_t payload;
} oc_device_info[MAX_NUM_DEVICES];
static int device_count;
static oc_string_t oc_platform_payload;

void
oc_core_encode_interfaces_mask(CborEncoder *parent,
                               oc_interface_mask_t interface)
{
    oc_rep_set_key((*parent), "if");
    oc_rep_start_array((*parent), if);
    if (interface & OC_IF_LL) {
        oc_rep_add_text_string(if, OC_RSRVD_IF_LL);
    }
    if (interface & OC_IF_B) {
        oc_rep_add_text_string(if, OC_RSRVD_IF_B);
    }
    if (interface & OC_IF_R) {
        oc_rep_add_text_string(if, OC_RSRVD_IF_R);
    }
    if (interface & OC_IF_RW) {
        oc_rep_add_text_string(if, OC_RSRVD_IF_RW);
    }
    if (interface & OC_IF_A) {
      oc_rep_add_text_string(if, OC_RSRVD_IF_A);
    }
    if (interface & OC_IF_S) {
        oc_rep_add_text_string(if, OC_RSRVD_IF_S);
    }
    oc_rep_add_text_string(if, OC_RSRVD_IF_BASELINE);
    oc_rep_end_array((*parent), if);
}

static void
oc_core_device_handler(oc_request_t *req, oc_interface_mask_t interface)
{
    struct oc_response_buffer *rsp_buf;
    struct os_mbuf *buffer;
    int size;
    char *str;

    rsp_buf = req->response->response_buffer;
    buffer = rsp_buf->buffer;
    size = oc_string_len(oc_device_info[req->resource->device].payload);
    str = oc_string(oc_device_info[req->resource->device].payload);

    switch (interface) {
    case OC_IF_R:
    case OC_IF_BASELINE:
        if (os_mbuf_append(buffer, str, size)) {
            rsp_buf->response_length = 0;
            rsp_buf->code = oc_status_code(OC_STATUS_INTERNAL_SERVER_ERROR);
        } else {
            rsp_buf->response_length = size;
            rsp_buf->code = oc_status_code(OC_STATUS_OK);
        }
        break;
    default:
        break;
    }
}

int
oc_core_get_num_devices(void)
{
    return device_count;
}

static int
finalize_payload(struct os_mbuf *m, oc_string_t *payload)
{
    int size;

    oc_rep_end_root_object();
    size = oc_rep_finalize();
    if (size != -1) {
        oc_alloc_string(payload, size + 1);
        os_mbuf_copydata(m, 0, size, payload->os_str);
        os_mbuf_free_chain(m);
        return 1;
    }
    os_mbuf_free_chain(m);
    return -1;
}

oc_string_t *
oc_core_add_new_device(const char *uri, const char *rt, const char *name,
                       const char *spec_version, const char *data_model_version,
                       oc_core_add_device_cb_t add_device_cb, void *data)
{
    struct os_mbuf *tmp;
    int ocf_d;
    char uuid[37];

    if (device_count == MAX_NUM_DEVICES) {
        return NULL;
    }

    /* Once provisioned, UUID is retrieved from the credential store.
       If not yet provisioned, a default is generated in the security
       layer.
    */
#ifdef OC_SECURITY /*fix if add new devices after provisioning, need to reset  \
                      or it will generate non-standard uuid */
    /* where are secondary device ids persisted? */
    if (!oc_sec_provisioned() && device_count > 0) {
        oc_gen_uuid(&oc_device_info[device_count].uuid);
    }
#else
    oc_gen_uuid(&oc_device_info[device_count].uuid);
#endif

    ocf_d = NUM_OC_CORE_RESOURCES - 1 - device_count;

    /* Construct device resource */
    oc_core_populate_resource(ocf_d, uri, rt, OC_IF_R | OC_IF_BASELINE,
                              OC_IF_BASELINE, OC_ACTIVE | OC_DISCOVERABLE,
                              oc_core_device_handler, 0, 0, 0, device_count);

    /* Encoding device resource payload */
    tmp = os_msys_get_pkthdr(0, 0);
    if (!tmp) {
        return NULL;
    }
    oc_rep_new(tmp);

    oc_rep_start_root_object();

    oc_rep_set_string_array(root, rt, core_resources[ocf_d].types);
    oc_core_encode_interfaces_mask(oc_rep_object(root),
                                   core_resources[ocf_d].interfaces);
    oc_rep_set_uint(root, p, core_resources[ocf_d].properties);

    oc_uuid_to_str(&oc_device_info[device_count].uuid, uuid, 37);
    oc_rep_set_text_string(root, di, uuid);
    oc_rep_set_text_string(root, n, name);
    oc_rep_set_text_string(root, icv, spec_version);
    oc_rep_set_text_string(root, dmv, data_model_version);

    if (add_device_cb) {
        add_device_cb(data);
    }
    if (!finalize_payload(tmp, &oc_device_info[device_count].payload)) {
        return NULL;
    }

    return &oc_device_info[device_count++].payload;
}

void
oc_core_platform_handler(oc_request_t *req, oc_interface_mask_t interface)
{
    struct oc_response_buffer *rsp_buf;
    struct os_mbuf *buffer;
    int size;
    char *str;

    rsp_buf = req->response->response_buffer;
    buffer = rsp_buf->buffer;
    size = oc_string_len(oc_platform_payload);
    str = oc_string(oc_platform_payload);

    switch (interface) {
    case OC_IF_R:
    case OC_IF_BASELINE:
        if (os_mbuf_append(buffer, str, size)) {
            rsp_buf->response_length = 0;
            rsp_buf->code = oc_status_code(OC_STATUS_INTERNAL_SERVER_ERROR);
        } else {
            rsp_buf->response_length = size;
            rsp_buf->code = oc_status_code(OC_STATUS_OK);
        }
        break;
    default:
        break;
    }
}

oc_string_t *
oc_core_init_platform(const char *mfg_name, oc_core_init_platform_cb_t init_cb,
                      void *data)
{
    struct os_mbuf *tmp;
    oc_uuid_t uuid; /*fix uniqueness of platform id?? */
    char uuid_str[37];

    if (oc_string_len(oc_platform_payload) > 0) {
        return NULL;
    }
    /* Populating resource obuject */
    oc_core_populate_resource(OCF_P, OC_RSRVD_PLATFORM_URI, "oic.wk.p",
                              OC_IF_R | OC_IF_BASELINE, OC_IF_BASELINE,
                              OC_ACTIVE | OC_DISCOVERABLE,
                              oc_core_platform_handler, 0, 0, 0, 0);

    /* Encoding platform resource payload */
    tmp = os_msys_get_pkthdr(0, 0);
    if (!tmp) {
        return NULL;
    }
    oc_rep_new(tmp);
    oc_rep_start_root_object();
    oc_rep_set_string_array(root, rt, core_resources[OCF_P].types);
    oc_core_encode_interfaces_mask(oc_rep_object(root),
                                   core_resources[OCF_P].interfaces);
    oc_rep_set_uint(root, p, core_resources[OCF_P].properties & ~OC_PERIODIC);

    oc_gen_uuid(&uuid);

    oc_uuid_to_str(&uuid, uuid_str, 37);
    oc_rep_set_text_string(root, pi, uuid_str);
    oc_rep_set_text_string(root, mnmn, mfg_name);
    oc_rep_set_text_string(root, mnos, "MyNewt");

    if (init_cb) {
        init_cb(data);
    }
    if (!finalize_payload(tmp, &oc_platform_payload)) {
        return NULL;
    }
    return &oc_platform_payload;
}

void
oc_core_populate_resource(int type, const char *uri, const char *rt,
                          oc_interface_mask_t interfaces,
                          oc_interface_mask_t default_interface,
                          oc_resource_properties_t properties,
                          oc_request_handler_t get, oc_request_handler_t put,
                          oc_request_handler_t post,
                          oc_request_handler_t delete, int device)
{
    oc_resource_t *r = &core_resources[type];
    r->device = device;
    oc_new_string(&r->uri, uri);
    r->properties = properties;
    oc_new_string_array(&r->types, 1);
    oc_string_array_add_item(r->types, rt);
    r->interfaces = interfaces;
    r->default_interface = default_interface;
    r->get_handler = get;
    r->put_handler = put;
    r->post_handler = post;
    r->delete_handler = delete;
}

oc_uuid_t *
oc_core_get_device_id(int device)
{
    return &oc_device_info[device].uuid;
}

oc_resource_t *
oc_core_get_resource_by_index(int type)
{
    return &core_resources[type];
}

oc_resource_t *
oc_core_get_resource_by_uri(const char *uri)
{
    int i;

    for (i = 0; i < NUM_OC_CORE_RESOURCES; i++) {
        if (oc_string_len(core_resources[i].uri) == strlen(uri) &&
          strncmp(uri, oc_string(core_resources[i].uri), strlen(uri)) == 0)
            return &core_resources[i];
    }
    return NULL;
}
