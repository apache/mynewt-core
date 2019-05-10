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

#include "console/console.h"
#include "streamer/streamer.h"

static int
streamer_console_write(struct streamer *streamer, const void *src, size_t len)
{
    console_write(src, len);
    return 0;
}

static int
streamer_console_vprintf(struct streamer *streamer,
                         const char *fmt, va_list ap)
{
    return console_vprintf(fmt, ap);
}

static const struct streamer_cfg streamer_cfg_console = {
    .write_cb = streamer_console_write,
    .vprintf_cb = streamer_console_vprintf,
};

static struct streamer streamer_console = {
    .cfg = &streamer_cfg_console,
};

struct streamer *
streamer_console_get(void)
{
    return &streamer_console;
}
