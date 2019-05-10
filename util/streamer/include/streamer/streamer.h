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

#ifndef H_STREAMER_
#define H_STREAMER_

#include "os/mynewt.h"

struct streamer;

typedef int streamer_write_fn(struct streamer *streamer,
                              const void *src, size_t len);

typedef int streamer_vprintf_fn(struct streamer *streamer,
                                const char *fmt, va_list ap); 

struct streamer_cfg {
    streamer_write_fn *write_cb;
    streamer_vprintf_fn *vprintf_cb;
};

/**
 * @brief Provides a generic data streaming interface.
 */
struct streamer {
    const struct streamer_cfg *cfg;
};

/**
 * @brief Streams data to an mbuf chain.
 */
struct streamer_mbuf {
    struct streamer streamer; /* Must be first member. */
    struct os_mbuf *om;
};

/**
 * @brief Writes the given flat buffer to a streamer.
 *
 * @param streamer              The streamer to write to.
 * @param src                   The flat buffer to write.
 * @param len                   The number of bytes to write.
 *
 * @return                      0 on success; SYS_E[...] on failure.
 */
int streamer_write(struct streamer *streamer, const void *src, size_t len);

/**
 * @brief Writes printf-formatted text to a streamer.
 *
 * A null-terminator does *not* get written.
 *
 * @param streamer              The streamer to write to.
 * @param src                   The flat buffer to write.
 * @param len                   The number of bytes to write.
 *
 * @return                      Number of bytes written on success;
 *                              SYS_E[...] on failure.
 */
int streamer_vprintf(struct streamer *streamer, const char *fmt, va_list ap);

/**
 * @brief Writes printf-formatted text to a streamer.
 */
int streamer_printf(struct streamer *streamer, const char *fmt, ...);

/**
 * @brief Acquires the singleton console streamer.
 */
struct streamer *streamer_console_get(void);

/**
 * Constructs an mbuf streamer.
 *
 * @param sm                    The mbuf streamer object to populate.
 * @param om                    The mbuf chain to write to.  This may already
 *                                  contain data.
 *
 * @return                      0 on success; SYS_E[...] on failure.
 */
int streamer_mbuf_new(struct streamer_mbuf *sm, struct os_mbuf *om);

/**
 * Constructs an mbuf streamer that uses mbufs acquired from msys.
 *
 * @param sm                    The mbuf streamer object to populate.
 *
 * @return                      0 on success; SYS_E[...] on failure.
 */
int streamer_msys_new(struct streamer_mbuf *sm);

#endif
