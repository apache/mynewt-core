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

#ifndef _HW_DRIVERS_I2S_H
#define _HW_DRIVERS_I2S_H

#include <stdint.h>
#include <os/os_dev.h>

/*
 * I2S API does not specify this structure. It is used by i2s_create() and is defined
 * by specific driver.
 */
struct i2s_cfg;

/**
 * Buffer for passing data between user code and i2s driver
 */
struct i2s_sample_buffer {
    /* Internal use */
    STAILQ_ENTRY(i2s_sample_buffer) next_buffer;
    /** Actual sample data pointer */
    void *sample_data;
    /**
     * Number of samples that buffer can hold
     * This value is used for input I2S by driver.
     */
    uint32_t capacity;
    /**
     * Actual number of samples in buffer.
     * This value is used when buffer is filled with samples.
     * For output i2s user code fills this value.
     * For input i2s driver fills this value.
     */
    uint32_t sample_count;
};

struct i2s_buffer_pool {
    uint16_t buffer_size;
    uint16_t buffer_count;
    struct i2s_sample_buffer *buffers;
};

/**
 * I2S buffer pool definition.
 * @param name   pool name to be used,
 * @param count  number of buffers to initialize
 * @param size   single buffer size in bytes
 */
#define I2S_BUFFER_POOL_DEF(name, count, size) \
    static uint8_t _Alignas(struct i2s_buffer_pool) name ## _buffers[(sizeof(struct i2s_sample_buffer) + size) * \
                                                                     count]; \
    struct i2s_buffer_pool name = { \
        .buffer_size = size, \
        .buffer_count = count, \
        .buffers = (struct i2s_sample_buffer *)name ## _buffers \
    }
#define I2S_BUFFER_POOL(name) ((struct i2s_buffer_pool *)(&name))

enum i2s_state {
    I2S_STATE_STOPPED,
    I2S_STATE_OUT_OF_BUFFERS,
    I2S_STATE_RUNNING,
};

enum i2s_direction {
    I2S_INVALID,
    I2S_OUT,
    I2S_IN,
    I2S_OUT_IN,
};

/**
 * I2S device
 */
struct i2s {
    struct os_dev dev;
    void *driver_data;
    struct i2s_buffer_pool *buffer_pool;

    /* Internal queues not for user code */
    STAILQ_HEAD(, i2s_sample_buffer) user_queue;
    STAILQ_HEAD(, i2s_sample_buffer) driver_queue;
    /* Semaphore holding number of elements in user queue */
    struct os_sem user_queue_buffer_count;

    struct i2s_client *client;
    /* Samples per second. */
    uint32_t sample_rate;
    uint8_t sample_size_in_bytes;
    enum i2s_direction direction;
    enum i2s_state state;
};

#define I2S_OK                  0
#define I2S_ERR_NO_BUFFER       -1
#define I2S_ERR_INTERNAL        -2

/**
 * Function that will be called from interrupt after sample_buffer if processed by I2S driver.
 * For output I2S it will be called when last sample was sent out.
 * For input I2S it will be called when whole buffer is filled with samples.
 *
 * @return 0 - buffer should be queued in i2s device for further usage.
 *         1 - buffer will not be stored in i2s. This is useful when buffers are provided for
 *             data playback from FLASH and once they are processed sent out they are not be used
 *             any more. That way output I2S device may work without any RAM buffer.
 */
typedef int (*i2s_sample_buffer_ready_t)(struct i2s *i2s, struct i2s_sample_buffer *sample_buffer);

/**
 * Function will be called (possibly from interrupt context) when driver state changes.
 * For practical reason it can be useful to know when I2S stream was stopped because
 * it run out of buffers (I2S_STATE_OUT_OF_BUFFERS).
 */
typedef void (*i2s_state_change_t)(struct i2s *i2s, enum i2s_state state);

/**
 * Client interface.
 */
struct i2s_client {
    /** Requested sample rate */
    uint32_t sample_rate;
    /** Function called when I2S state changes */
    i2s_state_change_t state_changed_cb;
    /** Function called when buffer is ready and i2s_buffer_get() will succeed */
    i2s_sample_buffer_ready_t sample_buffer_ready_cb;
};

/**
 * Creates I2S device with given name and configuration.
 *
 * @param i2s   device to crate
 * @param name  name of the i2s device that can be used in i2s_open()
 * @param cfg   device specific configuration
 *
 * @return OS_OK when device was created successfully, non zero on failure.
 */
int i2s_create(struct i2s *i2s, const char *name, const struct i2s_cfg *cfg);

/**
 * Open i2s device
 *
 * @param name        device name to open
 * @param timeout     timeout for open
 * @param client      structure with user callbacks, can be NULL
 *
 * @return pointer ot i2s device on success, NULL on failure
 */
struct i2s *i2s_open(const char *name, uint32_t timeout, struct i2s_client *client);

/**
 * Convenience function for closing i2s device
 *
 * @param i2s        device to close
 *
 * @return 0 on success, non zero on failure
 */
int i2s_close(struct i2s *i2s);

/**
 * Start i2s device operation
 *
 * For i2s input device, it will start filling buffers that were queued.
 *
 * @param i2s   device to start
 *
 * @return 0 on success, I2S_STATE_OUT_OF_BUFFERS is device is paused due to missing buffers
 */
int i2s_start(struct i2s *i2s);

/**
 * Stop i2s device operation
 *
 * @param i2s   device to stop
 *
 * @return 0 on success, other values in the future
 */
int i2s_stop(struct i2s *i2s);

/**
 * High level function to write samples to I2S device.
 * Function is intended to be used from tasks. It will block if sample buffers are
 * not ready yet.
 *
 * @param i2s                 device to send samples to
 * @param samples             sample buffer
 * @param sample_buffer_size  size of sample buffers in bytes
 *
 * @return number of bytes consumed, it may be less then sample_buffer_size
 */
int i2s_write(struct i2s *i2s, void *samples, uint32_t sample_buffer_size);

/**
 * High level function to read samples from I2S device.
 * Function is intended to be used from tasks. It will block if sample buffers are
 * not ready yet.
 *
 * @param i2s                 device to read samples from
 * @param samples             buffer to be filled with samples
 * @param sample_buffer_size  size of sample buffers in bytes
 *
 * @return number of bytes read, it may be less then sample_buffer_size.
 */
int i2s_read(struct i2s *i2s, void *samples, uint32_t sample_buffer_size);

/**
 * Return number of buffers that can be acquired by application without blocking
 *
 * @param i2s   device to check number of buffers from
 *
 * @return number of buffers that can be get without blocking, negative value on error
 */
int i2s_available_buffers(struct i2s *i2s);

/**
 * Dequeue buffer from I2S
 *
 * For output I2S (speaker), function will return buffer that user code will
 * fill with samples and then pass to same i2s to be played with i2s_buffer_put.
 *
 * For input I2S (microphone), function will block and receive samples that were
 * recorded. Once samples are processed user code should enqueue buffer to receive
 * more samples.
 *
 * @param i2s       interface to use (speaker of microphone)
 * @param timeout   time to wait for buffer to be ready
 *
 * @return pointer  to sample buffer with data (for microphone), with no data (for
 *                  speaker). NULL if there was buffer available in specified time.
 */
struct i2s_sample_buffer *i2s_buffer_get(struct i2s *i2s, os_time_t timeout);

/**
 * Add/return buffer to I2S
 *
 * For output I2S buffer contains sample that should be sent to I2S device.
 * For input I2S buffer will be filled with incoming data
 * samples.
 *
 * @param i2s     interface to use
 * @param buffer buffer with samples to be played for output I2S,
 *                buffer for incoming samples for input I2S.
 * @return 0 on success, non 0 on failure
 */
int i2s_buffer_put(struct i2s *i2s, struct i2s_sample_buffer *buffer);

static inline uint32_t
i2s_get_sample_rate(struct i2s *i2s)
{
    return i2s->sample_rate;
}

#endif /* _HW_DRIVERS_I2S_H */
