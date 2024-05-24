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

#include <assert.h>

#include <os/util.h>
#include <stream/stream.h>
#include <i2s/i2s.h>
#include <i2s/i2s_stream.h>

OSTREAM_DEF(i2s_out_stream);

static int
i2s_out_stream_pump_from(struct out_stream *ostream, struct in_stream *istream, uint32_t count)
{
    struct i2s_out_stream *i2s_str = CONTAINER_OF(ostream, struct i2s_out_stream, ostream);
    struct i2s_sample_buffer *buffer = i2s_str->buffer;
    struct i2s *i2s = i2s_str->i2s;
    int written = 0;
    uint32_t available = istream_available(istream);
    uint32_t sample_count;

    if (available < count) {
        count = available;
    }
    sample_count = count / i2s->sample_size_in_bytes;

    while (sample_count) {
        if (buffer == NULL) {
            buffer = i2s_buffer_get(i2s, 0);
            if (buffer == NULL) {
                break;
            }
            buffer->sample_count = 0;
        }
        uint32_t space = buffer->capacity - buffer->sample_count;
        size_t copied = min(space, sample_count);
        if (copied) {
            uint8_t *buf = buffer->sample_data;
            istream_read(istream, buf + buffer->sample_count * i2s->sample_size_in_bytes,
                         copied * i2s->sample_size_in_bytes);
            buffer->sample_count += copied;
            written += copied;
            sample_count -= copied;
        }
        if (buffer->sample_count >= buffer->capacity) {
            i2s_buffer_put(i2s, buffer);
            buffer = NULL;
        }
    }
    i2s_str->buffer = buffer;
    return written * i2s_str->i2s->sample_size_in_bytes;
}

static int
i2s_out_stream_write(struct out_stream *ostream, const uint8_t *buf, uint32_t count)
{
    struct mem_in_stream mstr;
    mem_istream_init(&mstr, buf, count);

    return i2s_out_stream_pump_from(ostream, (struct in_stream *)&mstr, count);
}

static int
i2s_out_stream_flush(struct out_stream *ostream)
{
    struct i2s_out_stream *str = CONTAINER_OF(ostream, struct i2s_out_stream, ostream);
    struct i2s *i2s = str->i2s;
    struct i2s_sample_buffer *buffer = str->buffer;

    str->buffer = NULL;
    if (buffer) {
        i2s_buffer_put(i2s, buffer);
    }

    return 0;
}
