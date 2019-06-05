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

#include <stdarg.h>
#include "streamer/streamer.h"

int
streamer_write(struct streamer *streamer, const void *src, size_t len)
{
    return streamer->cfg->write_cb(streamer, src, len);
}

int
streamer_vprintf(struct streamer *streamer, const char *fmt, va_list ap)
{
    return streamer->cfg->vprintf_cb(streamer, fmt, ap);
}

int
streamer_printf(struct streamer *streamer, const char *fmt, ...)
{
    va_list ap;
    int rc;

    va_start(ap, fmt);
    rc = streamer_vprintf(streamer, fmt, ap);
    va_end(ap);

    return rc;
}
