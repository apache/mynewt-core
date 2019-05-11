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

/* Source code is only included if the shell library is enabled.  Otherwise
 * this file is compiled out for code size.
 */
#if MYNEWT_VAL(STATS_CLI)

#include <string.h>
#include "shell/shell.h"
#include "streamer/streamer.h"
#include "stats/stats.h"

static int shell_stats_display(const struct shell_cmd *cmd,
                               int argc, char **argv,
                               struct streamer *streamer);

static struct shell_cmd shell_stats_cmd =
    SHELL_CMD_EXT("stat", shell_stats_display, NULL);

uint8_t stats_shell_registered;

static int 
stats_shell_display_entry(struct stats_hdr *hdr, void *arg, char *name,
        uint16_t stat_off)
{
    struct streamer *streamer;
    void *stat_val;

    streamer = arg;

    stat_val = (uint8_t *)hdr + stat_off;
    switch (hdr->s_size) {
        case sizeof(uint16_t):
            streamer_printf(streamer, "%s: %u\n", name,
                    *(uint16_t *) stat_val);
            break;
        case sizeof(uint32_t):
            streamer_printf(streamer, "%s: %lu\n", name,
                    *(unsigned long *) stat_val);
            break;
        case sizeof(uint64_t):
            streamer_printf(streamer, "%s: %llu\n", name,
                    *(uint64_t *) stat_val);
            break;
        default:
            streamer_printf(streamer, "Unknown stat size for %s %u\n", name, 
                    hdr->s_size);
            break;
    }

    return (0);
}

static int 
stats_shell_display_group(struct stats_hdr *hdr, void *arg)
{
    struct streamer *streamer;

    streamer = arg;
    streamer_printf(streamer, "\t%s\n", hdr->s_name);
    return (0);
}

static int
shell_stats_display(const struct shell_cmd *cmd, int argc, char **argv,
                    struct streamer *streamer)
{
    struct stats_hdr *hdr;
    char *name;
    int rc;

    name = argv[1];
    if (name == NULL || !strcmp(name, "")) {
        streamer_printf(streamer, "Must specify a statistic name to dump, "
                "possible names are:\n");
        stats_group_walk(stats_shell_display_group, streamer);
        rc = OS_EINVAL;
        goto err;
    }

    hdr = stats_group_find(name);
    if (!hdr) {
        streamer_printf(streamer, "Could not find statistic group %s\n", name);
        rc = OS_EINVAL;
        goto err;
    }

    rc = stats_walk(hdr, stats_shell_display_entry, streamer);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}


int 
stats_shell_register(void)
{
    if (!stats_shell_registered) {
        stats_shell_registered = 1;
        shell_cmd_register(&shell_stats_cmd);
    }

    return (0);
}


#endif
