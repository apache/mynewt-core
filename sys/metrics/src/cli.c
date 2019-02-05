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

#if MYNEWT_VAL(METRICS_CLI)

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "shell/shell.h"
#include "console/console.h"
#include "metrics/metrics.h"
#include "tinycbor/cbor.h"
#include "tinycbor/cbor_mbuf_reader.h"

static STAILQ_HEAD(, metrics_event_hdr) g_event_list =
                                        STAILQ_HEAD_INITIALIZER(g_event_list);

static struct metrics_event_hdr *
find_event_by_name(const char *name)
{
    struct metrics_event_hdr *hdr;

    STAILQ_FOREACH(hdr, &g_event_list, next) {
        if (!strcmp(hdr->name, name)) {
            break;
        }
    }

    return hdr;
}

static int
find_metric_by_name(struct metrics_event_hdr *hdr, const char *name)
{
    int i;

    for (i = 0; i < hdr->count; i++) {
        if (!strcmp(hdr->defs[i].name, name)) {
            return i;
        }
    }

    return -1;
}

static const char *
metric_type_str(uint8_t type)
{
    switch (type) {
    case METRICS_TYPE_SINGLE_U:
        return "unsigned";
    case METRICS_TYPE_SINGLE_S:
        return "signed";
    case METRICS_TYPE_SERIES_U8:
        return "unsigned8-series";
    case METRICS_TYPE_SERIES_S8:
        return "signed8-series";
    case METRICS_TYPE_SERIES_U16:
        return "unsigned16-series";
    case METRICS_TYPE_SERIES_S16:
        return "signed16-series";
    case METRICS_TYPE_SERIES_U32:
        return "unsigned32-series";
    case METRICS_TYPE_SERIES_S32:
        return "signed32-series";
    }

    return "<unknown>";
}

static void
print_event_metrics(struct metrics_event_hdr *hdr)
{
    int i;

    for (i = 0; i < hdr->count; i++) {
        console_printf("  %d = %s (%s, %d)\n", i, hdr->defs[i].name,
                                        metric_type_str(hdr->defs[i].type),
                                        !!(hdr->enabled & (1 << i)));
    }
}

static int
cmd_list_events(int argc, char **argv)
{
    struct metrics_event_hdr *hdr;
    bool full = false;

    if (argc > 1) {
        full = atoi(argv[1]);
    }

    STAILQ_FOREACH(hdr, &g_event_list, next) {
        console_printf("%s\n", hdr->name);
        if (full) {
            print_event_metrics(hdr);
        }
    }

    return 0;
}

static int
cmd_list_event_metrics(int argc, char **argv)
{
    struct metrics_event_hdr *hdr;

    if (argc < 2) {
        console_printf("Event name not specified\n");
        return -1;
    }

    STAILQ_FOREACH(hdr, &g_event_list, next) {
        if (!strcmp(hdr->name, argv[1])) {
            print_event_metrics(hdr);
            return 0;
        }
    }

    console_printf("Event '%s' not found\n", argv[1]);

    return -1;
}

static int
cmd_metric_set(int argc, char **argv)
{
    struct metrics_event_hdr *hdr;
    int i;

    if (argc < 4) {
        console_printf("Event and/or metric name not specified\n");
        return -1;
    }

    hdr = find_event_by_name(argv[1]);
    if (!hdr) {
        console_printf("Event '%s' not found\n", argv[1]);
        return -1;
    }

    i = find_metric_by_name(hdr, argv[2]);
    if (i < 0) {
        console_printf("Metric '%s' not found\n", argv[2]);
        return -1;
    }

    metrics_set_state(hdr, i, atoi(argv[3]));

    return -1;
}

static int
cmd_event_dump(int argc, char **argv)
{
    struct cbor_mbuf_reader reader;
    struct CborParser parser;
    struct CborValue value;
    struct metrics_event_hdr *hdr;
    struct os_mbuf *om;

    if (argc < 2) {
        console_printf("Event name not specified\n");
        return -1;
    }

    hdr = find_event_by_name(argv[1]);
    if (!hdr) {
        console_printf("Event '%s' not found\n", argv[1]);
        return -1;
    }

    /* We use msys so serializing event to CBOR will be non-destructive */
    om = os_msys_get_pkthdr(50, 0);
    metrics_event_to_cbor(hdr, om);
    cbor_mbuf_reader_init(&reader, om, 0);
    cbor_parser_init(&reader.r, 0, &parser, &value);
    cbor_value_to_pretty(stdout, &value);
    os_mbuf_free_chain(om);

    console_printf("\n");

    return 0;
}

static int
cmd_event_end(int argc, char **argv)
{
    struct metrics_event_hdr *hdr;

    if (argc < 2) {
        console_printf("Event name not specified\n");
        return -1;
    }

    hdr = find_event_by_name(argv[1]);
    if (!hdr) {
        console_printf("Event '%s' not found\n", argv[1]);
        return -1;
    }

    metrics_event_end(hdr);

    return 0;
}

static const struct shell_cmd metrics_commands[] = {
    {
        .sc_cmd = "list-events",
        .sc_cmd_func = cmd_list_events,
    },
    {
        .sc_cmd = "list-event-metrics",
        .sc_cmd_func = cmd_list_event_metrics,
    },
    {
        .sc_cmd = "metric-set",
        .sc_cmd_func = cmd_metric_set,
    },
    {
        .sc_cmd = "event-dump",
        .sc_cmd_func = cmd_event_dump,
    },
    {
        .sc_cmd = "event-end",
        .sc_cmd_func = cmd_event_end,
    },
    { },
};

int
metrics_cli_register_event(struct metrics_event_hdr *hdr)
{
    STAILQ_INSERT_TAIL(&g_event_list, hdr, next);

    return 0;
}

int
metrics_cli_init(void)
{
    shell_register("metrics", metrics_commands);

    return 0;
}
#endif
