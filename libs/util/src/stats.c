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

#include "util/stats.h" 

#ifdef SHELL_PRESENT
#include "shell/shell.h"
#include "console/console.h"
#endif

#include <stdio.h>

STATS_SECT_START(stats)
    STATS_SECT_ENTRY(num_registered)
STATS_SECT_END(stats)

STATS_NAME_START(stats)
    STATS_NAME(stats, num_registered)
STATS_NAME_END(stats)

STAILQ_HEAD(, stats_hdr) g_stats_registry = 
    STAILQ_HEAD_INITIALIZER(g_stats_registry);

#ifdef SHELL_PRESENT
uint8_t stats_shell_registered;
struct shell_cmd shell_stats_cmd;
#endif

#ifdef SHELL_PRESENT 

static void 
shell_stats_display_entry(struct stats_hdr *hdr, uint8_t *ptr)
{
    char *name;
    char buf[12];
    int ent_n;
    int len;
#ifdef STATS_NAME_ENABLE
    int i;
#endif

    name = NULL;

#ifdef STATS_NAME_ENABLE
    for (i = 0; i < hdr->s_map_cnt; i++) {
        if (hdr->s_map[i].snm_off == ptr) {
            name = hdr->s_map[i].snm_name;
            break;
        }
    }
#endif 

    if (name == NULL) {
        ent_n = (ptr - ((uint8_t *) hdr + sizeof(*hdr))) / hdr->s_size;

        len = snprintf(buf, sizeof(buf), "s%d", ent_n);
        buf[len] = 0;
        name = buf;
    }

    switch (hdr->s_size) {
        case sizeof(uint16_t):
            console_printf("%s: %u\n", name, *(uint16_t *) ptr);
            break;
        case sizeof(uint32_t):
            console_printf("%s: %u\n", name, *(uint32_t *) ptr);
            break;
        case sizeof(uint64_t):
            console_printf("%s: %llu\n", name, *(uint64_t *) ptr);
            break;
        default:
            console_printf("Unknown stat size for %s %u\n", name, 
                    hdr->s_size);
            break;
    }
}

static int 
shell_stats_display(int argc, char **argv)
{
    struct stats_hdr *hdr;
    char *name;
    uint8_t *cur;
    uint8_t *end;

    name = argv[1];
    if (name == NULL || !strcmp(name, "")) {
        console_printf("Must specify a statistic name to dump, "
                "possible names are:\n");
        STAILQ_FOREACH(hdr, &g_stats_registry, s_next) {
            console_printf("\t%s\n", hdr->s_name);
        }
        goto done;
    }


    hdr = stats_find(name);
    if (!hdr) {
        console_printf("Could not find statistic %s\n", name);
        goto done;
    }

    cur = (uint8_t *) hdr + sizeof(*hdr);
    end = (uint8_t *) hdr + sizeof(*hdr) + (hdr->s_size * hdr->s_cnt);
    while (cur < end) {
        shell_stats_display_entry(hdr, (uint8_t *) cur);
        cur += hdr->s_size;
    }

done:
    return (0);
}

#endif

int 
stats_module_init(void)
{
    int rc;
#ifdef SHELL_PRESENT
    if (!stats_shell_registered) {
        stats_shell_registered = 1;
        shell_cmd_register(&shell_stats_cmd, "stat", shell_stats_display);
    }
#endif

    rc = stats_init(STATS_HDR(stats), STATS_SIZE_INIT_PARMS(stats, STATS_SIZE_32), 
            STATS_NAME_INIT_PARMS(stats));
    if (rc != 0) {
        goto err;
    }
    
    rc = stats_register("stat", STATS_HDR(stats));
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
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

    STATS_INC(stats, num_registered);

    return (0);
err:
    return (rc);
}

struct stats_hdr * 
stats_find(char *name)
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

