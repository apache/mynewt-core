/**
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

#define OMGR_OC_EVENT	(OS_EVENT_T_PERUSER)
#define OMGR_OC_TIMER	(OS_EVENT_T_PERUSER + 1)
#define OICMGR_STACK_SZ	OS_STACK_ALIGN(MYNEWT_VAL(OICMGR_STACK_SIZE))

struct omgr_cbuf {
    struct mgmt_cbuf ob_mj;
    struct cbor_buf_reader ob_reader;
};

struct omgr_state {
    struct os_eventq os_evq;
    struct os_event os_oc_event;
    struct os_callout os_oc_timer;
    struct os_task os_task;
    struct omgr_cbuf os_cbuf;		/* CBOR buffer for NMGR task */
};

static struct omgr_state omgr_state = {
  .os_oc_event.ev_type = OMGR_OC_EVENT,
  .os_oc_timer.c_ev.ev_type = OMGR_OC_TIMER,
  .os_oc_timer.c_evq = &omgr_state.os_evq
};
struct os_eventq *g_mgmt_evq = &omgr_state.os_evq;

static os_stack_t oicmgr_stack[OICMGR_STACK_SZ];

static void omgr_oic_get(oc_request_t *request, oc_interface_mask_t interface);
static void omgr_oic_put(oc_request_t *request, oc_interface_mask_t interface);

static const struct mgmt_handler *
omgr_oic_find_handler(const char *q, int qlen)
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
    oc_rep_t *data;
    int rc;
    extern CborEncoder g_encoder;

    if (!req->query_len) {
        goto bad_req;
    }

    handler = omgr_oic_find_handler(req->query, req->query_len);
    if (!handler) {
        goto bad_req;
    }

    data = req->request_payload;
    if (data) {
        if (data->type != BYTE_STRING) {
            goto bad_req;
        }

        cbor_buf_reader_init(&o->os_cbuf.ob_reader,
                             (uint8_t *) oc_string(data->value_string),
                             oc_string_len(data->value_string));
    } else {
        cbor_buf_reader_init(&o->os_cbuf.ob_reader, NULL, 0);
    }

    cbor_parser_init(&o->os_cbuf.ob_reader.r, 0, &o->os_cbuf.ob_mj.parser, &o->os_cbuf.ob_mj.it);

    /* start generating the CBOR OUTPUT */
    /* this is worth a quick note.  We are encoding CBOR within CBOR, so we need
     * to use the same encoder as ocf stack.  We are using their global encoder
     * g_encoder which they intialized before this function is called. Byt we can't
     * call their macros here as it won't use the right mape encoder (ob_mj) */
    cbor_encoder_create_map(&g_encoder, &o->os_cbuf.ob_mj.encoder, CborIndefiniteLength);

    switch (mask) {
    case OC_IF_BASELINE:
        oc_process_baseline_interface(req->resource);
    case OC_IF_RW:
        cbor_encode_text_string(&root_map, "key", strlen("key"));
        if (!isset) {
            if (handler->mh_read) {
                rc = handler->mh_read(&o->os_cbuf.ob_mj);
            } else {
                goto bad_req;
            }
        } else {
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
    oc_send_response(req, OC_STATUS_BAD_REQUEST);
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
omgr_app_init(void)
{
    oc_init_platform("MyNewt", NULL, NULL);
    oc_add_device("/oic/d", "oic.d.light", "MynewtLed", "1.0", "1.0", NULL,
      NULL);
}

static void
omgr_register_resources(void)
{
    uint8_t mode;
    oc_resource_t *res = NULL;
    char name[12];

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

static const oc_handler_t omgr_oc_handler = {
    .init = omgr_app_init,
    .register_resources = omgr_register_resources
};

void
oc_signal_main_loop(void)
{
    struct omgr_state *o = &omgr_state;

    os_eventq_put(&o->os_evq, &o->os_oc_event);
}

void
omgr_oic_task(void *arg)
{
    struct omgr_state *o = &omgr_state;
    struct os_event *ev;
    struct os_callout_func *ocf;
    os_time_t next_event;

    oc_main_init((oc_handler_t *)&omgr_oc_handler);
    while (1) {
        ev = os_eventq_get(&o->os_evq);
        switch (ev->ev_type) {
        case OMGR_OC_EVENT:
        case OMGR_OC_TIMER:
            next_event = oc_main_poll();
            if (next_event) {
                os_callout_reset(&o->os_oc_timer, next_event - os_time_get());
            } else {
                os_callout_stop(&o->os_oc_timer);
            }
            break;
        case OS_EVENT_T_TIMER:
            ocf = (struct os_callout_func *)ev;
            ocf->cf_func(CF_ARG(ocf));
            break;
        }
    }
}

int
oicmgr_init(void)
{
    struct omgr_state *o = &omgr_state;
    int rc;

    os_eventq_init(&o->os_evq);

    rc = os_task_init(&o->os_task, "newtmgr_oic", omgr_oic_task, NULL,
      MYNEWT_VAL(OICMGR_TASK_PRIO), OS_WAIT_FOREVER,
      oicmgr_stack, OICMGR_STACK_SZ);
    if (rc != 0) {
        goto err;
    }

    rc = nmgr_os_groups_register();
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}
