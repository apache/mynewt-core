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

#include <assert.h>
#include <string.h>

#include "syscfg/syscfg.h"
#include "sysinit/sysinit.h"
#include "os/os.h"
#include "os/endian.h"

#include "newtmgr/newtmgr.h"
#include "nmgr_os/nmgr_os.h"

os_stack_t newtmgr_stack[OS_STACK_ALIGN(MYNEWT_VAL(NEWTMGR_STACK_SIZE))];

struct os_mutex g_nmgr_group_list_lock;

static struct os_eventq g_nmgr_evq;
static struct os_task g_nmgr_task;

STAILQ_HEAD(, nmgr_group) g_nmgr_group_list =
    STAILQ_HEAD_INITIALIZER(g_nmgr_group_list);

/* JSON buffer for NMGR task
 */
static struct nmgr_jbuf nmgr_task_jbuf;

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
    int rc;

    njb = (struct nmgr_jbuf *) jb;

    if (njb->njb_off + 1 > njb->njb_end) {
        return '\0';
    }

    rc = os_mbuf_copydata(njb->njb_in_m, njb->njb_off, 1, &c);
    if (rc == -1) {
        c = '\0';
    }
    ++njb->njb_off;

    return (c);
}

static char
nmgr_jbuf_read_prev(struct json_buffer *jb)
{
    struct nmgr_jbuf *njb;
    char c;
    int rc;

    njb = (struct nmgr_jbuf *) jb;

    if (njb->njb_off == 0) {
        return '\0';
    }

    --njb->njb_off;
    rc = os_mbuf_copydata(njb->njb_in_m, njb->njb_off, 1, &c);
    if (rc == -1) {
        c = '\0';
    }

    return (c);
}

static int
nmgr_jbuf_readn(struct json_buffer *jb, char *buf, int size)
{
    struct nmgr_jbuf *njb;
    int read;
    int left;
    int rc;

    njb = (struct nmgr_jbuf *) jb;

    left = njb->njb_end - njb->njb_off;
    read = size > left ? left : size;

    rc = os_mbuf_copydata(njb->njb_in_m, njb->njb_off, read, buf);
    if (rc != 0) {
        goto err;
    }

    return (read);
err:
    return (rc);
}

int
nmgr_jbuf_write(void *arg, char *data, int len)
{
    struct nmgr_jbuf *njb;
    int rc;

    njb = (struct nmgr_jbuf *) arg;

    rc = nmgr_rsp_extend(njb->njb_hdr, njb->njb_out_m, data, len);
    if (rc != 0) {
        assert(0);
        goto err;
    }

    return (0);
err:
    return (rc);
}

static int
nmgr_jbuf_init(struct nmgr_jbuf *njb)
{
    memset(njb, 0, sizeof(*njb));

    njb->njb_buf.jb_read_next = nmgr_jbuf_read_next;
    njb->njb_buf.jb_read_prev = nmgr_jbuf_read_prev;
    njb->njb_buf.jb_readn = nmgr_jbuf_readn;
    njb->njb_enc.je_write = nmgr_jbuf_write;
    njb->njb_enc.je_arg = njb;

    return (0);
}

static void
nmgr_jbuf_setibuf(struct nmgr_jbuf *njb, struct os_mbuf *m,
        uint16_t off, uint16_t len)
{
    njb->njb_off = off;
    njb->njb_end = off + len;
    njb->njb_in_m = m;
    njb->njb_enc.je_wr_commas = 0;
}

static void
nmgr_jbuf_setobuf(struct nmgr_jbuf *njb, struct nmgr_hdr *hdr,
        struct os_mbuf *m)
{
    njb->njb_out_m = m;
    njb->njb_hdr = hdr;
}

void
nmgr_jbuf_setoerr(struct nmgr_jbuf *njb, int errcode)
{
    struct json_value jv;

    json_encode_object_start(&njb->njb_enc);
    JSON_VALUE_INT(&jv, errcode);
    json_encode_object_entry(&njb->njb_enc, "rc", &jv);
    json_encode_object_finish(&njb->njb_enc);
}

static struct nmgr_hdr*
nmgr_init_rsp(struct os_mbuf *m, struct nmgr_hdr *src)
{
    struct nmgr_hdr *hdr;

    hdr = (struct nmgr_hdr *) os_mbuf_extend(m, sizeof(struct nmgr_hdr));
    if (!hdr) {
        return NULL;
    }
    memcpy(hdr, src, sizeof(*hdr));
    hdr->nh_len = 0;
    hdr->nh_flags = 0;
    hdr->nh_op = (src->nh_op == NMGR_OP_READ) ? NMGR_OP_READ_RSP :
      NMGR_OP_WRITE_RSP;
    hdr->nh_group = src->nh_group;
    hdr->nh_seq = src->nh_seq;
    hdr->nh_id = src->nh_id;

    nmgr_jbuf_setobuf(&nmgr_task_jbuf, hdr, m);

    return hdr;
}

static void
nmgr_send_err_rsp(struct nmgr_transport *nt, struct os_mbuf *m,
  struct nmgr_hdr *hdr, int rc)
{
    hdr = nmgr_init_rsp(m, hdr);
    if (!hdr) {
        return;
    }
    nmgr_jbuf_setoerr(&nmgr_task_jbuf, rc);
    hdr->nh_len = htons(hdr->nh_len);
    hdr->nh_flags = NMGR_F_JSON_RSP_COMPLETE;
    nt->nt_output(nt, nmgr_task_jbuf.njb_out_m);
}

static int
nmgr_send_rspfrag(struct nmgr_transport *nt, struct nmgr_hdr *rsp_hdr,
                  struct os_mbuf *rsp, struct os_mbuf *req, uint16_t len,
                  uint16_t *offset) {

    struct os_mbuf *rspfrag;
    int rc;

    rspfrag = NULL;

    rspfrag = os_msys_get_pkthdr(len, OS_MBUF_USRHDR_LEN(req));
    if (!rspfrag) {
        rc = NMGR_ERR_ENOMEM;
        goto err;
    }

    /* Copy the request packet header into the response. */
    memcpy(OS_MBUF_USRHDR(rspfrag), OS_MBUF_USRHDR(req), OS_MBUF_USRHDR_LEN(req));

    if (os_mbuf_append(rspfrag, rsp_hdr, sizeof(struct nmgr_hdr))) {
        rc = NMGR_ERR_ENOMEM;
        goto err;
    }

    if (os_mbuf_appendfrom(rspfrag, rsp, *offset, len)) {
        rc = NMGR_ERR_ENOMEM;
        goto err;
    }

    *offset += len;

    len = htons(len);

    if (os_mbuf_copyinto(rspfrag, offsetof(struct nmgr_hdr, nh_len), &len, sizeof(len))) {
        rc = NMGR_ERR_ENOMEM;
        goto err;
    }

    nt->nt_output(nt, rspfrag);

    return NMGR_ERR_EOK;
err:
    if (rspfrag) {
        os_mbuf_free_chain(rspfrag);
    }
    return rc;
}

static int
nmgr_rsp_fragment(struct nmgr_transport *nt, struct nmgr_hdr *rsp_hdr,
                  struct os_mbuf *rsp, struct os_mbuf *req) {

    uint16_t offset;
    uint16_t len;
    uint16_t mtu;
    int rc;

    offset = sizeof(struct nmgr_hdr);
    len = rsp_hdr->nh_len;

    mtu = nt->nt_get_mtu(req) - sizeof(struct nmgr_hdr);

    do {
        if (len <= mtu) {
            rsp_hdr->nh_flags |= NMGR_F_JSON_RSP_COMPLETE;
        } else {
            len = mtu;
        }

        rc = nmgr_send_rspfrag(nt, rsp_hdr, rsp, req, len, &offset);
        if (rc) {
            goto err;
        }

        len = rsp_hdr->nh_len - offset + sizeof(struct nmgr_hdr);

    } while (!((rsp_hdr->nh_flags & NMGR_F_JSON_RSP_COMPLETE) ==
                NMGR_F_JSON_RSP_COMPLETE));

    return NMGR_ERR_EOK;
err:
    return rc;
}

static void
nmgr_handle_req(struct nmgr_transport *nt, struct os_mbuf *req)
{
    struct os_mbuf *rsp;
    struct nmgr_handler *handler;
    struct nmgr_hdr *rsp_hdr;
    struct nmgr_hdr hdr;
    int off;
    uint16_t len;
    int rc;

    rsp_hdr = NULL;

    rsp = os_msys_get_pkthdr(512, OS_MBUF_USRHDR_LEN(req));
    if (!rsp) {
        rc = os_mbuf_copydata(req, 0, sizeof(hdr), &hdr);
        if (rc < 0) {
            goto err_norsp;
        }
        rsp = req;
        req = NULL;
        goto err;
    }

    /* Copy the request packet header into the response. */
    memcpy(OS_MBUF_USRHDR(rsp), OS_MBUF_USRHDR(req), OS_MBUF_USRHDR_LEN(req));

    off = 0;
    len = OS_MBUF_PKTHDR(req)->omp_len;

    while (off < len) {
        rc = os_mbuf_copydata(req, off, sizeof(hdr), &hdr);
        if (rc < 0) {
            rc = NMGR_ERR_EINVAL;
            goto err_norsp;
        }

        hdr.nh_len = ntohs(hdr.nh_len);

        handler = nmgr_find_handler(ntohs(hdr.nh_group), hdr.nh_id);
        if (!handler) {
            rc = NMGR_ERR_ENOENT;
            goto err;
        }

        /* Build response header apriori.  Then pass to the handlers
         * to fill out the response data, and adjust length & flags.
         */
        rsp_hdr = nmgr_init_rsp(rsp, &hdr);
        if (!rsp_hdr) {
            rc = NMGR_ERR_ENOMEM;
            goto err_norsp;
        }

        /*
         * Setup state for JSON encoding.
         */
        nmgr_jbuf_setibuf(&nmgr_task_jbuf, req, off + sizeof(hdr), hdr.nh_len);

        if (hdr.nh_op == NMGR_OP_READ) {
            if (handler->nh_read) {
                rc = handler->nh_read(&nmgr_task_jbuf);
            } else {
                rc = NMGR_ERR_ENOENT;
            }
        } else if (hdr.nh_op == NMGR_OP_WRITE) {
            if (handler->nh_write) {
                rc = handler->nh_write(&nmgr_task_jbuf);
            } else {
                rc = NMGR_ERR_ENOENT;
            }
        } else {
            rc = NMGR_ERR_EINVAL;
        }

        if (rc != 0) {
            goto err;
        }

        off += sizeof(hdr) + OS_ALIGN(hdr.nh_len, 4);
        rc = nmgr_rsp_fragment(nt, rsp_hdr, rsp, req);
        if (rc) {
            goto err;
        }
    }

    os_mbuf_free_chain(rsp);
    os_mbuf_free_chain(req);
    return;
err:
    OS_MBUF_PKTHDR(rsp)->omp_len = rsp->om_len = 0;
    nmgr_send_err_rsp(nt, rsp, &hdr, rc);
    os_mbuf_free_chain(req);
    return;
err_norsp:
    os_mbuf_free_chain(rsp);
    os_mbuf_free_chain(req);
    return;
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
    struct os_callout_func *ocf;

    nmgr_jbuf_init(&nmgr_task_jbuf);

    while (1) {
        ev = os_eventq_get(&g_nmgr_evq);
        switch (ev->ev_type) {
            case OS_EVENT_T_MQUEUE_DATA:
                nt = (struct nmgr_transport *) ev->ev_arg;
                nmgr_process(nt);
                break;
	    case OS_EVENT_T_TIMER:
		ocf = (struct os_callout_func *)ev;
		ocf->cf_func(CF_ARG(ocf));
		break;
        }
    }
}

int
nmgr_transport_init(struct nmgr_transport *nt,
        nmgr_transport_out_func_t output_func,
        nmgr_transport_get_mtu_func_t get_mtu_func)
{
    int rc;

    nt->nt_output = output_func;
    nt->nt_get_mtu = get_mtu_func;

    rc = os_mqueue_init(&nt->nt_imq, nt);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

/**
 * Transfers an incoming request to the newtmgr task.  The caller relinquishes
 * ownership of the supplied mbuf upon calling this function, whether this
 * function succeeds or fails.
 *
 * @param nt                    The transport that the request was received
 *                                  over.
 * @param req                   An mbuf containing the newtmgr request.
 *
 * @return                      0 on success; nonzero on failure.
 */
int
nmgr_rx_req(struct nmgr_transport *nt, struct os_mbuf *req)
{
    int rc;

    rc = os_mqueue_put(&nt->nt_imq, &g_nmgr_evq, req);
    if (rc != 0) {
        os_mbuf_free_chain(req);
    }

    return rc;
}

int
nmgr_task_init(void)
{
    int rc;

    os_eventq_init(&g_nmgr_evq);

    rc = os_task_init(&g_nmgr_task, "newtmgr", nmgr_task, NULL,
      MYNEWT_VAL(NEWTMGR_TASK_PRIO), OS_WAIT_FOREVER,
      newtmgr_stack, OS_STACK_ALIGN(MYNEWT_VAL(NEWTMGR_STACK_SIZE)));
    if (rc != 0) {
        goto err;
    }

    rc = nmgr_os_groups_register(&g_nmgr_evq);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

void
nmgr_pkg_init(void)
{
    int rc;

    rc = nmgr_task_init();
    SYSINIT_PANIC_ASSERT(rc == 0);
}
