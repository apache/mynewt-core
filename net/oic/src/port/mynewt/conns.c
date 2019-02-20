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
#include <os/mynewt.h>
#include <log/log.h>

#include "oic/oc_log.h"
#include "oic/port/oc_connectivity.h"
#include "oic/port/mynewt/adaptor.h"

#define OC_CONN_EV_CB   MYNEWT_VAL(OC_CONN_EV_CB_CNT)

static SLIST_HEAD(, oc_conn_cb) oc_conn_cbs =
    SLIST_HEAD_INITIALIZER(oc_conn_cbs);
static STAILQ_HEAD(, oc_conn_ev) oc_conn_evs =
    STAILQ_HEAD_INITIALIZER(oc_conn_evs);

static void oc_conn_ev_deliver(struct os_event *);
static struct os_event oc_conn_cb_ev = {
    .ev_cb = oc_conn_ev_deliver,
};
static struct os_mempool oc_conn_ev_pool;
static uint8_t oc_conn_ev_area[OS_MEMPOOL_BYTES(OC_CONN_EV_CB,
                                                sizeof(struct oc_conn_ev))];

void
oc_conn_cb_register(struct oc_conn_cb *cb)
{
    SLIST_INSERT_HEAD(&oc_conn_cbs, cb, occ_next);
}

void
oc_conn_init(void)
{
    os_mempool_init(&oc_conn_ev_pool, OC_CONN_EV_CB, sizeof(struct oc_conn_ev),
                    oc_conn_ev_area, "oc_conn_ev");
}

struct oc_conn_ev *
oc_conn_ev_alloc(void)
{
    return os_memblock_get(&oc_conn_ev_pool);
}

static void
oc_conn_ev_queue(struct oc_conn_ev *oce)
{
    int sr;

    OS_ENTER_CRITICAL(sr);
    STAILQ_INSERT_TAIL(&oc_conn_evs, oce, oce_next);
    OS_EXIT_CRITICAL(sr);
    os_eventq_put(oc_evq_get(), &oc_conn_cb_ev);
}

void
oc_conn_created(struct oc_conn_ev *oce)
{
    OC_LOG(DEBUG, "oc_conn_created: ");
    OC_LOG_ENDPOINT(LOG_LEVEL_DEBUG, &oce->oce_oe);
    oce->oce_type = OC_ENDPOINT_CONN_EV_OPEN;
    oc_conn_ev_queue(oce);
}

void
oc_conn_removed(struct oc_conn_ev *oce)
{
    OC_LOG(DEBUG, "oc_conn_removed: ");
    OC_LOG_ENDPOINT(LOG_LEVEL_DEBUG, &oce->oce_oe);
    oce->oce_type = OC_ENDPOINT_CONN_EV_CLOSE;
    oc_conn_ev_queue(oce);
}

static void
oc_conn_ev_deliver(struct os_event *ev)
{
    struct oc_conn_ev *oce;
    struct oc_conn_cb *occ;
    int sr;

    OS_ENTER_CRITICAL(sr);
    while ((oce = STAILQ_FIRST(&oc_conn_evs))) {
        STAILQ_REMOVE_HEAD(&oc_conn_evs, oce_next);
        OS_EXIT_CRITICAL(sr);
        SLIST_FOREACH(occ, &oc_conn_cbs, occ_next) {
            occ->occ_func(&oce->oce_oe, oce->oce_type);
        }
        os_memblock_put(&oc_conn_ev_pool, oce);
        OS_ENTER_CRITICAL(sr);
    }
    OS_EXIT_CRITICAL(sr);
}
