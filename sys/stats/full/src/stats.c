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

#include "os/mynewt.h"
#include "stats/stats.h"

/**
 * Declare an example statistics section, which is fittingly, the number
 * statistics registered in the system.  There are two, largely duplicated,
 * statistics sections here, in order to provide the optional ability to
 * name statistics.
 *
 * STATS_SECT_START/END actually declare the statistics structure definition,
 * STATS_SECT_DECL() creates the structure declaration so you can declare
 * these statistics as a global structure, and STATS_NAME_START/END are how
 * you name the statistics themselves.
 *
 * Statistics entries can be declared as any of the following values, however,
 * all statistics in a given structure must be of the same size, and they are
 * all unsigned.
 *
 * - STATS_SECT_ENTRY(): default statistic entry, 32-bits.  This
 *   is the good stuff. Two factors to consider:
 *       - With 16-bit numbers, rollovers happen, frequently.  Whether its
 *       testing a pathological condition, or just a long time since you've
 *       collected a statistic: it really sucks to not have a crucial piece
 *       of information.
 *       - 64-bit numbers are wonderful things.  However, the gods did not see
 *       fit to bless us with unlimited memory.  64-bit statistics are useful
 *       when you want to store non-statistics in a statistics entry (i.e. time),
 *       because its convenient...
 *
 * - STATS_SECT_ENTRY16(): 16-bits.  Smaller statistics if you need to fit into
 *   specific RAM or code size numbers.
 *
 * - STATS_SECT_ENTRY32(): 32-bits, if you want to force it.
 *
 * - STATS_SECT_ENTRY64(): 64-bits.  Useful for storing chunks of data.
 *
 * Following the statics entry declaration is the statistic names declaration.
 * This is compiled out when STATS_NAME_ENABLE is set to 0.  This declaration
 * is const, and therefore can be located in .text, not .data.
 *
 * In cases where the system configuration variable STATS_NAME_ENABLE is set
 * to 1, the statistics names are stored and returned to both the console
 * and management APIs.  Whereas, when STATS_NAME_ENABLE = 0, these statistics
 * are numbered, s0, s1, etc.
 */
STATS_SECT_START(stats)
    STATS_SECT_ENTRY(num_registered)
STATS_SECT_END

STATS_SECT_DECL(stats) g_stats_stats;

STATS_NAME_START(stats)
    STATS_NAME(stats, num_registered)
STATS_NAME_END(stats)

STAILQ_HEAD(, stats_hdr) g_stats_registry =
    STAILQ_HEAD_INITIALIZER(g_stats_registry);


/**
 * Walk a specific statistic entry, and call walk_func with arg for
 * each field within that entry.
 *
 * Walk func takes the following parameters:
 *
 * - The header of the statistics section (stats_hdr)
 * - The user supplied argument
 * - The name of the statistic (if STATS_NAME_ENABLE = 0, this is
 *   ("s%d", n), where n is the number of the statistic in the structure.
 * - A pointer to the current entry.
 *
 * @return 0 on success, the return code of the walk_func on abort.
 *
 */
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
#if MYNEWT_VAL(STATS_NAMES)
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
#if MYNEWT_VAL(STATS_NAMES)
        /* The stats name map contains two elements, an offset into the
         * statistics entry structure, and the name corresponding with that
         * offset.  This annotation allows for naming only certain statistics,
         * and doesn't enforce ordering restrictions on the stats name map.
         */
        for (i = 0; i < hdr->s_map_cnt; ++i) {
            if (hdr->s_map[i].snm_off == cur) {
                name = hdr->s_map[i].snm_name;
                break;
            }
        }
#endif
        /* Do this check irrespective of whether MYNEWT_VALUE(STATS_NAMES)
         * is set.  Users may only partially name elements in the statistics
         * structure.
         */
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

        /* Statistics are variable sized, move forward either 16, 32 or 64
         * bits in the structure.
         */
        cur += hdr->s_size;
    }

    return (0);
err:
    return (rc);
}

/**
 * Initialize the stastics module.  Called before any of the statistics get
 * registered to initialize global structures, and register the default
 * statistics "stat."
 *
 * ASSERT's if it fails, as something is likely h0rked system wide.
 */
void
stats_module_init(void)
{
    int rc;

    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    STAILQ_INIT(&g_stats_registry);

#if MYNEWT_VAL(STATS_CLI)
    rc = stats_shell_register();
    SYSINIT_PANIC_ASSERT(rc == 0);
#endif

#if MYNEWT_VAL(STATS_NEWTMGR)
    rc = stats_nmgr_register_group();
    SYSINIT_PANIC_ASSERT(rc == 0);
#endif

    rc = stats_init(STATS_HDR(g_stats_stats),
                    STATS_SIZE_INIT_PARMS(g_stats_stats, STATS_SIZE_32),
                    STATS_NAME_INIT_PARMS(stats));
    SYSINIT_PANIC_ASSERT(rc == 0);

    rc = stats_register("stat", STATS_HDR(g_stats_stats));
    SYSINIT_PANIC_ASSERT(rc == 0);
}


/**
 * Initialize a statistics structure, pointed to by hdr.
 *
 * @param hdr The header of the statistics structure, contains things
 *            like statistic section name, size of statistics entries,
 *            number of statistics, etc.
 * @param size The size of the individual statistics elements, either
 *             2 (16-bits), 4 (32-bits) or 8 (64-bits).
 * @param cnt The number of elements in the statistics structure
 * @param map The mapping of statistics name to statistic entry
 * @param map_cnt The number of items in the statistics map
 *
 * @return 0 on success, non-zero error code on failure.
 */
int
stats_init(struct stats_hdr *shdr, uint8_t size, uint8_t cnt,
        const struct stats_name_map *map, uint8_t map_cnt)
{
    memset((uint8_t *) shdr+sizeof(*shdr), 0, size * cnt);

    shdr->s_size = size;
    shdr->s_cnt = cnt;
#if MYNEWT_VAL(STATS_NAMES)
    shdr->s_map = map;
    shdr->s_map_cnt = map_cnt;
#endif

    return (0);
}

/**
 * Walk the group of registered statistics and call walk_func() for
 * each element in the list.  This function _DOES NOT_ lock the statistics
 * list, and assumes that the list is not being changed by another task.
 * (assumption: all statistics are registered prior to OS start.)
 *
 * @param walk_func The walk function to call, with a statistics header
 *                  and arg.
 * @param arg The argument to call the walk function with.
 *
 * @return 0 on success, non-zero error code on failure
 */
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

/**
 * Find a statistics structure by name, this is not thread-safe.
 * (assumption: all statistics are registered prior ot OS start.)
 *
 * @param name The statistic structure name to find
 *
 * @return statistic structure if found, NULL if not found.
 */
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

/**
 * Register the statistics pointed to by shdr, with the name of "name."
 *
 * @param name The name of the statistic to register.  This name is guaranteed
 *             unique in the statistics map.  If already exists, this function
 *             will return an error.
 * @param shdr The statistics header to register into the statistic map under
 *             name.
 *
 * @return 0 on success, non-zero error code on failure.
 */
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
 *
 * @param shdr The statistics header to register
 * @param size The entry size of the statistics to register either 2 (16-bit),
 *             4 (32-bit) or 8 (64-bit).
 * @param cnt  The number of statistics entries in the statistics structure.
 * @param map  The map of statistics entry to statistics name, only used when
 *             MYNEWT_VAL(STATS_NAME) = 1.
 * @param map_cnt The number of elements in the statistics name map.
 * @param name The name of the statistics element to register with the system.
 *
 * @return 0 on success, non-zero error code on failure.
 */
int
stats_init_and_reg(struct stats_hdr *shdr, uint8_t size, uint8_t cnt,
                   const struct stats_name_map *map, uint8_t map_cnt,
                   char *name)
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

/**
 * Resets and zeroes the specified statistics section.
 *
 * @param shdr The statistics header to zero
 */
void
stats_reset(struct stats_hdr *hdr)
{
    uint16_t cur;
    uint16_t end;
    void *stat_val;

    cur = sizeof(*hdr);
    end = sizeof(*hdr) + (hdr->s_size * hdr->s_cnt);

    while (cur < end) {
        stat_val = (uint8_t*)hdr + cur;
        switch (hdr->s_size) {
            case sizeof(uint16_t):
                *(uint16_t *)stat_val = 0;
                break;
            case sizeof(uint32_t):
                *(unsigned long *)stat_val = 0;
                break;
            case sizeof(uint64_t):
                *(uint64_t *)stat_val = 0;
                break;
        }

        /*
         * Statistics are variable sized, move forward either 16, 32 or 64
         * bits in the structure.
         */
        cur += hdr->s_size;
    }
    return;
}
