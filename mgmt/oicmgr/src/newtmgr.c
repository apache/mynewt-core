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

#include <oic/oc_api.h>

#define OMGR_OC_EVENT	(OS_EVENT_T_PERUSER)
#define OMGR_OC_TIMER	(OS_EVENT_T_PERUSER + 1)
#define OICMGR_STACK_SZ	OS_STACK_ALIGN(MYNEWT_VAL(OICMGR_STACK_SIZE))

struct omgr_jbuf {
    struct mgmt_jbuf ob_m;
    char *ob_in;
    uint16_t ob_in_off;
    uint16_t ob_in_end;
    char *ob_out;
    uint16_t ob_out_off;
    uint16_t ob_out_end;
};

struct omgr_state {
    struct os_eventq os_evq;
    struct os_event os_oc_event;
    struct os_callout os_oc_timer;
    struct os_task os_task;
    struct omgr_jbuf os_jbuf;		/* JSON buffer for NMGR task */
    char os_rsp[MGMT_MAX_MTU];
};

static struct omgr_state omgr_state = {
  .os_oc_event.ev_type = OMGR_OC_EVENT,
  .os_oc_timer.c_ev.ev_type = OMGR_OC_TIMER,
  .os_oc_timer.c_evq = &omgr_state.os_evq
};

static os_stack_t oicmgr_stack[OICMGR_STACK_SZ];

static void omgr_oic_get(oc_request_t *request, oc_interface_mask_t interface);
static void omgr_oic_put(oc_request_t *request, oc_interface_mask_t interface);

static char
omgr_jbuf_read_next(struct json_buffer *jb)
{
    struct omgr_jbuf *njb;
    char c;

    njb = (struct omgr_jbuf *) jb;

    if (njb->ob_in_off + 1 > njb->ob_in_end) {
        return '\0';
    }

    c = njb->ob_in[njb->ob_in_off];
    ++njb->ob_in_off;

    return (c);
}

static char
omgr_jbuf_read_prev(struct json_buffer *jb)
{
    struct omgr_jbuf *njb;
    char c;

    njb = (struct omgr_jbuf *) jb;

    if (njb->ob_in_off == 0) {
        return '\0';
    }

    --njb->ob_in_off;
    c = njb->ob_in[njb->ob_in_off];

    return (c);
}

static int
omgr_jbuf_readn(struct json_buffer *jb, char *buf, int size)
{
    struct omgr_jbuf *njb;
    int read;
    int left;

    njb = (struct omgr_jbuf *)jb;

    left = njb->ob_in_end - njb->ob_in_off;
    read = size > left ? left : size;

    memcpy(buf, njb->ob_in + njb->ob_in_off, read);

    return (read);
}

static int
omgr_jbuf_write(void *arg, char *data, int len)
{
    struct omgr_jbuf *njb;
    int rc;

    njb = (struct omgr_jbuf *)arg;

    if (njb->ob_out_off + len >= njb->ob_out_end) {
        assert(0);
        goto err;
    }
    memcpy(njb->ob_out + njb->ob_out_off, data, len);
    njb->ob_out_off += len;
    njb->ob_out[njb->ob_out_off] = '\0';

    return (0);
err:
    return (rc);
}

static void
omgr_jbuf_init(struct omgr_jbuf *ob)
{
    struct mgmt_jbuf *mjb;

    memset(ob, 0, sizeof(*ob));

    mjb = &ob->ob_m;
    mjb->mjb_buf.jb_read_next = omgr_jbuf_read_next;
    mjb->mjb_buf.jb_read_prev = omgr_jbuf_read_prev;
    mjb->mjb_buf.jb_readn = omgr_jbuf_readn;
    mjb->mjb_enc.je_write = omgr_jbuf_write;
    mjb->mjb_enc.je_arg = ob;
}

static void
omgr_jbuf_setibuf(struct omgr_jbuf *ob, char *ptr, uint16_t len)
{
    ob->ob_in_off = 0;
    ob->ob_in_end = len;
    ob->ob_in = ptr;
}

static void
omgr_jbuf_setobuf(struct omgr_jbuf *ob, char *ptr, uint16_t maxlen)
{
    ob->ob_out = ptr;
    ob->ob_out_off = 0;
    ob->ob_out_end = maxlen;
    ob->ob_out[0] = '\0';
    ob->ob_m.mjb_enc.je_wr_commas = 0;
}

static struct mgmt_handler *
omgr_oic_find_handler(const char *q, int qlen)
{
    int grp = -1;
    int id = -1;
    char *str;
    char *eptr;
    int slen;

    slen = oc_ri_get_query_value(q, qlen, "gr", &str);
    if (slen > 0) {
        grp = strtoul(str, &eptr, 0);
        if (*eptr != '\0' && *eptr != '&') {
            return NULL;
        }
    }
    slen = oc_ri_get_query_value(q, qlen, "id", &str);
    if (slen > 0) {
        id = strtoul(str, &eptr, 0);
        if (*eptr != '\0' && *eptr != '&') {
            return NULL;
        }
    }
    return mgmt_find_handler(grp, id);
}

static void
omgr_oic_op(oc_request_t *req, oc_interface_mask_t mask, int isset)
{
    struct omgr_state *o = &omgr_state;
    struct mgmt_handler *handler;
    oc_rep_t *data;
    int rc;

    if (!req->query_len) {
        goto bad_req;
    }

    handler = omgr_oic_find_handler(req->query, req->query_len);
    if (!handler) {
        goto bad_req;
    }

    /*
     * Setup state for JSON encoding.
     */
    omgr_jbuf_setobuf(&o->os_jbuf, o->os_rsp, sizeof(o->os_rsp));

    data = req->request_payload;
    if (data) {
        if (data->type != STRING) {
            goto bad_req;
        }
        omgr_jbuf_setibuf(&o->os_jbuf, oc_string(data->value_string),
          oc_string_len(data->value_string));
    } else {
        omgr_jbuf_setibuf(&o->os_jbuf, NULL, 0);
    }

    if (!isset) {
        if (handler->mh_read) {
            rc = handler->mh_read(&o->os_jbuf.ob_m);
        } else {
            goto bad_req;
        }
    } else {
        if (handler->mh_write) {
            rc = handler->mh_write(&o->os_jbuf.ob_m);
        } else {
            goto bad_req;
        }
    }
    if (rc) {
        goto bad_req;
    }

    oc_rep_start_root_object();
    switch (mask) {
    case OC_IF_BASELINE:
        oc_process_baseline_interface(req->resource);
    case OC_IF_RW:
        oc_rep_set_text_string(root, "key", o->os_rsp);
        break;
    default:
        break;
    }
    oc_rep_end_root_object();
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

    omgr_jbuf_init(&o->os_jbuf);

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

    rc = nmgr_os_groups_register(&o->os_evq);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}
