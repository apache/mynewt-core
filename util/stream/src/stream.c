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

#include <string.h>

#include <stream/stream.h>
#include "os/os.h"

int
istream_flush(struct in_stream *istream)
{
    int rc;

    if (istream->vft->flush) {
        rc = istream->vft->flush(istream);
    } else {
        rc = istream_available(istream);
        if (rc > 0) {
            rc = istream_read(istream, NULL, rc);
        }
    }

    return rc;
}

static int
mem_istream_available(struct in_stream *istream)
{
    struct mem_in_stream *mem = (struct mem_in_stream *)istream;
    return mem->size - mem->read_ptr;
}

static int
mem_istream_read(struct in_stream *istream, uint8_t *buf, uint32_t count)
{
    struct mem_in_stream *mem = (struct mem_in_stream *)istream;

    if (count > mem->size - mem->read_ptr) {
        count = mem->size - mem->read_ptr;
    }
    if (buf) {
        memcpy(buf, mem->buf + mem->read_ptr, count);
    }
    mem->read_ptr += count;

    return count;
}

struct in_stream_vft mem_istream_vft = {
    .available = mem_istream_available,
    .read = mem_istream_read,
};

void
mem_istream_init(struct mem_in_stream *mem, const uint8_t *buf, uint32_t size)
{
    mem->vft = &mem_istream_vft;
    mem->buf = buf;
    mem->size = size;
    mem->read_ptr = 0;
}

static int
mem_ostream_write(struct out_stream *ostream, const uint8_t *buf, uint32_t count)
{
    struct mem_out_stream *mem = (struct mem_out_stream *)ostream;
    int skip = 0;
    int write = count;

    /* If write_ptr is negative drop data and update write_ptr */
    if (mem->write_ptr < 0) {
        skip = min(-mem->write_ptr, count);
        mem->write_ptr += skip;
        write -= skip;
    }

    if (mem->write_ptr >= 0 && write > 0 && buf != NULL && mem->write_ptr < mem->size) {
        if (write > mem->size - mem->write_ptr) {
            write = mem->size - mem->write_ptr;
        }
        if (mem->buf) {
            memcpy(mem->buf + mem->write_ptr, buf + skip, write);
        }
    }
    mem->write_ptr += count - skip;

    return count;
}

static int
mem_ostream_flush(struct out_stream *ostream)
{
    (void)ostream;

    return 0;
}

struct out_stream_vft mem_ostream_vft = {
    .write = mem_ostream_write,
    .flush = mem_ostream_flush,
};

void
mem_ostream_init(struct mem_out_stream *mem, uint8_t *buf, uint32_t size)
{
    mem->vft = &mem_ostream_vft;
    mem->buf = buf;
    mem->size = size;
    mem->write_ptr = 0;
}

int
ostream_write(struct out_stream *ostream, const uint8_t *buf, uint32_t count, bool flush)
{
    int rc = ostream->vft->write(ostream, buf, count);
    if (flush) {
        (void)ostream_flush(ostream);
    }
    return rc;
}

int
ostream_flush(struct out_stream *ostream)
{
    if (ostream->vft->flush) {
        return ostream->vft->flush(ostream);
    } else {
        return 0;
    }
}

int
istream_available(struct in_stream *istream)
{
    return istream->vft->available(istream);
}

int
istream_read(struct in_stream *istream, uint8_t *buf, uint32_t count)
{
    return istream->vft->read(istream, buf, count);
}

int
stream_pump(struct in_stream *istream, struct out_stream *ostream, uint32_t count)
{
    int pumped;

    /* If output stream has pump_from function used it */
    if (ostream->vft->pump_from) {
        pumped = ostream->vft->pump_from(ostream, istream, count);
    } else if (istream->vft->pump_to) {
        /* When input stream has pump_to used it */
        pumped = istream->vft->pump_to(istream, ostream, count);
    } else {
        /* Both stream don't provide pump functions, use local buffer for pumping data */
        uint8_t buf[16];
        pumped = 0;
        while (count) {
            uint32_t n = min(count, sizeof(buf));
            uint32_t r = istream_read(istream, buf, n);
            pumped += r;
            count -= r;
            ostream_write(ostream, buf, r, false);
            if (r == 0) {
                break;
            }
        }
    }
    return pumped;
}

int
ostream_write_str(struct out_stream *ostream, const char *str)
{
    int len = strlen(str);
    return ostream_write(ostream, (const uint8_t *)str, len, false);
}
