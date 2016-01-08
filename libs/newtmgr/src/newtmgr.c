/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <os/os.h>
#include <os/endian.h>

#include <assert.h>

#include <shell/shell.h>
#include <newtmgr/newtmgr.h>

#include <string.h>

struct nmgr_transport g_nmgr_shell_transport;

struct os_mutex g_nmgr_group_list_lock;

struct os_eventq g_nmgr_evq;
struct os_task g_nmgr_task;

STAILQ_HEAD(, nmgr_group) g_nmgr_group_list = 
    STAILQ_HEAD_INITIALIZER(g_nmgr_group_list);

static int nmgr_def_echo(struct nmgr_hdr *, struct os_mbuf *, uint16_t, 
        struct nmgr_hdr *, struct os_mbuf *);

static struct nmgr_group nmgr_def_group;
/* ORDER MATTERS HERE.
 * Each element represents the command ID, referenced from newtmgr.
 */
static struct nmgr_handler nmgr_def_group_handlers[] = {
    {nmgr_def_echo, nmgr_def_echo}
};


struct json_encoder json_mbuf_encoder;

static int 
nmgr_def_echo(struct nmgr_hdr *nmr, struct os_mbuf *req, uint16_t srcoff,
        struct nmgr_hdr *rsp_hdr, struct os_mbuf *rsp)
{
    uint8_t echo_buf[128];
    int rc;
    
    rc = os_mbuf_copydata(req, srcoff + sizeof(*nmr), nmr->nh_len, echo_buf);
    if (rc != 0) {
        goto err;
    }

    rc = nmgr_rsp_init(rsp_hdr, rsp);
    if (rc != 0) {
        goto err;
    }

    // You can write to the mbuf here.

    rc = nmgr_rsp_finish(rsp_hdr, rsp);
    if (rc != 0) {
        goto err;
    }


    rc = nmgr_rsp_extend(rsp_hdr, rsp, echo_buf, nmr->nh_len);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

static int 
nmgr_json_write_mbuf(void *buf, uint8_t *data, int len)
{
    struct os_mbuf *m;
    int rc;

    m = (struct os_mbuf *) buf;

    rc = os_mbuf_append(m, data, len);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}


int 
nmgr_group_list_lock(void)
{
    int rc;

    if (!os_started()) {
        return (0);
    }

    rc = os_mutex_pend(&g_nmgr_group_list_lock, OS_WAIT_FOREVER);
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

    rc = os_mutex_release(&g_nmgr_group_list_lock);
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

    STAILQ_INSERT_TAIL(&g_nmgr_group_list, group, ng_next);

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

    STAILQ_FOREACH(group, &g_nmgr_group_list, ng_next) {
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

    if (handler_id > group->ng_handlers_count) {
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

static int 
nmgr_handle_req(struct nmgr_transport *nt, struct os_mbuf *req)
{
    struct os_mbuf *rsp;
    struct nmgr_handler *handler;
    struct nmgr_hdr *rsp_hdr;
    struct nmgr_hdr hdr;
    uint32_t off;
    uint32_t len;
    int rc;

    rsp = os_msys_get_pkthdr(512, 0);
    if (!rsp) {
        rc = OS_EINVAL;
        goto err;
    }

    off = 0;
    len = OS_MBUF_PKTHDR(req)->omp_len;

    while (off < len) {
        rc = os_mbuf_copydata(req, off, sizeof(hdr), &hdr);
        if (rc < 0) {
            rc = OS_EINVAL;
            goto err;
        }

        hdr.nh_len = ntohs(hdr.nh_len);
        hdr.nh_group = ntohs(hdr.nh_group);
        hdr.nh_id = ntohs(hdr.nh_id);

        handler = nmgr_find_handler(hdr.nh_group, hdr.nh_id);
        if (!handler) {
            rc = OS_EINVAL;
            goto err;
        }

        /* Build response header apriori.  Then pass to the handlers
         * to fill out the response data, and adjust length & flags.
         */
        rsp_hdr = (struct nmgr_hdr *) os_mbuf_extend(rsp, 
                sizeof(struct nmgr_hdr));
        if (!rsp_hdr) {
            rc = OS_EINVAL;
            goto err;
        }
        rsp_hdr->nh_len = 0;
        rsp_hdr->nh_flags = 0;
        rsp_hdr->nh_op = (hdr.nh_op == NMGR_OP_READ) ? NMGR_OP_READ_RSP : 
            NMGR_OP_WRITE_RSP;
        rsp_hdr->nh_group = hdr.nh_group;
        rsp_hdr->nh_id = hdr.nh_id;

        if (hdr.nh_op == NMGR_OP_READ) {
            rc = handler->nh_read(&hdr, req, off, rsp_hdr, rsp);
        } else if (hdr.nh_op == NMGR_OP_WRITE) {
            rc = handler->nh_write(&hdr, req, off, rsp_hdr, rsp);
        } else {
            rc = OS_EINVAL;
            goto err;
        }

        rsp_hdr->nh_len = htons(rsp_hdr->nh_len);
        rsp_hdr->nh_group = htons(rsp_hdr->nh_group);
        rsp_hdr->nh_id = htons(rsp_hdr->nh_id);

        off += sizeof(hdr) + OS_ALIGN(hdr.nh_len, 4);
    }

    nt->nt_output(nt, rsp);

    return (0);
err:
    os_mbuf_free_chain(rsp);
    return (rc);
}


void
nmgr_process(struct nmgr_transport *nt)
{
    struct os_mbuf *m;

    while (1) {
        m = os_mqueue_get(&nt->nt_imq);
        if (!m) {
            break;
        }

        nmgr_handle_req(nt, m);
        os_mbuf_free_chain(m);
    }
}

void
nmgr_task(void *arg)
{
    struct nmgr_transport *nt;
    struct os_event *ev;

    while (1) {
        ev = os_eventq_get(&g_nmgr_evq);
        switch (ev->ev_type) {
            case OS_EVENT_T_MQUEUE_DATA:
                nt = (struct nmgr_transport *) ev->ev_arg;
                nmgr_process(nt);
                break;
        }
    }
}

int 
nmgr_transport_init(struct nmgr_transport *nt, 
        nmgr_transport_out_func_t output_func)
{
    int rc;

    nt->nt_output = output_func;

    rc = os_mqueue_init(&nt->nt_imq, nt);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

static int 
nmgr_shell_out(struct nmgr_transport *nt, struct os_mbuf *m)
{
    int rc;

    rc = shell_nlip_output(m);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

static int
nmgr_shell_in(struct os_mbuf *m, void *arg)
{
    struct nmgr_transport *nt;
    int rc;

    nt = (struct nmgr_transport *) arg;

    rc = os_mqueue_put(&nt->nt_imq, &g_nmgr_evq, m);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}


static int 
nmgr_default_groups_register(void)
{
    int rc;

    NMGR_GROUP_SET_HANDLERS(&nmgr_def_group, nmgr_def_group_handlers);
    nmgr_def_group.ng_group_id = NMGR_GROUP_ID_DEFAULT;

    rc = nmgr_group_register(&nmgr_def_group);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}
    
int 
nmgr_task_init(uint8_t prio, os_stack_t *stack_ptr, uint16_t stack_len)
{
    int rc;

    os_eventq_init(&g_nmgr_evq);
    
    rc = nmgr_transport_init(&g_nmgr_shell_transport, nmgr_shell_out);
    if (rc != 0) {
        goto err;
    }

    rc = shell_nlip_input_register(nmgr_shell_in, 
            (void *) &g_nmgr_shell_transport);
    if (rc != 0) {
        goto err;
    }

    rc = os_task_init(&g_nmgr_task, "newtmgr", nmgr_task, NULL, prio, 
            OS_WAIT_FOREVER, stack_ptr, stack_len);
    if (rc != 0) {
        goto err;
    }

    rc = nmgr_default_groups_register();
    if (rc != 0) {
        goto err;
    }

    rc = json_encoder_init(&json_mbuf_encoder, json_write_mbuf);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}


