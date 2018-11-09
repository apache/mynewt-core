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

#include "os/mynewt.h"

#if MYNEWT_VAL(STATS_PERSIST)

#include <assert.h>
#include "stats/stats.h"
#include "stats_priv.h"

static void
stats_persist_timer_exp(struct os_event *ev)
{
    const struct stats_hdr *hdr;
    int rc;

    hdr = ev->ev_arg;

    rc = stats_conf_save_group(hdr);
    if (rc != 0) {
        /* XXX: Trigger a system fault if configured to (requres fault feature
         * to be merged).
         */
    }
}

void
stats_persist_sched(struct stats_hdr *hdr)
{
    struct stats_persisted_hdr *sphdr;
    int rc;

    if (!(hdr->s_flags & STATS_HDR_F_PERSIST)) {
        return;
    }

    sphdr = (void *)hdr;

    if (!os_callout_queued(&sphdr->sp_persist_timer)) {
        rc = os_callout_reset(&sphdr->sp_persist_timer,
                              sphdr->sp_persist_delay);
        assert(rc == 0);
    }
}

/**
 * Passed to `stats_group_walk`; flushes the specified stat group to disk if a
 * write is pending.
 */
static int
stats_persist_flush_walk(struct stats_hdr *hdr, void *arg)
{
    struct stats_persisted_hdr *sphdr;

    if (!(hdr->s_flags & STATS_HDR_F_PERSIST)) {
        return 0;
    }

    sphdr = (void *)hdr;
    if (!os_callout_queued(&sphdr->sp_persist_timer)) {
        return 0;
    }

    os_callout_stop(&sphdr->sp_persist_timer);
    return stats_conf_save_group(hdr);
}

int
stats_persist_flush(void)
{
    return stats_group_walk(stats_persist_flush_walk, NULL);
}

/**
 * Called on system shutdown.  Flushes to disk all persisted stat groups with
 * pending writes.
 */
int
stats_persist_sysdown(int reason)
{
    stats_persist_flush();
    return 0;
}

int
stats_persist_init(struct stats_hdr *hdr, uint8_t size,
        uint8_t cnt, const struct stats_name_map *map, uint8_t map_cnt,
        os_time_t persist_delay)
{
    struct stats_persisted_hdr *sphdr;
    int rc;

    rc = stats_init(hdr, size, cnt, map, map_cnt);
    if (rc != 0) {
        return rc;
    }
    hdr->s_flags |= STATS_HDR_F_PERSIST;

    sphdr = (void *)hdr;

    sphdr->sp_persist_delay = persist_delay;
    os_callout_init(&sphdr->sp_persist_timer, os_eventq_dflt_get(),
            stats_persist_timer_exp, hdr);

    return 0;
}

#endif
