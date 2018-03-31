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

#ifndef __ADC_H__
#define __ADC_H__

#include "os/mynewt.h"

#ifdef __cplusplus
extern "C" {
#endif

struct adc_dev;

/**
 * Types of ADC events passed to the ADC driver.
 */
typedef enum {
    /* This event represents the result of an ADC run. */
    ADC_EVENT_RESULT = 0
} adc_event_type_t;

/**
 * Event handler for ADC events that are processed in asynchronous mode.
 *
 * @param The ADC device being processed
 * @param The argument data passed to adc_set_result_handler()
 * @param The event type
 * @param The buffer containing event data
 * @param The size in bytes of that buffer.
 *
 * @return 0 on success, non-zero error code on failure
 */
typedef int (*adc_event_handler_func_t)(struct adc_dev *, void *,
    adc_event_type_t, void *, int);

/**
 * Configure an ADC channel for this ADC device.  This is implemented
 * by the HW specific drivers.
 *
 * @param The ADC device to configure
 * @param The channel number to configure
 * @param An opaque blob containing HW specific configuration.
 *
 * @return 0 on success, non-zero error code on failure.
 */
typedef int (*adc_configure_channel_func_t)(struct adc_dev *dev, uint8_t,
        void *);

/**
 * Trigger a sample on the ADC device asynchronously.  This is implemented
 * by the HW specific drivers.
 *
 * @param The ADC device to sample
 *
 * @return 0 on success, non-zero error code on failure
 */
typedef int (*adc_sample_func_t)(struct adc_dev *);

/**
 * Blocking read of an ADC channel.  This is implemented by the HW specific
 * drivers.
 *
 * @param The ADC device to perform the blocking read on
 * @param The channel to read
 * @param The result to put the ADC reading into
 *
 * @return 0 on success, non-zero error code on failure
 */
typedef int (*adc_read_channel_func_t)(struct adc_dev *, uint8_t, int *);

/**
 * Set the buffer(s) to read ADC results into for non-blocking reads.  This
 * is implemented by the HW specific drivers.
 *
 * @param The ADC device to read results into
 * @param The primary buffer to read results into
 * @param The secondary buffer to read results into (for cases where
 *        DMA'ing occurs, and secondary buffer can be used while primary
 *        is being processed.)
 * @param The length of both buffers in number of bytes
 *
 * @return 0 on success, non-zero error code on failure.
 */
typedef int (*adc_buf_set_func_t)(struct adc_dev *, void *, void *, int);

/**
 * Release a buffer for an ADC device, allowing the driver to re-use it for
 * DMA'ing data.
 *
 * @param The ADC device to release the buffer to
 * @param A pointer to the buffer to release
 * @param The length of the buffer being released.
 *
 * @return 0 on success, non-zero error code on failure.
 */
typedef int (*adc_buf_release_func_t)(struct adc_dev *, void *, int);

/**
 * Read the next entry from an ADC buffer as a integer
 *
 * @param The ADC device to read the entry from
 * @param The buffer to read the entry from
 * @param The total length of the buffer
 * @param The entry number to read from the buffer
 * @param The result to put the entry into
 *
 * @return 0 on success, non-zero error code on failure.
 */
typedef int (*adc_buf_read_func_t)(struct adc_dev *, void *, int, int, int *);

/**
 * Get the size of a buffer
 *
 * @param The ADC device to get the buffer size from
 * @param The number of channels in the buffer
 * @param The number of samples in the buffer
 *
 * @return The size of the buffer
 */
typedef int (*adc_buf_size_func_t)(struct adc_dev *, int, int);

struct adc_driver_funcs {
    adc_configure_channel_func_t af_configure_channel;
    adc_sample_func_t af_sample;
    adc_read_channel_func_t af_read_channel;
    adc_buf_set_func_t af_set_buffer;
    adc_buf_release_func_t af_release_buffer;
    adc_buf_read_func_t af_read_buffer;
    adc_buf_size_func_t af_size_buffer;
};

struct adc_chan_config {
    uint16_t c_refmv;
    uint8_t c_res;
    uint8_t c_configured;
    uint8_t c_cnum;
};

struct adc_dev {
    struct os_dev ad_dev;
    struct os_mutex ad_lock;
    struct adc_driver_funcs ad_funcs;
    struct adc_chan_config *ad_chans;
    int ad_chan_count;
    adc_event_handler_func_t ad_event_handler_func;
    void *ad_event_handler_arg;
};

int adc_chan_config(struct adc_dev *, uint8_t, void *);
int adc_chan_read(struct adc_dev *, uint8_t, int *);
int adc_event_handler_set(struct adc_dev *, adc_event_handler_func_t,
        void *);

/**
 * Sample the device specified by dev.  This is used in non-blocking mode
 * to generate samples into the event buffer.
 *
 * @param dev The device to sample
 *
 * @return 0 on success, non-zero on failure
 */
static inline int
adc_sample(struct adc_dev *dev)
{
    return (dev->ad_funcs.af_sample(dev));
}

/**
 * Blocking read of an ADC channel.  This is implemented by the HW specific
 * drivers.
 *
 * @param The ADC device to perform the blocking read on
 * @param The channel to read
 * @param The result to put the ADC reading into
 *
 * @return 0 on success, non-zero error code on failure
 */
static inline int
adc_read_channel(struct adc_dev *dev, uint8_t ch, int *result)
{
    return (dev->ad_funcs.af_read_channel(dev, ch, result));
}

/**
 * Set a result buffer to store data into.  Optionally, provide a
 * second buffer to continue writing data into as the first buffer
 * fills up.  Both buffers must be the same size.
 *
 * @param dev The ADC device to set the buffer for
 * @param buf1 The first buffer to spool data into
 * @param buf2 The second buffer to spool data into, while the first
 *             buffer is being processed.  If NULL is provided, it's
 *             unused.
 * @param buf_len The length of both buffers, in bytes.
 *
 * @return 0 on success, non-zero on failure.
 */
static inline int
adc_buf_set(struct adc_dev *dev, void *buf1, void *buf2,
        int buf_len)
{
    return (dev->ad_funcs.af_set_buffer(dev, buf1, buf2, buf_len));
}

/**
 * Release a result buffer on the ADC device, and allow for it to be
 * re-used for DMA'ing results.
 *
 * @param dev The device to release the buffer for
 * @param buf The buffer to release
 * @param buf_len The length of the buffer being released.
 *
 * @return 0 on success, non-zero error code on failure.
 */
static inline int
adc_buf_release(struct adc_dev *dev, void *buf, int buf_len)
{
    return (dev->ad_funcs.af_release_buffer(dev, buf, buf_len));
}

/**
 * Read an entry from an ADC buffer
 *
 * @param dev The device that the entry is being read from
 * @param buf The buffer that we're reading the entry from
 * @param buf_len The length of the buffer we're reading the entry from
 * @param off The entry number to read from the buffer
 * @param result A pointer to the result to store the data in
 *
 * @return 0 on success, non-zero error code on failure
 */
static inline int
adc_buf_read(struct adc_dev *dev, void *buf, int buf_len, int entry,
        int *result)
{
    return (dev->ad_funcs.af_read_buffer(dev, buf, buf_len, entry, result));
}

/**
 * Return the size of an ADC buffer
 *
 * @param dev The ADC device to return the buffer size from
 * @param channels The number of channels for these samples
 * @param samples The number of SAMPLES on this ADC device
 *
 * @return The size of the resulting buffer
 */
static inline int
adc_buf_size(struct adc_dev *dev, int chans, int samples)
{
    return (dev->ad_funcs.af_size_buffer(dev, chans, samples));
}

/**
 * Take an ADC result and convert it to millivolts.
 *
 * @param dev The ADC dev to convert the result on
 * @param cnum The channel number to convert the result from
 * @param val The ADC value to convert to millivolts
 *
 * @return The convert value in millivolts
 */
static inline int
adc_result_mv(struct adc_dev *dev, uint8_t cnum, int val)
{
    int res;
    int refmv;
    int ret;

    refmv = (int) dev->ad_chans[cnum].c_refmv;
    res = (int) dev->ad_chans[cnum].c_res;

    ret = val * refmv;
    ret += (1 << (res - 2));
    ret = ret >> res;

    return (ret);
}

#ifdef __cplusplus
}
#endif

#endif /* __ADC_H__ */
