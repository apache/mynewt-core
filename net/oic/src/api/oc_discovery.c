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
#ifdef OC_CLIENT
#include "oic/oc_client_state.h"
#endif /* OC_CLIENT */

#include "oic/messaging/coap/oc_coap.h"
#include "oic/oc_api.h"
#include "oic/oc_core_res.h"
#include "oic/port/mynewt/ip.h"

static bool
filter_resource(oc_resource_t *resource, const char *rt, int rt_len,
                CborEncoder *links)
{
  int i;
  bool match = true;
  if (rt_len > 0) {
    match = false;
    for (i = 0; i < oc_string_array_get_allocated_size(resource->types); i++) {
      int size = oc_string_array_get_item_size(resource->types, i);
      const char *t =
        (const char *)oc_string_array_get_item(resource->types, i);
      if (rt_len == size && strncmp(rt, t, rt_len) == 0) {
        match = true;
        break;
      }
    }
  }

  if (!match) {
    return false;
  }

  oc_rep_start_object(*links, res);

  // uri
  oc_rep_set_text_string(res, href, oc_string(resource->uri));

  // rt
  oc_rep_set_array(res, rt);
  for (i = 0; i < oc_string_array_get_allocated_size(resource->types); i++) {
    int size = oc_string_array_get_item_size(resource->types, i);
    const char *t = (const char *)oc_string_array_get_item(resource->types, i);
    if (size > 0)
      oc_rep_add_text_string(rt, t);
  }
  oc_rep_close_array(res, rt);

  // if
  oc_core_encode_interfaces_mask(oc_rep_object(res), resource->interfaces);

  // p
  oc_rep_set_object(res, p);
  oc_rep_set_uint(p, bm, resource->properties & ~OC_PERIODIC);
#ifdef OC_SECURITY
  if (resource->properties & OC_SECURE) {
    oc_rep_set_boolean(p, sec, true);
    oc_rep_set_uint(p, port, oc_connectivity_get_dtls_port());
  }
#endif /* OC_SECURITY */

  oc_rep_close_object(res, p);

  oc_rep_end_object(*links, res);
  return true;
}

static int
process_device_object(CborEncoder *device, const char *uuid, const char *rt,
                      int rt_len)
{
  int dev, matches = 0;
#ifdef OC_SERVER
  oc_resource_t *resource;
#endif
  oc_rep_start_object(*device, links);
  oc_rep_set_text_string(links, di, uuid);
  oc_rep_set_array(links, links);

  if (filter_resource(oc_core_get_resource_by_index(OCF_P), rt, rt_len,
                      oc_rep_array(links)))
    matches++;

  for (dev = 0; dev < oc_core_get_num_devices(); dev++) {
    if (filter_resource(
          oc_core_get_resource_by_index(NUM_OC_CORE_RESOURCES - 1 - dev), rt,
          rt_len, oc_rep_array(links)))
      matches++;
  }

#ifdef OC_SERVER
  for (resource = oc_ri_get_app_resources(); resource;
       resource = SLIST_NEXT(resource, next)) {
      if (!(resource->properties & OC_DISCOVERABLE)) {
          continue;
      }
      if (filter_resource(resource, rt, rt_len, oc_rep_array(links))) {
          matches++;
      }
  }
#endif

#ifdef OC_SECURITY
  if (filter_resource(oc_core_get_resource_by_index(OCF_SEC_DOXM), rt, rt_len,
                      oc_rep_array(links)))
    matches++;
#endif

  oc_rep_close_array(links, links);
  oc_rep_end_object(*device, links);

  return matches;
}

static void
oc_core_discovery_handler(oc_request_t *req, oc_interface_mask_t interface)
{
    char *rt = NULL;
    int rt_len = 0, matches = 0;
    char uuid[37];

    rt_len = oc_ri_get_query_value(req->query, req->query_len, "rt", &rt);

    oc_uuid_to_str(oc_core_get_device_id(0), uuid, sizeof(uuid));

    switch (interface) {
    case OC_IF_LL: {
        oc_rep_start_links_array();
        matches = process_device_object(oc_rep_array(links), uuid, rt, rt_len);
        oc_rep_end_links_array();
    } break;
    case OC_IF_BASELINE: {
        oc_rep_start_root_object();
        oc_process_baseline_interface(req->resource);
        oc_rep_set_array(root, links);
        matches = process_device_object(oc_rep_array(links), uuid, rt, rt_len);
        oc_rep_close_array(root, links);
        oc_rep_end_root_object();
    } break;
    default:
        break;
    }

    int response_length = oc_rep_finalize();

    if (matches && response_length > 0) {
        req->response->response_buffer->response_length = response_length;
        req->response->response_buffer->code = oc_status_code(OC_STATUS_OK);
    } else {
        /* There were rt/if selections and there were no matches, so ignore */
        req->response->response_buffer->code = OC_IGNORE;
    }
}

void
oc_create_discovery_resource(void)
{
  oc_core_populate_resource(OCF_RES, "/oic/res", "oic.wk.res",
                            OC_IF_LL | OC_IF_BASELINE, OC_IF_LL, OC_ACTIVE,
                            oc_core_discovery_handler, 0, 0, 0, 0);
}

#ifdef OC_CLIENT
oc_discovery_flags_t
oc_ri_process_discovery_payload(struct coap_packet_rx *rsp,
                                oc_discovery_cb_t *handler,
                                oc_endpoint_t *endpoint)
{
  oc_discovery_flags_t ret = OC_CONTINUE_DISCOVERY;
  oc_string_t uri = {
      .os_sz = 0,
      .os_str = NULL
  };
  oc_string_t di = {
      .os_sz = 0,
      .os_str = NULL
  };
  bool secure = false;
  uint16_t dtls_port = 0, default_port = 0;
  oc_string_array_t types = {};
  oc_interface_mask_t interfaces = 0;
  oc_server_handle_t handle;
  uint16_t data_off;
  struct os_mbuf *m;
  int len;
  struct oc_endpoint_ip *ip;

  memcpy(&handle.endpoint, endpoint, oc_endpoint_size(endpoint));
  if (oc_endpoint_is_ip(endpoint)) {
      default_port = ((struct oc_endpoint_ip *)endpoint)->port;
  }

  oc_rep_t *array = 0, *rep;

  len = coap_get_payload(rsp, &m, &data_off);
  int s = oc_parse_rep(m, data_off, len, &rep);
  if (s == 0)
    array = rep;
  while (array != NULL) {
    oc_rep_t *device_map = array->value_object;
    while (device_map != NULL) {
      switch (device_map->type) {
      case STRING:
        if (oc_string_len(device_map->name) == 2 &&
            strncmp(oc_string(device_map->name), "di", 2) == 0)
          di = device_map->value_string;
        break;
      default:
        break;
      }
      device_map = device_map->next;
    }
    device_map = array->value_object;
    while (device_map != NULL) {
      switch (device_map->type) {
      case OBJECT_ARRAY: {
        oc_rep_t *links = device_map->value_object_array;
        while (links != NULL) {
          switch (links->type) {
          case OBJECT: {
            oc_rep_t *resource_info = links->value_object;
            while (resource_info != NULL) {
              switch (resource_info->type) {
              case STRING:
                uri = resource_info->value_string;
                break;
              case STRING_ARRAY:
                if (oc_string_len(resource_info->name) == 2 &&
                    strncmp(oc_string(resource_info->name), "rt", 2) == 0)
                  types = resource_info->value_array;
                else {
                  interfaces = 0;
                  int i;
                  for (i = 0; i < oc_string_array_get_allocated_size(
                                    resource_info->value_array);
                       i++) {
                    interfaces |= oc_ri_get_interface_mask(
                      oc_string_array_get_item(resource_info->value_array, i),
                      oc_string_array_get_item_size(resource_info->value_array,
                                                    i));
                  }
                }
                break;
              case OBJECT: {
                oc_rep_t *policy_info = resource_info->value_object;
                while (policy_info != NULL) {
                  if (policy_info->type == INT &&
                      oc_string_len(policy_info->name) == 4 &&
                      strncmp(oc_string(policy_info->name), "port", 4) == 0) {
                    dtls_port = policy_info->value_int;
                  }
                  if (policy_info->type == BOOL &&
                      oc_string_len(policy_info->name) == 3 &&
                      strncmp(oc_string(policy_info->name), "sec", 3) == 0 &&
                      policy_info->value_boolean == true) {
                    secure = true;
                  }
                  policy_info = policy_info->next;
                }
              } break;
              default:
                break;
              }
              resource_info = resource_info->next;
            }
            if (default_port) {
                ip = (struct oc_endpoint_ip *)&handle.endpoint;
                if (secure) {
                    ip->port = dtls_port;
                    ip->ep.oe_flags |= OC_ENDPOINT_SECURED;
                } else {
                    ip->port = default_port;
                    ip->ep.oe_flags &= ~OC_ENDPOINT_SECURED;
                }
            }
            if (handler(oc_string(di), oc_string(uri), types, interfaces,
                        &handle) == OC_STOP_DISCOVERY) {
              ret = OC_STOP_DISCOVERY;
              goto done;
            }
            dtls_port = 0;
            secure = false;
          } break;
          default:
            break;
          }
          links = links->next;
        }
      } break;
      default:
        break;
      }
      device_map = device_map->next;
    }
    array = array->next;
  }
done:
  oc_free_rep(rep);
  return ret;
}
#endif /* OC_CLIENT */
