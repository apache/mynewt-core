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

/* Source code is only included if the shell library is enabled.  Otherwise
 * this file is compiled out for code size.
 */
#ifdef SHELL_PRESENT

#include <shell/shell.h>
#include <console/console.h>

uint8_t stats_shell_registered;
struct shell_cmd shell_stats_cmd;

static int 
stats_shell_display_entry(struct stats_hdr *hdr, void *arg, char *name,
        uint8_t *stat)
{
    switch (hdr->s_size) {
        case sizeof(uint16_t):
            console_printf("%s: %u\n", name, *(uint16_t *) stat);
            break;
        case sizeof(uint32_t):
            console_printf("%s: %lu\n", name, *(unsigned long *) stat);
            break;
        case sizeof(uint64_t):
            console_printf("%s: %llu\n", name, *(uint64_t *) stat);
            break;
        default:
            console_printf("Unknown stat size for %s %u\n", name, 
                    hdr->s_size);
            break;
    }

    return (0);
}

static int 
stats_shell_display_group(struct stats_hdr *hdr, void *arg)
{
    console_printf("\t%s\n", hdr->s_name);
    return (0);
}

static int 
shell_stats_display(int argc, char **argv)
{
    struct stats_hdr *hdr;
    char *name;
    int rc;

    name = argv[1];
    if (name == NULL || !strcmp(name, "")) {
        console_printf("Must specify a statistic name to dump, "
                "possible names are:\n");
        stats_group_walk(stats_shell_display_group, NULL);
        rc = OS_EINVAL;
        goto err;
    }

    hdr = stats_group_find(name);
    if (!hdr) {
        console_printf("Could not find statistic group %s\n", name);
        rc = OS_EINVAL;
        goto err;
    }

    rc = stats_walk(hdr, stats_shell_display_entry, NULL);
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
        shell_cmd_register(&shell_stats_cmd, "stat", shell_stats_display);
    }

    return (0);
}


#endif /* SHELL_PRESENT */
