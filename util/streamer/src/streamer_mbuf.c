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
#include "streamer/streamer.h"

static int
streamer_mbuf_write(struct streamer *streamer, const void *src, size_t len)
{
    struct streamer_mbuf *sm;
    int rc;

    if (len > UINT16_MAX) {
        return SYS_EINVAL;
    }

    sm = (struct streamer_mbuf *)streamer;
    rc = os_mbuf_append(sm->om, src, len);
    if (rc != 0) {
        return os_error_to_sys(rc);
    }

    return 0;
}

static int
streamer_mbuf_vprintf(struct streamer *streamer, const char *fmt, va_list ap)
{
    char buf[MYNEWT_VAL(STREAMER_MBUF_PRINTF_MAX)];
    int num_chars;
    int rc;

    num_chars = vsnprintf(buf, sizeof buf, fmt, ap);
    if (num_chars > sizeof buf - 1) {
        num_chars = sizeof buf - 1;
    }

    rc = streamer_mbuf_write(streamer, buf, num_chars);
    if (rc != 0) {
        return rc;
    }

    return num_chars;
}

static const struct streamer_cfg streamer_cfg_mbuf = {
    .write_cb = streamer_mbuf_write,
    .vprintf_cb = streamer_mbuf_vprintf,
};

int
streamer_mbuf_new(struct streamer_mbuf *sm, struct os_mbuf *om)
{
    if (sm == NULL || om == NULL) {
        return SYS_EINVAL;
    }

    *sm = (struct streamer_mbuf) {
        .streamer.cfg = &streamer_cfg_mbuf,
        .om = om,
    };

    return 0;
}

int
streamer_msys_new(struct streamer_mbuf *sm)
{
    struct os_mbuf *om;
    int rc;

    om = os_msys_get_pkthdr(0, 0);
    if (om == NULL) {
        return SYS_ENOMEM;
    }

    rc = streamer_mbuf_new(sm, om);
    if (rc != 0) {
        return rc;
    }

    return 0;
}
