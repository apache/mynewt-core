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

#include <os/os.h>
#include <os/endian.h>

#include <assert.h>
#include <string.h>

#include <newtmgr/newtmgr.h>
#include <nmgr_os/nmgr_os.h>

#include <iotivity/oc_api.h>

#define NMGR_OC_EVENT	(OS_EVENT_T_PERUSER)
#define NMGR_OC_TIMER	(OS_EVENT_T_PERUSER + 1)

struct nmgr_state {
    struct os_mutex ns_group_lock;
    STAILQ_HEAD(, nmgr_group) ns_groups;
    struct os_eventq ns_evq;
    struct os_event ns_oc_event;
    struct os_callout ns_oc_timer;
    struct os_task ns_task;
    struct nmgr_jbuf ns_jbuf;		/* JSON buffer for NMGR task */
    char ns_rsp[NMGR_MAX_MTU];
};

static struct nmgr_state nmgr_state = {
  .ns_groups = STAILQ_HEAD_INITIALIZER(nmgr_state.ns_groups),
  .ns_oc_event.ev_type = NMGR_OC_EVENT,
  .ns_oc_timer.c_ev.ev_type = NMGR_OC_TIMER,
  .ns_oc_timer.c_evq = &nmgr_state.ns_evq
};

static void nmgr_oic_get(oc_request_t *request, oc_interface_mask_t interface);
static void nmgr_oic_put(oc_request_t *request, oc_interface_mask_t interface);

int
nmgr_group_list_lock(void)
{
    int rc;

    if (!os_started()) {
        return (0);
    }

    rc = os_mutex_pend(&nmgr_state.ns_group_lock, OS_WAIT_FOREVER);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

int
nmgr_group_list_unlock(void)
{
    int rc;

    if (!os_started()) {
        return (0);
    }

    rc = os_mutex_release(&nmgr_state.ns_group_lock);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}


int
nmgr_group_register(struct nmgr_group *group)
{
    int rc;

    rc = nmgr_group_list_lock();
    if (rc != 0) {
        goto err;
    }

    STAILQ_INSERT_TAIL(&nmgr_state.ns_groups, group, ng_next);

    rc = nmgr_group_list_unlock();
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

static struct nmgr_group *
nmgr_find_group(uint16_t group_id)
{
    struct nmgr_group *group;
    int rc;

    group = NULL;

    rc = nmgr_group_list_lock();
    if (rc != 0) {
        goto err;
    }

    STAILQ_FOREACH(group, &nmgr_state.ns_groups, ng_next) {
        if (group->ng_group_id == group_id) {
            break;
        }
    }

    rc = nmgr_group_list_unlock();
    if (rc != 0) {
        goto err;
    }

    return (group);
err:
    return (NULL);
}

static struct nmgr_handler *
nmgr_find_handler(uint16_t group_id, uint16_t handler_id)
{
    struct nmgr_group *group;
    struct nmgr_handler *handler;

    group = nmgr_find_group(group_id);
    if (!group) {
        goto err;
    }

    if (handler_id >= group->ng_handlers_count) {
        goto err;
    }

    handler = &group->ng_handlers[handler_id];

    return (handler);
err:
    return (NULL);
}

int
nmgr_rsp_extend(struct nmgr_hdr *hdr, struct os_mbuf *rsp, void *data,
        uint16_t len)
{
    int rc;

    rc = os_mbuf_append(rsp, data, len);
    if (rc != 0) {
        goto err;
    }
    hdr->nh_len += len;

    return (0);
err:
    return (rc);
}

static char
nmgr_jbuf_read_next(struct json_buffer *jb)
{
    struct nmgr_jbuf *njb;
    char c;

    njb = (struct nmgr_jbuf *) jb;

    if (njb->njb_in_off + 1 > njb->njb_in_end) {
        return '\0';
    }

    c = njb->njb_in[njb->njb_in_off];
    ++njb->njb_in_off;

    return (c);
}

static char
nmgr_jbuf_read_prev(struct json_buffer *jb)
{
    struct nmgr_jbuf *njb;
    char c;

    njb = (struct nmgr_jbuf *) jb;

    if (njb->njb_in_off == 0) {
        return '\0';
    }

    --njb->njb_in_off;
    c = njb->njb_in[njb->njb_in_off];

    return (c);
}

static int
nmgr_jbuf_readn(struct json_buffer *jb, char *buf, int size)
{
    struct nmgr_jbuf *njb;
    int read;
    int left;

    njb = (struct nmgr_jbuf *) jb;

    left = njb->njb_in_end - njb->njb_in_off;
    read = size > left ? left : size;

    memcpy(buf, njb->njb_in + njb->njb_in_off, read);

    return (read);
}

static int
nmgr_jbuf_write(void *arg, char *data, int len)
{
    struct nmgr_jbuf *njb;
    int rc;

    njb = (struct nmgr_jbuf *) arg;

    if (njb->njb_out_off + len >= njb->njb_out_end) {
        assert(0);
        goto err;
    }
    memcpy(njb->njb_out + njb->njb_out_off, data, len);
    njb->njb_out_off += len;
    njb->njb_out[njb->njb_out_off] = '\0';

    return (0);
err:
    return (rc);
}

static void
nmgr_jbuf_init(struct nmgr_jbuf *njb)
{
    memset(njb, 0, sizeof(*njb));

    njb->njb_buf.jb_read_next = nmgr_jbuf_read_next;
    njb->njb_buf.jb_read_prev = nmgr_jbuf_read_prev;
    njb->njb_buf.jb_readn = nmgr_jbuf_readn;
    njb->njb_enc.je_write = nmgr_jbuf_write;
    njb->njb_enc.je_arg = njb;
}

static void
nmgr_jbuf_setibuf(struct nmgr_jbuf *njb, char *ptr, uint16_t len)
{
    njb->njb_in_off = 0;
    njb->njb_in_end = len;
    njb->njb_in = ptr;
    njb->njb_enc.je_wr_commas = 0;
}

static void
nmgr_jbuf_setobuf(struct nmgr_jbuf *njb, char *ptr, uint16_t maxlen)
{
    njb->njb_out = ptr;
    njb->njb_out_off = 0;
    njb->njb_out_end = maxlen;
    njb->njb_out[0] = '\0';
}

int
nmgr_jbuf_setoerr(struct nmgr_jbuf *njb, int errcode)
{
    struct json_value jv;

    json_encode_object_start(&njb->njb_enc);
    JSON_VALUE_INT(&jv, errcode);
    json_encode_object_entry(&njb->njb_enc, "rc", &jv);
    json_encode_object_finish(&njb->njb_enc);

    return (0);
}

static struct nmgr_handler *
nmgr_oic_find_handler(const char *q, int qlen)
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
    return nmgr_find_handler(grp, id);
}

static void
nmgr_oic_op(oc_request_t *req, oc_interface_mask_t mask, int isset)
{
    struct nmgr_state *ns = &nmgr_state;
    struct nmgr_handler *handler;
    oc_rep_t *data;
    int rc;

    if (!req->query_len) {
        goto bad_req;
    }

    handler = nmgr_oic_find_handler(req->query, req->query_len);
    if (!handler) {
        goto bad_req;
    }

    /*
     * Setup state for JSON encoding.
     */
    nmgr_jbuf_setobuf(&ns->ns_jbuf, ns->ns_rsp, sizeof(ns->ns_rsp));

    data = req->request_payload;
    if (data) {
        if (data->type != STRING) {
            goto bad_req;
        }
        nmgr_jbuf_setibuf(&ns->ns_jbuf, oc_string(data->value_string),
          oc_string_len(data->value_string));
    } else {
        nmgr_jbuf_setibuf(&ns->ns_jbuf, NULL, 0);
    }

    if (!isset) {
        if (handler->nh_read) {
            rc = handler->nh_read(&ns->ns_jbuf);
        } else {
            goto bad_req;
        }
    } else {
        if (handler->nh_write) {
            rc = handler->nh_write(&ns->ns_jbuf);
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
        oc_rep_set_text_string(root, "key", ns->ns_rsp);
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
nmgr_oic_get(oc_request_t *req, oc_interface_mask_t mask)
{
    nmgr_oic_op(req, mask, 0);
}

static void
nmgr_oic_put(oc_request_t *req, oc_interface_mask_t mask)
{
    nmgr_oic_op(req, mask, 1);
}

static void
nmgr_app_init(void)
{
    oc_init_platform("MyNewt", NULL, NULL);
    oc_add_device("/oic/d", "oic.d.light", "MynewtLed", "1.0", "1.0", NULL,
      NULL);
}

static void
nmgr_register_resources(void)
{
    uint8_t mode;
    oc_resource_t *res = NULL;
    char name[12];

    snprintf(name, sizeof(name), "/nmgr");
    res = oc_new_resource(name, 1, 0);
    oc_resource_bind_resource_type(res, "x.mynewt.nmgr");
    mode = OC_IF_RW;
    oc_resource_bind_resource_interface(res, mode);
    oc_resource_set_default_interface(res, mode);
    oc_resource_set_discoverable(res);
    oc_resource_set_request_handler(res, OC_GET, nmgr_oic_get);
    oc_resource_set_request_handler(res, OC_PUT, nmgr_oic_put);
    oc_add_resource(res);
}

static const oc_handler_t nmgr_oc_handler = {
    .init = nmgr_app_init,
    .register_resources = nmgr_register_resources
};

void
oc_signal_main_loop(void)
{
    struct nmgr_state *ns = &nmgr_state;

    os_eventq_put(&ns->ns_evq, &ns->ns_oc_event);
}

void
nmgr_oic_task(void *arg)
{
    struct nmgr_state *ns = &nmgr_state;
    struct os_event *ev;
    struct os_callout_func *ocf;
    os_time_t next_event;

    nmgr_jbuf_init(&ns->ns_jbuf);

    oc_main_init((oc_handler_t *)&nmgr_oc_handler);
    while (1) {
        ev = os_eventq_get(&ns->ns_evq);
        switch (ev->ev_type) {
        case NMGR_OC_EVENT:
        case NMGR_OC_TIMER:
            next_event = oc_main_poll();
            if (next_event) {
                os_callout_reset(&ns->ns_oc_timer, next_event - os_time_get());
            } else {
                os_callout_stop(&ns->ns_oc_timer);
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
nmgr_oic_init(uint8_t prio, os_stack_t *stack_ptr, uint16_t stack_len)
{
    struct nmgr_state *ns = &nmgr_state;
    int rc;

    os_eventq_init(&ns->ns_evq);

    rc = os_task_init(&ns->ns_task, "newtmgr_oic", nmgr_oic_task, NULL, prio,
            OS_WAIT_FOREVER, stack_ptr, stack_len);
    if (rc != 0) {
        goto err;
    }

    rc = nmgr_os_groups_register(&ns->ns_evq);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}
