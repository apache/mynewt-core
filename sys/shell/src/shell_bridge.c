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

#if MYNEWT_VAL(SHELL_BRIDGE)

#include "mgmt/mgmt.h"
#include "cborattr/cborattr.h"
#include "streamer/streamer.h"
#include "tinycbor/cbor_mbuf_writer.h"
#include "shell_priv.h"

static struct mgmt_group shell_bridge_group;

static int shell_bridge_exec(struct mgmt_cbuf *cb);

static struct mgmt_handler shell_bridge_group_handlers[] = {
    [SHELL_NMGR_OP_EXEC] = { NULL, shell_bridge_exec },
};

/**
 * Handler for the `shell exec` newtmgr command.
 */
static int
shell_bridge_exec(struct mgmt_cbuf *cb)
{
    char line[MYNEWT_VAL(SHELL_BRIDGE_MAX_IN_LEN)];
    char *argv[MYNEWT_VAL(SHELL_CMD_ARGC_MAX)];
    struct shell_bridge_streamer sbs;
    CborEncoder str_encoder;
    CborError err;
    int argc;
    int rc;

    const struct cbor_attr_t attrs[] = {
        {
            .attribute = "argv",
            .type = CborAttrArrayType,
            .addr.array = {
                .element_type = CborAttrTextStringType,
                .arr.strings.ptrs = argv,
                .arr.strings.store = line,
                .arr.strings.storelen = sizeof line,
                .count = &argc,
                .maxlen = sizeof argv / sizeof argv[0],
            },
        },
        { 0 },
    };

    err = cbor_read_object(&cb->it, attrs);
    if (err != 0) {
        return MGMT_ERR_EINVAL;
    }

    /* Key="o"; value=<command-output> */
    err |= cbor_encode_text_stringz(&cb->encoder, "o");
    err |= cbor_encoder_create_indef_text_string(&cb->encoder, &str_encoder);

    shell_bridge_streamer_new(&sbs, &str_encoder);
    rc = shell_exec(argc, argv, &sbs.streamer);

    err |= cbor_encoder_close_container(&cb->encoder, &str_encoder);

    /* Key="rc"; value=<status> */
    err |= cbor_encode_text_stringz(&cb->encoder, "rc");
    err |= cbor_encode_int(&cb->encoder, rc);

    if (err != 0) {
        return MGMT_ERR_ENOMEM;
    }

    return 0;
}

int
shell_bridge_init(void)
{
    int rc;

    MGMT_GROUP_SET_HANDLERS(&shell_bridge_group, shell_bridge_group_handlers);
    shell_bridge_group.mg_group_id = MGMT_GROUP_ID_SHELL;

    rc = mgmt_group_register(&shell_bridge_group);
    if (rc) {
        return rc;
    }

    return 0;
}

#endif
