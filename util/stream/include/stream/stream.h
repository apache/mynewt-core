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

#ifndef H_STREAM_
#define H_STREAM_

#include <stdint.h>
#include <stdbool.h>

/* input stream type */
struct in_stream;
/* output stream type */
struct out_stream;

/* Memory reader stream, should be initialized with mem_istream_init() */
struct mem_in_stream;

/* Input stream functions */
struct in_stream_vft {
    /** Read count of bytes from stream
     *
     * @param istream - stream to read from
     * @param buf - buffer to read bytes to
     * @param count - number of bytes to read
     * @return non negative value indicating number of bytes actually read
     *         negative error code
     */
    int (*read)(struct in_stream *istream, uint8_t *buf, uint32_t count);
    /** Flush input stream (drop any data)
     *
     * Optional function to drop data, if not present istream_flush() will just read
     * all available data to memory and discard it. If function is present dropping
     * data may be just changing internal stream data without copying data that is
     * going to be lost anyway.
     *
     * @param istream - stream to drop byte from
     * @return non negative value indicating number of bytes dropped
     *         negative value for error code
     */
    int (*flush)(struct in_stream *istream);
    /** Read available bytes number
     *
     * @param istream - stream to check for available bytes
     * @return non negative number of bytes that can be read
     *         negative error code
     */
    int (*available)(struct in_stream *istream);

    /**
     * Pump data from this in_stream to other out_stream
     *
     * @param istream - this input stream to get data from
     * @param ostream - output stream to put data to
     * @param count - number of bytes to pump
     * @return non-negative number of bytes written
     *         negative error code
     */
    int (*pump_to)(struct in_stream *istream, struct out_stream *ostream, uint32_t count);
};

/* Output stream functions */
struct out_stream_vft {
    /** Write number of bytes to the stream
     *
     * @param ostream - stream to writes bytes to
     * @param buf - address of memory buffer with bytes to write
     * @param count - number of bytes to write
     * @return non-negative number of bytes written (can be less then requested)
     *         negative value for error code
     */
    int (*write)(struct out_stream *ostream, const uint8_t *buf, uint32_t count);
    /** Flush data from stream
     *
     * Optional function that forces data to be flushed. For some streams it may
     * actually send cached data in packets.
     * @param ostream - stream to flush
     * @return non-negative number of bytes written
     *         negative error code
     */
    int (*flush)(struct out_stream *ostream);

    /**
     * Pump data from in_stream to this out_stream
     *
     * @param ostream - this output stream to write data to
     * @param istream - other input stream to get data from
     * @param count - number for bytes to pump
     * @return non-negative number of bytes written
     *         negative error code
     */
    int (*pump_from)(struct out_stream *ostream, struct in_stream *istream, uint32_t count);
};

/* Plain input stream */
struct in_stream {
    const struct in_stream_vft *vft;
};

/* Plain output stream */
struct out_stream {
    const struct out_stream_vft *vft;
};

struct mem_in_stream {
    struct in_stream_vft *vft;
    const uint8_t *buf;
    uint32_t size;
    uint32_t read_ptr;
};

struct mem_out_stream {
    struct out_stream_vft *vft;
    uint8_t *buf;
    uint32_t size;
    int write_ptr;
};

#define OSTREAM_DEF(type) \
    static int type ## _write(struct out_stream *ostream, const uint8_t *buf, uint32_t count); \
    static int type ## _flush(struct out_stream *ostream); \
    const struct out_stream_vft type ## _vft = { \
        .write = type ## _write, \
        .flush = type ## _flush, \
        .pump_from = NULL, \
    }

#define OSTREAM_VFT(type, _write, _flush, _pump) \
    static int _write(struct out_stream *ostream, const uint8_t *buf, uint32_t count); \
    static int _flush(struct out_stream *ostream); \
    static int _pump_from(struct out_stream *ostream, struct in_stream *istream, uint32_t count); \
    const struct out_stream_vft type ## _vft = { \
        .write = _write, \
        .flush = _flush, \
        .pump_from = _pump_from, \
    }

#define OSTREAM(type, name)     \
    struct type name = {        \
        .vft = &type ## _vft    \
    }

#define OSTREAM_INIT(type, field)     \
    .field.vft = &type ## _vft

#define ISTREAM_DEF(type)                       \
    const struct in_stream_vft type ## _vft = { \
        .available = type ## _available,        \
        .read = type ## _read,                  \
        .pump_to = NULL,                        \
    }

#define ISTREAM(type, name)     \
    struct in_stream name = {   \
        .vft = &type ## _vft    \
    }

#define ISTREAM_INIT(type, field)     \
    .field.vft = &type ## _vft  \

/**
 * Check how many bytes can be read out of stream.
 *
 * @param istream stream to check
 *
 * @return number of bytes that can be read
 */
int istream_available(struct in_stream *istream);

/**
 * Read data from stream.
 *
 * @param istream   stream to read data from
 * @param buf       memory to write data to
 * @param count     number of bytes to read
 *
 * @return actual number of bytes read
 */
int istream_read(struct in_stream *istream, uint8_t *buf, uint32_t count);

/**
 * Flush input stream
 *
 * All available data are discarded.
 *
 * @param istream   stream to discard data from
 *
 * @return number of bytes that were discarded
 */
int istream_flush(struct in_stream *istream);

/**
 * Flush output stream
 *
 * If stream supports buffering function writes data out.
 *
 * @param ostream   stream to flush
 *
 * @return number of bytes flushed
 */
int ostream_flush(struct out_stream *ostream);

/**
 * Write data to stream
 *
 * @param ostream   stream to write data to
 * @param buf       buffer with data to write
 * @param count     number of bytes to write
 * @param flush     if true data should be immediately flushed
 *
 * @return number of bytes written
 */
int ostream_write(struct out_stream *ostream, const uint8_t *buf, uint32_t count, bool flush);

int stream_pump(struct in_stream *istream, struct out_stream *ostream, uint32_t count);

/**
 * Initialize memory input stream
 *
 * @param mem   stream to initialize
 * @param buf   buffer with data
 * @param size  size of data
 */
void mem_istream_init(struct mem_in_stream *mem, const uint8_t *buf, uint32_t size);

/**
 * Initialize output stream that will receive data into provided buffer
 *
 * @param mem - stream to initialize
 * @param buf - memory to store written data
 * @param size - size of buf
 */
void mem_ostream_init(struct mem_out_stream *mem, uint8_t *buf, uint32_t size);

static inline int
ostream_write_uint8(struct out_stream *ostream, uint8_t data)
{
    return ostream_write(ostream, &data, 1, false);
}

static inline int
ostream_write_uint16(struct out_stream *ostream, uint16_t data)
{
    return ostream_write(ostream, (uint8_t *)&data, 2, false);
}

static inline int
ostream_write_uint32(struct out_stream *ostream, uint32_t data)
{
    return ostream_write(ostream, (uint8_t *)&data, 4, false);
}

/**
 * Write null terminated string to output stream
 *
 * @param ostream - stream to write to
 * @param str     - string to write to stream
 *
 * @return number of bytes written, negative on error
 */
int ostream_write_str(struct out_stream *ostream, const char *str);

#endif /* H_STREAM_ */
