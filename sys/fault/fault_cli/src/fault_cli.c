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

#include <inttypes.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>
#include "os/mynewt.h"
#include "console/console.h"
#include "parse/parse.h"
#include "shell/shell.h"
#include "fault/fault.h"

static int fault_cli_cmd_fn(int argc, char **argv);

static const struct shell_cmd_help fault_cli_help = {
    .summary = "Fault management",
    .usage = 
        "\n"
        "fault chronls\n"
        "    Lists the chronic fault failure counts.\n"
        "fault chronset <fault-domain> <count>\n"
        "    Sets and persists the specified chronic fault failure count for\n"
        "    the given domain.\n"
        "fault chronsetr <fault-domain> <count>\n"
        "    Performs a chronset and then immediately reboots the device.\n"
        "fault chronclr\n"
        "    Sets all chronic fault failure counts to 0 and persists the\n"
        "    counts.\n"
        "fault fatalfail <fault-domain> [uint-param]\n"
        "    Simulates a fatal error for the given domain.\n"
        "fault fatalgood <fault-domain> [uint-param]\n"
        "    Simulates a fatal success for the given domain.\n",
};

static const struct shell_cmd fault_cli_cmd = {
    .sc_cmd = "fault",
    .sc_cmd_func = fault_cli_cmd_fn,
    .help = &fault_cli_help,
};

static int
fault_cli_parse_domain(const char *arg)
{
    int dom;
    const char *name;
    int rc;

    /* First, try to parse a numeric ID. */
    dom = parse_ull_bounds(arg, 0, MYNEWT_VAL(FAULT_MAX_DOMAINS), &rc);
    if (rc == 0) {
        return dom;
    }

    /* Otherwise, see if the user typed the name of a domain. */
    for (dom = 0; dom < MYNEWT_VAL(FAULT_MAX_DOMAINS); dom++) {
        name = fault_domain_name(dom);
        if (name != NULL && strcasecmp(arg, name) == 0) {
            return dom;
        }
    }

    return -1;
}

static int
fault_cli_chronls(int argc, char **argv)
{
    const char *name;
    uint8_t count;
    int rc;
    int i;

    for (i = 0; i < MYNEWT_VAL(FAULT_MAX_DOMAINS); i++) {
        rc = fault_get_chronic_count(i, &count);
        if (rc == 0) {
            name = fault_domain_name(i);
            if (name == NULL) {
                name = "";
            }
            console_printf("(%d) %s: %d\n", i, name, count);
        }
    }

    return 0;
}

static int
fault_cli_chronset(int argc, char **argv)
{
    uint8_t count;
    int dom;
    int rc;

    if (argc < 2) {
        return SYS_EINVAL;
    }

    dom = fault_cli_parse_domain(argv[0]);
    if (dom == -1) {
        console_printf("invalid domain string\n");
        return SYS_EINVAL;
    }

    count = parse_ull_bounds(argv[1], 0, UINT8_MAX, &rc);
    if (rc != 0) {
        console_printf("invalid failure count\n");
        return rc;
    }

    rc = fault_set_chronic_count(dom, count);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

static int
fault_cli_chronsetr(int argc, char **argv)
{
    int rc;

    rc = fault_cli_chronset(argc, argv);
    if (rc != 0) {
        return rc;
    }

    os_reboot(0);

    return 0;
}

static int
fault_cli_chronclr(int argc, char **argv)
{
    int dom;
    int rc;

    for (dom = 0; dom < MYNEWT_VAL(FAULT_MAX_DOMAINS); dom++) {
        if (fault_domain_is_registered(dom)) {
            rc = fault_set_chronic_count(dom, 0);
            if (rc != 0) {
                return rc;
            }
        }
    }

    return 0;
}

static int
fault_cli_fatal_gen(int argc, char **argv, bool is_failure)
{
    uintptr_t int_arg;
    int dom;
    int rc;

    if (argc < 1) {
        return SYS_EINVAL;
    }

    dom = fault_cli_parse_domain(argv[0]);
    if (dom == -1) {
        console_printf("invalid domain string\n");
        return SYS_EINVAL;
    }

    if (argc >= 2) {
        int_arg = parse_ull_bounds(argv[1], 0, UINT_MAX, &rc);
        if (rc != 0)
        {
            console_printf("invalid fault argument (`arg`)\n");
            return rc;
        }
    } else {
        int_arg = 0;
    }

    fault_fatal(dom, (void *)int_arg, is_failure);

    return 0;
}

static int
fault_cli_fatalfail(int argc, char **argv)
{
    return fault_cli_fatal_gen(argc, argv, true);
}

static int
fault_cli_fatalgood(int argc, char **argv)
{
    return fault_cli_fatal_gen(argc, argv, false);
}

static int
fault_cli_cmd_fn(int argc, char **argv)
{
    /* Shift initial "fault" argument. */
    argc--;
    argv++;

    if (argc < 1) {
        return SYS_EINVAL;
    }

    if (strcmp(argv[0], "chronls") == 0) {
        return fault_cli_chronls(argc - 1, argv + 1);
    }

    if (strcmp(argv[0], "chronset") == 0) {
        return fault_cli_chronset(argc - 1, argv + 1);
    }

    if (strcmp(argv[0], "chronsetr") == 0) {
        return fault_cli_chronsetr(argc - 1, argv + 1);
    }

    if (strcmp(argv[0], "chronclr") == 0) {
        return fault_cli_chronclr(argc - 1, argv + 1);
    }

    if (strcmp(argv[0], "fatalfail") == 0) {
        return fault_cli_fatalfail(argc - 1, argv + 1);
    }

    if (strcmp(argv[0], "fatalgood") == 0) {
        return fault_cli_fatalgood(argc - 1, argv + 1);
    }

    return SYS_EINVAL;
}

void
fault_cli_init(void)
{
    int rc;

    rc = shell_cmd_register(&fault_cli_cmd);
    SYSINIT_PANIC_ASSERT(rc == 0);
}
