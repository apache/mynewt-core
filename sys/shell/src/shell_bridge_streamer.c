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

#include "tinycbor/cbor.h"
#include "shell_priv.h"

static int
shell_bridge_streamer_write(struct streamer *streamer,
                            const void *src, size_t len)
{
    struct shell_bridge_streamer *sbs;
    int err;

    sbs = (struct shell_bridge_streamer *)streamer;

    /* Encode the data as a CBOR text string. */
    err = cbor_encode_text_string(sbs->str_encoder, src, len);
    if (err != 0) {
        return SYS_ENOMEM;
    }

    return 0;
}

static int
shell_bridge_streamer_vprintf(struct streamer *streamer,
                              const char *fmt, va_list ap)
{
    char buf[MYNEWT_VAL(SHELL_BRIDGE_PRINTF_LEN)];
    int num_chars;
    int rc;

    num_chars = vsnprintf(buf, sizeof buf, fmt, ap);
    if (num_chars > sizeof buf - 1) {
        num_chars = sizeof buf - 1;
    }

    rc = shell_bridge_streamer_write(streamer, buf, num_chars);
    if (rc != 0) {
        return rc;
    }

    return num_chars;
}

static const struct streamer_cfg shell_bridge_streamer_cfg = {
    .write_cb = shell_bridge_streamer_write,
    .vprintf_cb = shell_bridge_streamer_vprintf,
};

void
shell_bridge_streamer_new(struct shell_bridge_streamer *sbs,
                          struct CborEncoder *str_encoder)
{
    *sbs = (struct shell_bridge_streamer) {
        .streamer.cfg = &shell_bridge_streamer_cfg,
        .str_encoder = str_encoder,
    };
}

#endif
