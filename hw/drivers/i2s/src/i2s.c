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

#include <os/os_eventq.h>
#include <os/os_sem.h>

#include <i2s/i2s.h>
#include <i2s/i2s_driver.h>

/* Function called from i2s_open/os_dev_open */
static int
i2s_open_handler(struct os_dev *dev, uint32_t timout, void *arg)
{
    struct i2s *i2s;
    struct i2s_client *client = (struct i2s_client *)arg;
    struct i2s_sample_buffer *buffer;

    if (dev->od_flags & OS_DEV_F_STATUS_OPEN) {
        return OS_EBUSY;
    }

    i2s = (struct i2s *)dev;

    assert(client == NULL ||
           (client->sample_buffer_ready_cb != NULL &&
            client->state_changed_cb != NULL));
    i2s->client = client;
    if (client && client->sample_rate) {
        i2s->sample_rate = client->sample_rate;
    }

    if (i2s->direction == I2S_IN) {
        while (NULL != (buffer = i2s_buffer_get(i2s, 0))) {
            i2s_buffer_put(i2s, buffer);
        }
    } else {
        i2s_start(i2s);
    }

    return OS_OK;
}

/* Function called from i2s_close/os_dev_close */
static int
i2s_close_handler(struct os_dev *dev)
{
    struct i2s *i2s;

    i2s = (struct i2s *)dev;
    i2s_stop(i2s);
    i2s->client = NULL;

    return OS_OK;
}

static int
i2s_suspend_handler(struct os_dev *dev, os_time_t timeout, int arg)
{
    return i2s_driver_suspend((struct i2s *)dev, timeout, arg);
}

static int
i2s_resume_handler(struct os_dev *dev)
{
    return i2s_driver_resume((struct i2s *)dev);
}

static void
i2s_add_to_user_queue(struct i2s *i2s, struct i2s_sample_buffer *buffer)
{
    STAILQ_INSERT_TAIL(&i2s->user_queue, buffer, next_buffer);
    os_sem_release(&i2s->user_queue_buffer_count);
}

static void
i2s_add_to_driver_queue(struct i2s *i2s, struct i2s_sample_buffer *buffer)
{
    STAILQ_INSERT_TAIL(&i2s->driver_queue, buffer, next_buffer);
    if (i2s->state != I2S_STATE_STOPPED) {
        i2s_driver_buffer_queued(i2s);
    }
}

static void
i2s_buffers_from_pool(struct i2s *i2s, struct i2s_buffer_pool *pool)
{
    int i;
    int sr;
    struct i2s_sample_buffer *buffers;
    uintptr_t sample_data;
    uint32_t samples_per_buffer;

    if (i2s->direction != I2S_IN && pool != NULL) {
        os_sem_init(&i2s->user_queue_buffer_count, pool->buffer_count);
    } else {
        os_sem_init(&i2s->user_queue_buffer_count, 0);
    }

    i2s->buffer_pool = pool;
    if (pool == NULL) {
        return;
    }

    buffers = (struct i2s_sample_buffer *)pool->buffers;

    samples_per_buffer = pool->buffer_size / i2s->sample_size_in_bytes;
    sample_data = (uintptr_t)&buffers[pool->buffer_count];

    for (i = 0; i < pool->buffer_count; ++i) {
        buffers[i].capacity = samples_per_buffer;
        buffers[i].sample_data = (void *)sample_data;
        buffers[i].sample_count = 0;
        sample_data += pool->buffer_size;

        OS_ENTER_CRITICAL(sr);
        if (i2s->direction == I2S_IN) {
            i2s_add_to_driver_queue(i2s, &buffers[i]);
        } else {
            STAILQ_INSERT_TAIL(&i2s->user_queue, &buffers[i], next_buffer);
        }
        OS_EXIT_CRITICAL(sr);
    }
}

int
i2s_init(struct i2s *i2s, struct i2s_buffer_pool *pool)
{
    STAILQ_INIT(&i2s->driver_queue);
    STAILQ_INIT(&i2s->user_queue);

    i2s->state = I2S_STATE_STOPPED;

    i2s_buffers_from_pool(i2s, pool);

    i2s->dev.od_handlers.od_open = i2s_open_handler;
    i2s->dev.od_handlers.od_close = i2s_close_handler;
    i2s->dev.od_handlers.od_suspend = i2s_suspend_handler;
    i2s->dev.od_handlers.od_resume = i2s_resume_handler;

    return 0;
}

struct i2s *
i2s_open(const char *name, uint32_t timeout, struct i2s_client *client)
{
    return (struct i2s *)os_dev_open(name, timeout, client);
}

int
i2s_close(struct i2s *i2s)
{
    return os_dev_close(&i2s->dev);
}

int
i2s_write(struct i2s *i2s, void *samples, uint32_t sample_buffer_size)
{
    uint32_t sample_pair_size = (i2s->sample_size_in_bytes * 2);
    size_t sample_count;
    struct i2s_sample_buffer *buffer;

    assert(i2s->direction == I2S_OUT);

    buffer = i2s_buffer_get(i2s, OS_WAIT_FOREVER);

    if (buffer == NULL) {
        return -1;
    }

    /* Calculate buffer size */
    sample_count = sample_buffer_size / sample_pair_size;
    if (buffer->capacity < sample_count) {
        sample_count = buffer->capacity;
    }

    sample_buffer_size = sample_count * sample_pair_size;

    /* Move data to buffer */
    memcpy(buffer->sample_data, samples, sample_buffer_size);

    /* Pass buffer to output device */
    i2s_buffer_put(i2s, buffer);

    return sample_buffer_size;
}

int
i2s_read(struct i2s *i2s, void *samples, uint32_t sample_buffer_size)
{
    int sr;
    uint32_t sample_pair_size = (i2s->sample_size_in_bytes * 2);
    size_t sample_capacity = sample_buffer_size / sample_pair_size;
    struct i2s_sample_buffer *buffer;

    assert(i2s->direction == I2S_IN);

    if (i2s->state == I2S_STATE_STOPPED) {
        i2s_start(i2s);
    }

    buffer = i2s_buffer_get(i2s, OS_WAIT_FOREVER);

    if (buffer == NULL) {
        return 0;
    }

    if (sample_capacity > buffer->sample_count) {
        sample_capacity = buffer->sample_count;
    }
    sample_buffer_size = sample_capacity * sample_pair_size;
    memcpy(samples, buffer->sample_data, sample_buffer_size);
    if (sample_capacity < buffer->sample_count) {
        /* Not all data consumed, modify buffer and put buffer at the front again */
        memmove(buffer->sample_data, buffer->sample_data,
                (buffer->sample_count - sample_capacity) * sample_pair_size);
        OS_ENTER_CRITICAL(sr);
        STAILQ_INSERT_HEAD(&i2s->user_queue, buffer, next_buffer);
        OS_EXIT_CRITICAL(sr);
    }

    return sample_buffer_size;
}

int
i2s_start(struct i2s *i2s)
{
    int rc = I2S_OK;

    if (i2s->state != I2S_STATE_RUNNING) {
        if (STAILQ_EMPTY(&i2s->driver_queue)) {
            i2s->state = I2S_STATE_OUT_OF_BUFFERS;
            rc = I2S_ERR_NO_BUFFER;
        } else {
            rc = i2s_driver_start(i2s);
            if (rc == I2S_OK) {
                i2s->state = I2S_STATE_RUNNING;
                if (i2s->client) {
                    i2s->client->state_changed_cb(i2s, I2S_STATE_RUNNING);
                }
            }
        }
    }
    return rc;
}

int
i2s_stop(struct i2s *i2s)
{
    struct i2s_sample_buffer *buffer;

    i2s_driver_stop(i2s);

    i2s->state = I2S_STATE_STOPPED;
    if (i2s->client) {
        i2s->client->state_changed_cb(i2s, i2s->state);
    }

    if (i2s->direction == I2S_IN) {
        while (NULL != (buffer = i2s_buffer_get(i2s, 0))) {
            i2s_add_to_driver_queue(i2s, buffer);
        }
    } else {
        while (!STAILQ_EMPTY(&i2s->driver_queue)) {
            buffer = STAILQ_FIRST(&i2s->driver_queue);
            STAILQ_REMOVE_HEAD(&i2s->driver_queue, next_buffer);
            i2s_add_to_user_queue(i2s, buffer);
        }
    }
    return 0;
}

int
i2s_available_buffers(struct i2s *i2s)
{
    return os_sem_get_count(&i2s->user_queue_buffer_count);
}

struct i2s_sample_buffer *
i2s_buffer_get(struct i2s *i2s, os_time_t timeout)
{
    int sr;
    struct i2s_sample_buffer *buffer = NULL;

    if (OS_OK == os_sem_pend(&i2s->user_queue_buffer_count, timeout)) {
        OS_ENTER_CRITICAL(sr);
        buffer = STAILQ_FIRST(&i2s->user_queue);
        assert(buffer);
        STAILQ_REMOVE_HEAD(&i2s->user_queue, next_buffer);
        OS_EXIT_CRITICAL(sr);
        assert(buffer->capacity > 0);
    }

    return buffer;
}

int
i2s_buffer_put(struct i2s *i2s, struct i2s_sample_buffer *buffer)
{
    int sr;
    int rc = I2S_OK;

    /*
     * Output sample buffer without samples?
     * Don't bother driver, just put in client queue.
     */
    if (i2s->direction == I2S_OUT && buffer->sample_count == 0) {
        i2s_driver_buffer_put(i2s, buffer);
    } else {
        OS_ENTER_CRITICAL(sr);
        i2s_add_to_driver_queue(i2s, buffer);
        OS_EXIT_CRITICAL(sr);

        if (i2s->state == I2S_STATE_OUT_OF_BUFFERS) {
            rc = i2s_driver_start(i2s);
        }
    }
    return rc;
}

struct i2s_sample_buffer *
i2s_driver_buffer_get(struct i2s *i2s)
{
    int sr;
    struct i2s_sample_buffer *buffer;

    OS_ENTER_CRITICAL(sr);
    buffer = STAILQ_FIRST(&i2s->driver_queue);
    if (buffer) {
        STAILQ_REMOVE_HEAD(&i2s->driver_queue, next_buffer);
    }
    OS_EXIT_CRITICAL(sr);

    return buffer;
}

void
i2s_driver_buffer_put(struct i2s *i2s, struct i2s_sample_buffer *buffer)
{
    int sr;

    assert(buffer != NULL && i2s != NULL);
    if (i2s->client) {
        /* If callback returns 1, buffer is not added to the pool */
        if (i2s->client->sample_buffer_ready_cb(i2s, buffer) == 1) {
            return;
        }
    }
    OS_ENTER_CRITICAL(sr);
    i2s_add_to_user_queue(i2s, buffer);
    OS_EXIT_CRITICAL(sr);
}

void
i2s_driver_state_changed(struct i2s *i2s, enum i2s_state state)
{
    if (i2s->state != state) {
        i2s->state = state;
        if (i2s->client) {
            i2s->client->state_changed_cb(i2s, state);
        }
    }
}
