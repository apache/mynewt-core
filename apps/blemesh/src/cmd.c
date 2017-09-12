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

#include "syscfg/syscfg.h"

#include <inttypes.h>

#include "console/console.h"
#include "shell/shell.h"

#include "parse.h"
#include "blemesh.h"

#define MESH_MODULE "mesh"


static int
cmd_relay_set(int argc, char **argv)
{
    uint8_t enable;
    int rc;

    rc = parse_arg_all(argc - 1, argv + 1);
    if (rc != 0) {
        return rc;
    }

    enable = parse_arg_uint8("enable", &rc);
    if (rc != 0) {
        console_printf("invalid 'enable' parameter\n");
        return rc;
    }

    rc = blemesh_cfg_relay_set(enable);
    if (rc != 0) {
        console_printf("Config relay set failed\n");
        return rc;
    }

    console_printf("Config relay set successful\n");
    return rc;
}

static const struct shell_param relay_set_params[] = {
    {"enable", "usage: =<UINT8>"},
    {NULL, NULL}
};

static const struct shell_cmd_help relay_set_help = {
    .summary = "set relay configuration",
    .usage = NULL,
    .params = relay_set_params,
};

static const struct shell_cmd mesh_commands[] = {
    {
        .sc_cmd = "relay-set",
        .sc_cmd_func = cmd_relay_set,
        .help = &relay_set_help,
    },
    { NULL, NULL, NULL },
};


void
cmd_init(void)
{
    shell_register(MESH_MODULE, mesh_commands);
    shell_register_default_module(MESH_MODULE);
}
