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

#include <string.h>

#include "stats/stats.h"

#include <stdio.h>

STATS_SECT_START(stats)
    STATS_SECT_ENTRY(num_registered)
STATS_SECT_END

STATS_SECT_DECL(stats) g_stats_stats;

STATS_NAME_START(stats)
    STATS_NAME(stats, num_registered)
STATS_NAME_END(stats)

STAILQ_HEAD(, stats_hdr) g_stats_registry =
    STAILQ_HEAD_INITIALIZER(g_stats_registry);

static uint8_t stats_module_inited;

int
stats_walk(struct stats_hdr *hdr, stats_walk_func_t walk_func, void *arg)
{
    char *name;
    char name_buf[12];
    uint16_t cur;
    uint16_t end;
    int ent_n;
    int len;
    int rc;
#ifdef STATS_NAME_ENABLE
    int i;
#endif

    cur = sizeof(*hdr);
    end = sizeof(*hdr) + (hdr->s_size * hdr->s_cnt);

    while (cur < end) {
        /*
         * Access and display the statistic name.  Pass that to the
         * walk function
         */
        name = NULL;
#ifdef STATS_NAME_ENABLE
        for (i = 0; i < hdr->s_map_cnt; ++i) {
            if (hdr->s_map[i].snm_off == cur) {
                name = hdr->s_map[i].snm_name;
                break;
            }
        }
#endif
        if (name == NULL) {
            ent_n = (cur - sizeof(*hdr)) / hdr->s_size;
            len = snprintf(name_buf, sizeof(name_buf), "s%d", ent_n);
            name_buf[len] = '\0';
            name = name_buf;
        }

        rc = walk_func(hdr, arg, name, cur);
        if (rc != 0) {
            goto err;
        }

        cur += hdr->s_size;
    }

    return (0);
err:
    return (rc);
}


int
stats_module_init(void)
{
    int rc;

    if (stats_module_inited) {
        return 0;
    }
    stats_module_inited = 1;

#ifdef SHELL_PRESENT
    rc = stats_shell_register();
    if (rc != 0) {
        goto err;
    }
#endif

#ifdef NEWTMGR_PRESENT
    rc = stats_nmgr_register_group();
    if (rc != 0) {
        goto err;
    }
#endif

    rc = stats_init(STATS_HDR(g_stats_stats),
                    STATS_SIZE_INIT_PARMS(g_stats_stats, STATS_SIZE_32),
                    STATS_NAME_INIT_PARMS(stats));
    if (rc != 0) {
        goto err;
    }

    rc = stats_register("stat", STATS_HDR(g_stats_stats));
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

/**
 * Uninitializes all statistic sections.  This is likely only useful for unit
 * tests that need to run in sequence.
 */
void
stats_module_reset(void)
{
    stats_module_inited = 0;

    STAILQ_INIT(&g_stats_registry);
}

int
stats_init(struct stats_hdr *shdr, uint8_t size, uint8_t cnt,
        struct stats_name_map *map, uint8_t map_cnt)
{
    memset((uint8_t *) shdr, 0, sizeof(*shdr) + (size * cnt));

    shdr->s_size = size;
    shdr->s_cnt = cnt;
#ifdef STATS_NAME_ENABLE
    shdr->s_map = map;
    shdr->s_map_cnt = map_cnt;
#endif

    return (0);
}

int
stats_group_walk(stats_group_walk_func_t walk_func, void *arg)
{
    struct stats_hdr *hdr;
    int rc;

    STAILQ_FOREACH(hdr, &g_stats_registry, s_next) {
        rc = walk_func(hdr, arg);
        if (rc != 0) {
            goto err;
        }
    }
    return (0);
err:
    return (rc);
}

struct stats_hdr *
stats_group_find(char *name)
{
    struct stats_hdr *cur;

    cur = NULL;
    STAILQ_FOREACH(cur, &g_stats_registry, s_next) {
        if (!strcmp(cur->s_name, name)) {
            break;
        }
    }

    return (cur);
}

int
stats_register(char *name, struct stats_hdr *shdr)
{
    struct stats_hdr *cur;
    int rc;

    /* Don't allow duplicate entries, return an error if this stat
     * is already registered.
     */
    STAILQ_FOREACH(cur, &g_stats_registry, s_next) {
        if (!strcmp(cur->s_name, name)) {
            rc = -1;
            goto err;
        }
    }

    shdr->s_name = name;

    STAILQ_INSERT_TAIL(&g_stats_registry, shdr, s_next);

    STATS_INC(g_stats_stats, num_registered);

    return (0);
err:
    return (rc);
}

/**
 * Initializes and registers the specified statistics section.
 */
int
stats_init_and_reg(struct stats_hdr *shdr, uint8_t size, uint8_t cnt,
                   struct stats_name_map *map, uint8_t map_cnt, char *name)
{
    int rc;

    rc = stats_init(shdr, size, cnt, map, map_cnt);
    if (rc != 0) {
        return rc;
    }

    rc = stats_register(name, shdr);
    if (rc != 0) {
        return rc;
    }

    return rc;
}
