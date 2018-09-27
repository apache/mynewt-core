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

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include <os/os.h>
#include <os/os_mempool.h>
#include <log/log.h>
#include <bsp/bsp.h>
#include <log/log.h>
#include <log/log_async.h>

static os_stack_t log_task_stack[MYNEWT_VAL(LOG_ASYNC_TASK_STACK_SIZE)] __attribute__ ((aligned (8)));
static struct os_task log_task;

static struct os_mqueue log_mqueue;
static struct os_eventq log_equeue;

#define NUM_BLOCKS MYNEWT_VAL(LOG_ASYNC_MEMPOL_NUM_BLOCKS)
#define BLOCK_SIZE MYNEWT_VAL(LOG_ASYNC_MEMPOL_BLOCK_SIZE)

static os_membuf_t log_mempool_buffer[OS_MEMPOOL_SIZE(NUM_BLOCKS, BLOCK_SIZE)];
static struct os_mempool log_mempool;
static struct os_mbuf_pool log_mbuf_pool;

struct log_async_pkt_header
{
    struct log *log;
};

static struct log sync_log;

static int
log_async_read(struct log *log, void *dptr, void *buf, uint16_t offset,
               uint16_t len)
{
    const struct log_handler *sync_handler;

    sync_handler = sync_log.l_log;

    return sync_handler->log_read(&sync_log, dptr, buf, offset, len);
}

static int
log_async_read_mbuf(struct log *log, void *dptr, struct os_mbuf *om,
                    uint16_t offset, uint16_t len)
{
    const struct log_handler *sync_handler;

    sync_handler = sync_log.l_log;

    return sync_handler->log_read_mbuf(&sync_log, dptr, om, offset, len);
}

static struct os_mbuf *
log_async_get_mbuf(const struct log_async_pkt_header *header)
{
    struct os_mbuf *nom = os_mbuf_get_pkthdr(&log_mbuf_pool, sizeof(*header));
    if (nom) {
        memcpy(OS_MBUF_USRHDR(nom), header, sizeof(*header));
    }
    return nom;
}

static int
log_async_append(struct log *log, void *buf, int len)
{
    struct os_mbuf *nom = NULL;
    struct log_async_pkt_header header = { .log = log };
    int rc = 0;

    nom = log_async_get_mbuf(&header);
    if (nom == NULL) {
        return OS_ENOMEM;
    }
    rc = os_mbuf_copyinto(nom, 0, buf, len);
    if (rc) {
        goto err;
    }
    rc = os_mqueue_put(&log_mqueue, &log_equeue, nom);
err:
    if (rc) {
        os_mbuf_free_chain(nom);
    }
    return rc;
}

static int
log_async_append_body(struct log *log, const struct log_entry_hdr *hdr,
                      const void *body, int body_len)
{
    struct os_mbuf *nom = NULL;
    struct log_async_pkt_header header = { .log = log };
    int rc = 0;

    nom = log_async_get_mbuf(&header);
    if (nom == NULL) {
        return OS_ENOMEM;
    }
    rc = os_mbuf_copyinto(nom, 0, hdr, sizeof(*hdr));
    if (rc) {
        goto err;
    }
    rc = os_mbuf_copyinto(nom, sizeof(*hdr), body, body_len);
    if (rc) {
        goto err;
    }
    rc = os_mqueue_put(&log_mqueue, &log_equeue, nom);
err:
    if (rc) {
        os_mbuf_free_chain(nom);
    }
    return rc;
}

static int
log_async_append_mbuf(struct log *log, const struct os_mbuf *om)
{
    struct os_mbuf *nom;
    struct log_async_pkt_header header = { .log = log };
    int rc = 0;

    nom = log_async_get_mbuf(&header);
    if (nom == NULL) {
        return OS_ENOMEM;
    }
    rc = os_mbuf_appendfrom(nom, om, 0, os_mbuf_len(om));
    if (rc) {
        goto err;
    }
    rc = os_mqueue_put(&log_mqueue, &log_equeue, nom);
err:
    if (rc) {
        os_mbuf_free_chain(nom);
    }
    return rc;
}

static int
log_async_append_mbuf_body(struct log *log, const struct log_entry_hdr *hdr,
                           const struct os_mbuf *om)
{
    struct os_mbuf *nom = NULL;
    struct log_async_pkt_header header = { .log = log };
    int rc = 0;

    nom = log_async_get_mbuf(&header);
    if (nom == NULL) {
        return OS_ENOMEM;
    }
    rc = os_mbuf_copyinto(nom, 0, hdr, sizeof(*hdr));
    if (rc) {
        goto err;
    }
    rc = os_mbuf_appendfrom(nom, om, 0, os_mbuf_len(om));
    if (rc) {
        goto err;
    }
    rc = os_mqueue_put(&log_mqueue, &log_equeue, nom);

err:
    if (rc) {
        os_mbuf_free_chain(nom);
    }
    return rc;
}

static int
log_async_walk(struct log *log, log_walk_func_t walk_func,
               struct log_offset *log_offset)
{
    const struct log_handler *sync_handler;

    sync_handler = sync_log.l_log;

    return sync_handler->log_walk(&sync_log, walk_func, log_offset);
}

static int
log_async_flush(struct log *log)
{
    const struct log_handler *sync_handler;

    sync_handler = sync_log.l_log;

    return sync_handler->log_flush(&sync_log);
}

struct log_handler async_handler =
{
    .log_type = LOG_TYPE_STORAGE,
    .log_read = log_async_read,
    .log_read_mbuf = log_async_read_mbuf,
    .log_append = log_async_append,
    .log_append_body = log_async_append_body,
    .log_append_mbuf = log_async_append_mbuf,
    .log_append_mbuf_body = log_async_append_mbuf_body,
    .log_walk = log_async_walk,
    .log_flush = log_async_flush,
};

static void
log_async_task_f(void *arg)
{
    /* Main log handling loop */
    while (1)
    {
        os_eventq_run(&log_equeue);
    }
}

static void
log_async_handle_log(struct log *log, struct os_mbuf *req)
{
    struct log_async_pkt_header header;

    memcpy(&header, OS_MBUF_USRHDR(req), sizeof(header));
    sync_log.l_log->log_append_mbuf(&sync_log, req);
    os_mbuf_free_chain(req);
}

static void
log_process(struct log *log)
{
    struct os_mbuf *m;

    do {
        m = os_mqueue_get(&log_mqueue);
        if (m) {
            log_async_handle_log(log, m);
        }
    } while (m);
}

static void
log_event_data_in(struct os_event *ev)
{
    log_process(ev->ev_arg);
}

int
log_switch_to_async(struct log *log)
{
    int rc;

    rc = os_mempool_init(&log_mempool, NUM_BLOCKS, BLOCK_SIZE,
                         log_mempool_buffer, "log");
    if (rc) {
        goto err;
    }

    rc = os_mbuf_pool_init(&log_mbuf_pool, &log_mempool, BLOCK_SIZE, NUM_BLOCKS);
    if (rc) {
        goto err;
    }

    os_eventq_init(&log_equeue);

    rc = os_mqueue_init(&log_mqueue, log_event_data_in, &log);
    if (rc) {
        goto err;
    }

    /*
     * Copy log structure needed for calling lower driver
     * Change linked handler to point to async functions
     */
    sync_log = *log;
    log->l_log = &async_handler;

    rc = os_task_init(&log_task, "log", log_async_task_f, NULL,
                      MYNEWT_VAL(LOG_ASYNC_TASK_PRIORITY), OS_WAIT_FOREVER,
                      log_task_stack, MYNEWT_VAL(LOG_ASYNC_TASK_STACK_SIZE));
    assert(rc == 0);
err:
    return rc;

}
