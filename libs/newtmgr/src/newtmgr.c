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

#include <shell/shell.h>
#include <console/console.h>
#include <newtmgr/newtmgr.h>

#include "newtmgr_priv.h"

struct nmgr_transport g_nmgr_shell_transport;

struct os_mutex g_nmgr_group_list_lock;

struct os_eventq g_nmgr_evq;
struct os_task g_nmgr_task;

STAILQ_HEAD(, nmgr_group) g_nmgr_group_list =
    STAILQ_HEAD_INITIALIZER(g_nmgr_group_list);

static int nmgr_def_echo(struct nmgr_jbuf *);
static int nmgr_def_console_echo(struct nmgr_jbuf *);

static struct nmgr_group nmgr_def_group;
/* ORDER MATTERS HERE.
 * Each element represents the command ID, referenced from newtmgr.
 */
static const struct nmgr_handler nmgr_def_group_handlers[] = {
    [NMGR_ID_ECHO] = {nmgr_def_echo, nmgr_def_echo},
    [NMGR_ID_CONS_ECHO_CTRL] = {nmgr_def_console_echo, nmgr_def_console_echo},
    [NMGR_ID_TASKSTATS] = {nmgr_def_taskstat_read, NULL},
    [NMGR_ID_MPSTATS] = {nmgr_def_mpstat_read, NULL},
    [NMGR_ID_DATETIME_STR] = {nmgr_datetime_get, nmgr_datetime_set},
    [NMGR_ID_RESET] = {NULL, nmgr_reset},
};

/* JSON buffer for NMGR task
 */
struct nmgr_jbuf nmgr_task_jbuf;

static int
nmgr_def_echo(struct nmgr_jbuf *njb)
{
    uint8_t echo_buf[128];
    struct json_attr_t attrs[] = {
        { "d", t_string, .addr.string = (char *) &echo_buf[0],
            .len = sizeof(echo_buf) },
        { NULL },
    };
    struct json_value jv;
    int rc;

    rc = json_read_object((struct json_buffer *) njb, attrs);
    if (rc != 0) {
        goto err;
    }

    json_encode_object_start(&njb->njb_enc);
    JSON_VALUE_STRINGN(&jv, (char *) echo_buf, strlen((char *) echo_buf));
    json_encode_object_entry(&njb->njb_enc, "r", &jv);
    json_encode_object_finish(&njb->njb_enc);

    return (0);
err:
    return (rc);
}

static int
nmgr_def_console_echo(struct nmgr_jbuf *njb)
{
    long long int echo_on = 1;
    int rc;
    struct json_attr_t attrs[3] = {
        [0] = {
            .attribute = "echo",
            .type = t_integer,
            .addr.integer = &echo_on,
            .nodefault = 1
        },
        [1] = {
            .attribute = NULL
        }
    };

    rc = json_read_object(&njb->njb_buf, attrs);
    if (rc) {
        return OS_EINVAL;
    }

    if (echo_on) {
        console_echo(1);
    } else {
        console_echo(0);
    }
    return (0);
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

int
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

static int
nmgr_jbuf_setibuf(struct nmgr_jbuf *njb, struct os_mbuf *m,
        uint16_t off, uint16_t len)
{
    njb->njb_off = off;
    njb->njb_end = off + len;
    njb->njb_in_m = m;
    njb->njb_enc.je_wr_commas = 0;

    return (0);
}

static int
nmgr_jbuf_setobuf(struct nmgr_jbuf *njb, struct nmgr_hdr *hdr,
        struct os_mbuf *m)
{
    njb->njb_out_m = m;
    njb->njb_hdr = hdr;

    return (0);
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

    rsp = os_msys_get_pkthdr(512, OS_MBUF_USRHDR_LEN(req));
    if (!rsp) {
        rc = OS_EINVAL;
        goto err;
    }

    /* Copy the request packet header into the response. */
    memcpy(OS_MBUF_USRHDR(rsp), OS_MBUF_USRHDR(req), OS_MBUF_USRHDR_LEN(req));

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
        rsp_hdr->nh_seq = hdr.nh_seq;
        rsp_hdr->nh_id = hdr.nh_id;

        /*
         * Setup state for JSON encoding.
         */
        rc = nmgr_jbuf_setibuf(&nmgr_task_jbuf, req, off + sizeof(hdr),
          hdr.nh_len);
        if (rc) {
            goto err;
        }
        rc = nmgr_jbuf_setobuf(&nmgr_task_jbuf, rsp_hdr, rsp);
        if (rc) {
            goto err;
        }

        if (hdr.nh_op == NMGR_OP_READ) {
            if (handler->nh_read) {
                rc = handler->nh_read(&nmgr_task_jbuf);
            } else {
                rc = OS_EINVAL;
            }
        } else if (hdr.nh_op == NMGR_OP_WRITE) {
            if (handler->nh_write) {
                rc = handler->nh_write(&nmgr_task_jbuf);
            } else {
                rc = OS_EINVAL;
            }
        } else {
            rc = OS_EINVAL;
        }

        if (rc != 0) {
            goto err;
        }

        rsp_hdr->nh_len = htons(rsp_hdr->nh_len);
        rsp_hdr->nh_group = htons(rsp_hdr->nh_group);

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

    NMGR_GROUP_SET_HANDLERS(&nmgr_def_group,
      (struct nmgr_handler *)nmgr_def_group_handlers);
    nmgr_def_group.ng_group_id = NMGR_GROUP_ID_DEFAULT;
    nmgr_def_group.ng_handlers_count =
      sizeof(nmgr_def_group_handlers) / sizeof(nmgr_def_group_handlers[0]);

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

    return (0);
err:
    return (rc);
}
