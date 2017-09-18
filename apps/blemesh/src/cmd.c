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
#include "mesh/mesh.h"

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

#define MSG_PAYLOAD_SIZE 120

static int
cmd_send_msg(int argc, char **argv)
{
    uint8_t payload_buf[MSG_PAYLOAD_SIZE];
    int payload_size;
    uint8_t ttl;
    uint16_t appkey_index;
    uint16_t src_addr, dst_addr;
    int rc;

    rc = parse_arg_all(argc - 1, argv + 1);
    if (rc != 0) {
        return rc;
    }

    ttl = parse_arg_uint8_dflt("ttl", 0, &rc);
    if (rc != 0) {
        console_printf("invalid 'ttl' parameter\n");
        return rc;
    }

    appkey_index = parse_arg_uint16_dflt("appkey_index", BT_MESH_KEY_DEV, &rc);
    if (rc != 0) {
        console_printf("invalid 'appkey_index' parameter\n");
        return rc;
    }

    src_addr = parse_arg_uint16("src", &rc);
    if (rc != 0) {
        console_printf("invalid 'src' parameter\n");
        return rc;
    }

    dst_addr = parse_arg_uint16("dst", &rc);
    if (rc != 0) {
        console_printf("invalid 'dst' parameter\n");
        return rc;
    }

    rc = parse_arg_byte_stream("payload", sizeof payload_buf,
                               payload_buf, &payload_size);
    if (rc != 0) {
        console_printf("invalid 'payload' parameter\n");
        return rc;
    }

    rc = blemesh_send_msg(ttl, appkey_index, src_addr,
                          dst_addr, payload_buf, payload_size);
    if (rc != 0) {
        console_printf("Message send failed\n");
        return rc;
    }

    console_printf("Message send successful\n");
    return rc;
}

static const struct shell_param send_msg_params[] = {
    {"ttl", "usage: =[UINT8], default: 0"},
    {"appkey_index", "usage: =[UINT16], default: 0xfffe"},
    {"src", "usage: =<UINT16>"},
    {"dst", "usage: =<UINT16>"},
    {"payload", "usage: =<XX:XX:XX..>"},
    {NULL, NULL}
};

static const struct shell_cmd_help send_msg_help = {
    .summary = "send message",
    .usage = NULL,
    .params = send_msg_params,
};

static int
cmd_iv_update(int argc, char **argv)
{
    int rc;
    uint32_t index;
    bool enable, update;


    rc = parse_arg_all(argc - 1, argv + 1);
    if (rc != 0) {
        return rc;
    }

    enable = parse_arg_bool("enable", &rc);
    if (rc != 0) {
        console_printf("invalid 'enable' parameter\n");
        return rc;
    }

    index = parse_arg_uint32_dflt("index", 0, &rc);
    if (rc != 0) {
        console_printf("invalid 'index' parameter\n");
        return rc;
    }

    update = parse_arg_bool_dflt("update", 0, &rc);
    if (rc != 0) {
        console_printf("invalid 'update' parameter\n");
        return rc;
    }

    blemesh_iv_update(enable, index, update);

    return rc;
}

static const struct shell_param iv_update_params[] = {
    {"enable", "usage: =<0-1>"},
    {"index", "usage: =<UINT32>"},
    {"update", "usage: =<0-1>"},
    {NULL, NULL}
};

static const struct shell_cmd_help iv_update_help = {
    .summary = "iv update",
    .usage = NULL,
    .params = iv_update_params,
};


static const struct shell_cmd mesh_commands[] = {
    {
        .sc_cmd = "iv-update",
        .sc_cmd_func = cmd_iv_update,
        .help = &iv_update_help,
    },
    {
        .sc_cmd = "relay-set",
        .sc_cmd_func = cmd_relay_set,
        .help = &relay_set_help,
    },
    {
        .sc_cmd = "send-msg",
        .sc_cmd_func = cmd_send_msg,
        .help = &send_msg_help,
    },
    { NULL, NULL, NULL },
};


void
cmd_init(void)
{
    shell_register(MESH_MODULE, mesh_commands);
    shell_register_default_module(MESH_MODULE);
}
