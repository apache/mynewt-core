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

#include <string.h>

struct nmgr_transport g_nmgr_transport;

struct os_mutex g_nmgr_group_list_lock;

struct os_eventq g_nmgr_evq;
struct os_task g_nmgr_task;

STAILQ_HEAD(, nmgr_group) g_nmgr_group_list = 
    STAILQ_HEAD_INITIALIZER(nmgr_group);

int 
nmgr_group_list_lock(void)
{
    int rc;

    if (!os_started()) {
        return (0);
    }

    rc = os_mutex_pend(&g_nmgr_group_list_lock);
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

    group = nmgr_find_group(hdr->nh_group);
    if (!group) {
        goto err;
    }

    if (hdr->nh_id > group->ng_handlers_count) {
        goto err;
    }

    handler = &group->ng_handlers[hdr->nh_id];

    return (handler);
err:
    return (NULL);
}


static int 
nmgr_handle_req(struct nmgr_transport *nt, struct os_mbuf *req)
{
    struct os_mbuf *rsp;
    struct nmgr_handler *handler;
    struct nmgr_hdr hdr;
    uint32_t off;
    uint32_t len;

    rsp = os_msys_get_pkthdr(512, 0);
    if (!rsp) {
        rc = EINVAL;
        goto err;
    }

    off = 0;
    len = OS_MBUF_PKTHDR(req)->omp_len;

    while (off < len) {
        rc = os_mbuf_copydata(req, off, sizeof(hdr), &hdr);
        if (rc < 0) {
            rc = EINVAL;
            goto err;
        }

        hdr.nh_len = ntohs(hdr.nh_len);
        hdr.nh_group = ntohs(hdr.nh_group);
        hdr.nh_id = ntohs(hdr.nh_id);

        handler = nmgr_find_handler(hdr.nh_group, hdr.nh_id);
        if (!handler) {
            rc = EINVAL;
            goto err;
        }

        if (hdr.nh_op == NMGR_OP_READ) {
            rc = handler->nh_read(&hdr, req, off, rsp);
        } else if (hdr.nh_op == NMGR_OP_WRITE) {
            rc = handler->nh_write(&hdr, req, off, rsp);
        } else {
            rc = OS_EINVAL;
            goto err;
        }

        off += sizeof(hdr) + OS_ALIGN(hdr.nh_len, 4);
    }

    nt->nt_output(nt, rsp);

    return (0);
err:
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

int 
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

    
int 
nmgr_task_init(uint8_t prio, os_stack_t *stack_ptr, uint16_t stack_len)
{
    int rc;

    rc = os_eventq_init(&g_nmgr_evq);
    if (rc != 0) {
        goto err;
    }
    
    rc = nmgr_transport_init(&g_nmgr_shell_transport, nmgr_shell_out);
    if (rc != 0) {
        goto err;
    }

    rc = shell_nlip_input_register(nmgr_shell_in, 
            (void *) &g_nmgr_shell_transport);
    if (rc != 0) {
        goto err;
    }

    rc = os_task_init(&nmgr_task, "newtmgr", nmgr_task, NULL, prio, 
            OS_WAIT_FOREVER, stack_ptr, stack_len);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}


