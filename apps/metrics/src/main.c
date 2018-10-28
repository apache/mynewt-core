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
#include <errno.h>
#include "os/os.h"
#include "sysinit/sysinit.h"
#include "metrics/metrics.h"
#include "tinycbor/cbor_mbuf_reader.h"
#include "tinycbor/cbor.h"
#include "log/log.h"
#include "sysflash/sysflash.h"
#include "fcb/fcb.h"

/*
 * XXX
 * Metrics are numbered starting from 0 in order as defined. It may be good idea
 * to define an enum with proper symbols to identify each metric so it's easy
 * to rearrange them later. Or we may find some nice way to actually define
 * these symbols automatically, not yet sure how though...
 */

/* Define symbols for metrics */
enum {
    MY_METRIC_VAL_S,
    MY_METRIC_VAL_U,
    MY_METRIC_VAL_SS16,
    MY_METRIC_VAL_SU32,
    MY_METRIC_VAL_SS32,
};

/* Define all metrics */
METRICS_SECT_START(my_metrics)
    METRICS_SECT_ENTRY(val_s, METRICS_TYPE_SINGLE_S)
    METRICS_SECT_ENTRY(val_u, METRICS_TYPE_SINGLE_U)
    METRICS_SECT_ENTRY(val_ss16, METRICS_TYPE_SERIES_S16)
    METRICS_SECT_ENTRY(val_su32, METRICS_TYPE_SERIES_U32)
    METRICS_SECT_ENTRY(val_ss32, METRICS_TYPE_SERIES_S32)
METRICS_SECT_END;

/* Declare event struct to accommodate all metrics */
METRICS_EVENT_DECLARE(my_event, my_metrics);

/* Sample event */
struct my_event g_event;

/* Target log instance */
static struct log g_log;
static struct fcb_log g_log_fcb;
static struct flash_area g_log_fcb_fa;

static void
init_log_instance(void)
{
    const struct flash_area *fa;
    int rc;

    /* Temporarily just use reboot log fa */
    rc = flash_area_open(FLASH_AREA_REBOOT_LOG, &fa);
    assert(rc == 0);

    g_log_fcb_fa = *fa;
    g_log_fcb.fl_fcb.f_sectors = &g_log_fcb_fa;
    g_log_fcb.fl_fcb.f_sector_cnt = 1;
    g_log_fcb.fl_fcb.f_magic = 0xBABABABA;
    g_log_fcb.fl_fcb.f_version = g_log_info.li_version;

    g_log_fcb.fl_entries = 0;

    rc = fcb_init(&g_log_fcb.fl_fcb);
    if (rc) {
        flash_area_erase(fa, 0, fa->fa_size);
        rc = fcb_init(&g_log_fcb.fl_fcb);
        assert(rc == 0);
    }

    log_register("my_metrics", &g_log, &log_fcb_handler, &g_log_fcb,
                 LOG_SYSLEVEL);
}

int
main(void)
{
    struct os_mbuf *om;
    int i;

    sysinit();

    init_log_instance();

    /* Initialize event internals and enable logging for all metrics */
    metrics_event_init(&g_event.hdr, my_metrics, METRICS_SECT_COUNT(my_metrics), "myev");
    metrics_event_register(&g_event.hdr);
    metrics_set_state_mask(&g_event.hdr, 0xffffffff);

    /* Start new event */
    metrics_event_start(&g_event.hdr, os_cputime_get32());

    /* Log some data to event */
    metrics_set_value(&g_event.hdr, MY_METRIC_VAL_S, -10);
    metrics_set_value(&g_event.hdr, MY_METRIC_VAL_U, 10);
    metrics_set_value(&g_event.hdr, MY_METRIC_VAL_SU32, 100);
    metrics_set_value(&g_event.hdr, MY_METRIC_VAL_SU32, 101);
    metrics_set_value(&g_event.hdr, MY_METRIC_VAL_SU32, 102);
    metrics_set_value(&g_event.hdr, MY_METRIC_VAL_SU32, 103);

    /* Serialize event to mbuf from metrics pool */
    om = metrics_get_mbuf();
    metrics_event_to_cbor(&g_event.hdr, om);
    os_mbuf_free_chain(om);

    /* Start new event */
    metrics_event_start(&g_event.hdr, os_cputime_get32());
    metrics_event_set_log(&g_event.hdr, &g_log, LOG_MODULE_DEFAULT, LOG_LEVEL_INFO);

    /* Log some data to event */
    metrics_set_value(&g_event.hdr, MY_METRIC_VAL_S, -10);
    metrics_set_value(&g_event.hdr, MY_METRIC_VAL_U, 10);
    metrics_set_value(&g_event.hdr, MY_METRIC_VAL_U, 11);
    metrics_set_value(&g_event.hdr, MY_METRIC_VAL_U, 12);
    for (i = 32750; i < 32800; i++) {
        metrics_set_value(&g_event.hdr, MY_METRIC_VAL_SS16, -i);
        metrics_set_value(&g_event.hdr, MY_METRIC_VAL_SS32, -i);
    }

    /*
     * event state can be dumped via shell:
     *   select metrics
     *   list-events 1
     *   event-dump myev
     */

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }

    return 0;
}
