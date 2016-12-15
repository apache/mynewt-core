/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <syscfg/syscfg.h>
#include <sysinit/sysinit.h>

#include <os/os.h>
#include <os/endian.h>

#include <assert.h>
#include <string.h>

#include <mgmt/mgmt.h>
#include <nmgr_os/nmgr_os.h>

#include <cborattr/cborattr.h>
#include <tinycbor/cbor.h>
#include <tinycbor/cbor_buf_writer.h>
#include <tinycbor/cbor_buf_reader.h>
#include <oic/oc_api.h>

struct omgr_cbuf {
    struct mgmt_cbuf ob_mj;
    struct cbor_buf_reader ob_reader;
};

struct omgr_state {
    struct os_eventq *os_evq;
    struct os_event os_event;
    struct os_task os_task;
    struct omgr_cbuf os_cbuf;		/* CBOR buffer for NMGR task */
};

static void omgr_event_start(struct os_event *ev);

static struct omgr_state omgr_state = {
    .os_event.ev_cb = omgr_event_start,
};

static void omgr_oic_get(oc_request_t *request, oc_interface_mask_t interface);
static void omgr_oic_put(oc_request_t *request, oc_interface_mask_t interface);

struct os_eventq *
mgmt_evq_get(void)
{
    return omgr_state.os_evq;
}

void
mgmt_evq_set(struct os_eventq *evq)
{
    os_eventq_designate(&omgr_state.os_evq, evq, &omgr_state.os_event);
}

static const struct mgmt_handler *
omgr_find_handler(const char *q, int qlen)
{
    char id_str[8];
    int grp = -1;
    int id = -1;
    char *str;
    char *eptr;
    int slen;

    slen = oc_ri_get_query_value(q, qlen, "gr", &str);
    if (slen > 0 && slen < sizeof(id_str) - 1) {
        memcpy(id_str, str, slen);
        id_str[slen] = '\0';
        grp = strtoul(id_str, &eptr, 0);
        if (*eptr != '\0') {
            return NULL;
        }
    }
    slen = oc_ri_get_query_value(q, qlen, "id", &str);
    if (slen > 0 && slen < sizeof(id_str) - 1) {
        memcpy(id_str, str, slen);
        id_str[slen] = '\0';
        id = strtoul(id_str, &eptr, 0);
        if (*eptr != '\0') {
            return NULL;
        }
    }
    return mgmt_find_handler(grp, id);
}

static void
omgr_oic_op(oc_request_t *req, oc_interface_mask_t mask, int isset)
{
    struct omgr_state *o = &omgr_state;
    const struct mgmt_handler *handler;
    const uint8_t *data;
    int rc = 0;
    extern CborEncoder g_encoder;

    if (!req->query_len) {
        goto bad_req;
    }

    handler = omgr_find_handler(req->query, req->query_len);
    if (!handler) {
        goto bad_req;
    }

    rc = coap_get_payload(req->packet, &data);
    cbor_buf_reader_init(&o->os_cbuf.ob_reader, data, rc);

    cbor_parser_init(&o->os_cbuf.ob_reader.r, 0, &o->os_cbuf.ob_mj.parser, &o->os_cbuf.ob_mj.it);

    /* start generating the CBOR OUTPUT */
    /* this is worth a quick note.  We are encoding CBOR within CBOR, so we
     * need to use the same encoder as ocf stack.  We are using their global
     * encoder g_encoder which they intialized before this function is called.
     * But we can't call their macros here as it won't use the right mape
     * encoder (ob_mj) */
    cbor_encoder_create_map(&g_encoder, &o->os_cbuf.ob_mj.encoder,
                            CborIndefiniteLength);

    switch (mask) {
    case OC_IF_BASELINE:
        oc_process_baseline_interface(req->resource);
    case OC_IF_RW:
        if (!isset) {
            cbor_encode_text_string(&root_map, "r", 1);
            if (handler->mh_read) {
                rc = handler->mh_read(&o->os_cbuf.ob_mj);
            } else {
                goto bad_req;
            }
        } else {
            cbor_encode_text_string(&root_map, "w", 1);
            if (handler->mh_write) {
                rc = handler->mh_write(&o->os_cbuf.ob_mj);
            } else {
                goto bad_req;
            }
        }
        if (rc) {
            goto bad_req;
        }
        break;
    default:
        break;
    }

    cbor_encoder_close_container(&g_encoder, &o->os_cbuf.ob_mj.encoder);
    oc_send_response(req, OC_STATUS_OK);

    return;
bad_req:
    /*
     * XXXX might send partially constructed response as payload
     */
    if (rc == MGMT_ERR_ENOMEM) {
        rc = OC_STATUS_INTERNAL_SERVER_ERROR;
    } else {
        rc = OC_STATUS_BAD_REQUEST;
    }
    oc_send_response(req, rc);
}

static void
omgr_oic_get(oc_request_t *req, oc_interface_mask_t mask)
{
    omgr_oic_op(req, mask, 0);
}

static void
omgr_oic_put(oc_request_t *req, oc_interface_mask_t mask)
{
    omgr_oic_op(req, mask, 1);
}

static void
omgr_event_start(struct os_event *ev)
{
    uint8_t mode;
    oc_resource_t *res = NULL;
    char name[12];

    /*
     * net/oic must be initialized before now.
     */
    snprintf(name, sizeof(name), "/omgr");
    res = oc_new_resource(name, 1, 0);
    oc_resource_bind_resource_type(res, "x.mynewt.nmgr");
    mode = OC_IF_RW;
    oc_resource_bind_resource_interface(res, mode);
    oc_resource_set_default_interface(res, mode);
    oc_resource_set_discoverable(res);
    oc_resource_set_request_handler(res, OC_GET, omgr_oic_get);
    oc_resource_set_request_handler(res, OC_PUT, omgr_oic_put);
    oc_add_resource(res);
}

void
oicmgr_init(struct sysinit_init_ctxt *ctxt)
{
    int rc;

    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    rc = nmgr_os_groups_register();
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

SYSINIT_REGISTER_INIT(oicmgr_init, 5);
