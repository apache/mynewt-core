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
#ifndef __UTIL_STATS_H__
#define __UTIL_STATS_H__

#include <stddef.h>
#include <stdint.h>
#include "os/os.h"

#ifdef __cplusplus
extern "C" {
#endif

/** The stat group is periodically written to sys/config. */
#define STATS_HDR_F_PERSIST             0x01

struct stats_name_map {
    uint16_t snm_off;
    char *snm_name;
} __attribute__((packed));

struct stats_hdr {
    const char *s_name;
    uint8_t s_size;
    uint8_t s_cnt;
    uint16_t s_flags;
#if MYNEWT_VAL(STATS_NAMES)
    const struct stats_name_map *s_map;
    int s_map_cnt;
#endif
    STAILQ_ENTRY(stats_hdr) s_next;
};

/**
 * Header describing a persistent stat group.  A pointer to a regular
 * `stats_hdr` can be safely cast to a pointer to `stats_persisted_hdr` (and
 * vice-versa) if the `STATS_HDR_F_PERSIST` flag is set.
 */
struct stats_persisted_hdr {
    struct stats_hdr sp_hdr;
    struct os_callout sp_persist_timer;
    os_time_t sp_persist_delay;
};

#define STATS_SECT_DECL(__name)             \
    struct stats_ ## __name

#define STATS_SECT_START(__name)            \
STATS_SECT_DECL(__name) {                   \
    struct stats_hdr s_hdr;

#define STATS_SECT_END };

#define STATS_SECT_VAR(__var) \
    s##__var

#define STATS_HDR(__sectname) &(__sectname).s_hdr

#define STATS_SIZE_16 (sizeof(uint16_t))
#define STATS_SIZE_32 (sizeof(uint32_t))
#define STATS_SIZE_64 (sizeof(uint64_t))

#define STATS_SECT_ENTRY(__var) uint32_t STATS_SECT_VAR(__var);
#define STATS_SECT_ENTRY16(__var) uint16_t STATS_SECT_VAR(__var);
#define STATS_SECT_ENTRY32(__var) uint32_t STATS_SECT_VAR(__var);
#define STATS_SECT_ENTRY64(__var) uint64_t STATS_SECT_VAR(__var);

/**
 * @brief Resets all stats in the provided group to 0.
 *
 * NOTE: This must only be used with non-persistent stat groups.
 */
#define STATS_RESET(__sectvarname)                                      \
    memset((uint8_t *)&__sectvarname + sizeof(struct stats_hdr), 0,     \
           sizeof(__sectvarname) - sizeof(struct stats_hdr))

#define STATS_SIZE_INIT_PARMS(__sectvarname, __size)                        \
    (__size),                                                               \
    ((sizeof (__sectvarname)) - sizeof (__sectvarname.s_hdr)) / (__size)

#define STATS_GET(__sectvarname, __var)             \
    ((__sectvarname).STATS_SECT_VAR(__var))

#define STATS_SET_RAW(__sectvarname, __var, __val)  \
    (STATS_GET(__sectvarname, __var) = (__val))

#define STATS_SET(__sectvarname, __var, __val) do               \
{                                                               \
    STATS_SET_RAW(__sectvarname, __var, __val);                 \
    STATS_PERSIST_SCHED((struct stats_hdr *)&__sectvarname);    \
} while (0)

/**
 * @brief Adjusts a stat's in-RAM value by the specified delta.
 *
 * For non-persistent stats, this is more efficient than `STATS_INCN()`.  This
 * must only be used with non-persistent stats; for persistent stats the
 * behavior is undefined.
 *
 * @param __sectvarname         The name of the stat group containing the stat
 *                                  to modify.
 * @param __var                 The name of the individual stat to modify.
 * @param __n                   The amount to add to the specified stat.
 */
#define STATS_INCN_RAW(__sectvarname, __var, __n)   \
    (STATS_SET_RAW(__sectvarname, __var,            \
                   STATS_GET(__sectvarname, __var) + (__n))

/**
 * @brief Increments a stat's in-RAM value.
 *
 * For non-persistent stats, this is more efficient than `STATS_INCN()`.  This
 * must only be used with non-persistent stats; for persistent stats the
 * behavior is undefined.
 *
 * @param __sectvarname         The name of the stat group containing the stat
 *                                  to modify.
 * @param __var                 The name of the individual stat to modify.
 */
#define STATS_INC_RAW(__sectvarname, __var)       \
    STATS_INCN_RAW(__sectvarname, __var, 1)

/**
 * @brief Adjusts a stat's value by the specified delta.
 *
 * If the specified stat group is persistent, this also schedules the group to
 * be flushed to disk.
 *
 * @param __sectvarname         The name of the stat group containing the stat
 *                                  to modify.
 * @param __var                 The name of the individual stat to modify.
 * @param __n                   The amount to add to the specified stat.
 */
#define STATS_INCN(__sectvarname, __var, __n)       \
    STATS_SET(__sectvarname, __var, STATS_GET(__sectvarname, __var) + (__n))

/**
 * @brief Increments a stat's value.
 *
 * If the specified stat group is persistent, this also schedules the group to
 * be flushed to disk.
 *
 * @param __sectvarname         The name of the stat group containing the stat
 *                                  to modify.
 * @param __var                 The name of the individual stat to modify.
 */
#define STATS_INC(__sectvarname, __var)             \
    STATS_INCN(__sectvarname, __var, 1)

#define STATS_CLEAR(__sectvarname, __var)           \
    STATS_SET(__sectvarname, __var, 0)

#if MYNEWT_VAL(STATS_NAMES)

#define STATS_NAME_MAP_NAME(__sectname) g_stats_map_ ## __sectname

#define STATS_NAME_START(__sectname)                                        \
const struct stats_name_map STATS_NAME_MAP_NAME(__sectname)[] = {

#define STATS_NAME(__sectname, __entry)                                     \
    { offsetof(STATS_SECT_DECL(__sectname), STATS_SECT_VAR(__entry)),       \
      #__entry },

#define STATS_NAME_END(__sectname)                                          \
};

#define STATS_NAME_INIT_PARMS(__name)                                       \
    &(STATS_NAME_MAP_NAME(__name)[0]),                                      \
    (sizeof(STATS_NAME_MAP_NAME(__name)) / sizeof(struct stats_name_map))

#else /* MYNEWT_VAL(STATS_NAME) */

#define STATS_NAME_START(__name)
#define STATS_NAME(__name, __entry)
#define STATS_NAME_END(__name)
#define STATS_NAME_INIT_PARMS(__name) NULL, 0

#endif /* MYNEWT_VAL(STATS_NAME) */

int stats_init(struct stats_hdr *shdr, uint8_t size, uint8_t cnt,
    const struct stats_name_map *map, uint8_t map_cnt);
int stats_register(const char *name, struct stats_hdr *shdr);
int stats_init_and_reg(struct stats_hdr *shdr, uint8_t size, uint8_t cnt,
                       const struct stats_name_map *map, uint8_t map_cnt,
                       const char *name);
void stats_reset(struct stats_hdr *shdr);

typedef int (*stats_walk_func_t)(struct stats_hdr *, void *, char *,
        uint16_t);
int stats_walk(struct stats_hdr *, stats_walk_func_t, void *);

typedef int (*stats_group_walk_func_t)(struct stats_hdr *, void *);
int stats_group_walk(stats_group_walk_func_t, void *);

struct stats_hdr *stats_group_find(const char *name);

/* Private */
#if MYNEWT_VAL(STATS_NEWTMGR)
int stats_nmgr_register_group(void);
#endif
#if MYNEWT_VAL(STATS_CLI)
int stats_shell_register(void);
#endif

#if MYNEWT_VAL(STATS_PERSIST)

/**
 * @brief Starts the definition of a peristed stat group.
 *
 * o Follow with invocations of the `STATS_SECT_ENTRY[...]` macros to define
 *   individual stats.
 * o Use `STATS_SECT_END` to complete the group definition.
 */
#define STATS_PERSISTED_SECT_START(__name)  \
STATS_SECT_DECL(__name) {                   \
    struct stats_persisted_hdr s_hdr;

#define STATS_PERSISTED_HDR(__sectname) &(__sectname).s_hdr.sp_hdr

/**
 * @brief (private) Starts the provided stat group's persistence timer if it is
 * a persistent group.
 *
 * This should be used whenever a statistic's value changes.  This is a no-op
 * for non-persistent stat groups.
 */
#define STATS_PERSIST_SCHED(hdrp_) stats_persist_sched(hdrp_)

/**
 * @brief (private) Starts the provided stat group's persistence timer.
 *
 * This should be used whenever a statistic's value changes.  This is a no-op
 * for non-persistent stat groups.
 */
void stats_persist_sched(struct stats_hdr *hdr);

/**
 * @brief Flushes to disk all persisted stat groups with pending writes.
 *
 * @return                      0 on success; nonzero on failure.
 */
int stats_persist_flush(void);

/**
 * @brief Initializes a persistent stat group.
 *
 * This function must be called before any other stats API functions are
 * applied to the specified stat group.  This is typically done during system
 * startup.
 *
 * Example usage:
 *     STATS_PERSISTED_SECT_START(my_stats)
 *         STATS_SECT_ENTRY(stat1)
 *     STATS_SECT_END(my_stats)
 *
 *     STATS_NAME_START(my_stats)
 *         STATS_SECT_ENTRY(my_stats, stat1)
 *     STATS_NAME_END(my_stats)
 *
 *     rc = stats_persist_init(STATS_PERSISTED_HDR(my_stats),
 *                             STATS_SIZE_INIT_PARMS(my_stats, STATS_SIZE_32),
 *                             STATS_NAME_INIT_PARMS(my_stats),
 *                             1 * OS_TICKS_PER_SEC);   // One second.
 * 
 *
 * @param hdr                   The header of the stat group to initialize.
 *                                  Use the `STATS_PERSISTED_HDR()` macro to
 *                                  generate this argument.
 * @param size                  The size, in bytes, of each statistic.  Use the
 *                                  `STATS_SIZE_INIT_PARMS()` macro to generate
 *                                  this and the `cnt` arguments.
 * @param cnt                   The number of statistics in the group.  Use the
 *                                  `STATS_SIZE_INIT_PARMS()` macro to generate
 *                                  this and the `size` arguments.
 * @param map                   Maps each stat to a human-readable name.  Use
 *                                  the `STATS_NAME_INIT_PARMS()` macro to
 *                                  generate this and the `map_cnt` arguments.
 * @param map_cnt               The number of names in `map.  Use the the
 *                                  `STATS_NAME_INIT_PARMS()` macro to generate
 *                                  this and the `map` arguments.
 * @param persist_delay         The delay, in OS ticks, before the stat group
 *                                  is flushed to disk after modification.
 *
 * @return                      0 on success; nonzero on failure.
 */
int stats_persist_init(struct stats_hdr *hdr, uint8_t size,
                       uint8_t cnt, const struct stats_name_map *map,
                       uint8_t map_cnt, os_time_t persist_delay);

#else /* MYNEWT_VAL(STATS_PERSIST) */

#define STATS_PERSISTED_SECT_START(__name) \
    static_assert(0, "You must enable STATS_PERSIST to use persistent stats");

#define STATS_PERSIST_SCHED(hdrp_)

#endif /* MYNEWT_VAL(STATS_PERSIST) */

#ifdef __cplusplus
}
#endif

#endif /* __UTIL_STATS_H__ */
