<!--
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
#  KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#
-->

# I2S interface (Inter-IC Sound)

# Overview

Inter-IC Sound is interface for sending digital audio data.

Data send to and received from I2S device is grouped in sample buffers.
When user code wants to send audio data to I2S device (speaker), it first
request buffer from I2S device, fills it with samples and send it back to the
device. When data is transmitted out of the system to external IC, buffer
become available to the user core and can be obtained again.

For input devices like digital microphone, correctly configured i2s device has
several buffers that are filled with incoming data.
User code requests sample buffer and gets buffers filled with data, user code
can process this data and must return buffer back to the i2s device.

General flow is to get buffers from i2s device fill them for outgoing transmission
(or interpret samples for incoming transmission) and send buffer back to the device. 

# API

#### Device creation

```i2s_create(i2s, "name", cfg)```

Function creates I2S device with specified name and configuration.
Note that cfg argument is not defined in **api.h** but by driver package that must be present in build.
#### Device access

`i2s_open("dev_name", timout, client)`

Opens I2S interface. It can be input (microphone) or output (speaker) interface.

`i2s_close()`

Closes I2S interface, opened with `i2s_open()`

#### Start/stop device operation

`i2s_start()`

Starts I2S device operation, for input, device starts collecting samples to internal buffers.
For output, samples are streamed out of device if they are already sent to device with `i2s_buffer_put()`.
* NOTE: this function must be called explicitly for input i2s device.

`i2s_stop()`

Stops sending or receiving samples. 

#### High level read and write
It is possible to use simple blocking functions to read from I2S input device or write to I2S output device.

`i2s_write(i2s, data, size)`

This function write user provided data to the output I2S device. It returns number of bytes written.
Return value will in most cases will be less then requested since data is written in internal buffer size chunks.

`i2s_read(i2s, data_buffer, size)`
Read data from microphone to user provided buffer. Function returns actual number of bytes read. Return value
may be less then size because it will most likely be truncated to internal buffer size. 

#### Sample buffers

I2S software device needs at least 2 buffers for seamless sample streaming.

`i2s_buffer_get()`

Returns a buffer with samples taken from the I2S input device.
For output device it will return a buffer that should be filled with samples by user code and then passed back to the device.

`i2s_buffer_put()`

Sends sample to output I2S device. For input device it simply returns buffer so it can be reused for next
incoming samples.

- For output device user code gets a buffers from the i2s device with `i2s_buffer_get()`, then fills the buffer with
new samples, then sends the buffer with samples back to the device using `i2s_buffer_put()`.
- For input device after user code starts sampling operation calling `i2s_start()` it must wait for collected samples.
Simplest way to wait is to call `i2s_buffer_get(i2s, OS_WAIT_FOREVER)` that will block till samples are available
and returns buffer full of data.
Once application does something with the samples it should immediately call `i2s_buffer_put()` to return buffer back
to the driver so next samples can be collected.
- It is possible to have I2S created without internal buffers. In that case user code can create buffers and pass
them to the driver using `i2s_buffer_put()`. Such buffers would then be available when processed by the driver and
could be taken back with `i2s_buffer_get()`. If user provided callback function `sample_buffer_ready_cb` is called
from interrupt context and return value other then 0, buffer will not be put in internal queue and can not be obtained
with `i2s_buffer_get()`.

Buffer pool can be created by `I2S_BUFFER_POOL_DEF()` macro


Definition ```I2S_BUFFER_POOL_DEF(my_pool, 2, 512);``` would create following structure in memory:

```

my_pool_buffers: +----------------------------+
                 | i2s_sample_buffer[0]       |<---------+
                 |               sample_data  |------+   |
                 + - - - - - - - - - - - - - -+      |   |
                 | i2s_sample_buffer[1]       |      |   |
            +----|               sample_data  |      |   |
            |    +----------------------------+      |   |
            |    | buffer0[0]                 |<-----+   |
            |    | ....                       |          |
            |    | buffer0[511]               |          |
            |    +----------------------------|          |
            +--->| buffer1[0]                 |          |
                 | ....                       |          |
                 | buffer1[511]               |          |
                 +----------------------------|          |
                                                         |
my_pool:         +----------------------------+          |
                 | buf_size: 512              |          |
                 | buf_count: 2               |          |
                 | buffers:                   |----------+
                 +----------------------------+
```
# Usage examples

#####1. Simple code that streams data to output I2S using own task

````c
int total_data_to_write = ....
uint8_t *data_ptr = ....

struct i2s *spkr;
int data_written;

spekr = i2s_open("speaker", OS_WAIT_FOREVER, NULL);

while (total_data_to_write) {
    data_written = i2s_write(spkr, data_ptr, total_data_to_write);
    total_data_to_write -= data_written;
    data_ptr += total_data_to_write;
}
i2s_close(spkr);
````
#####2. Blocking read from microphone example
````c
int total_data_to_read = ....
uint8_t *data_ptr = ....

struct i2s *mic;
int data_read;

spekr = i2s_open("mic", OS_WAIT_FOREVER, NULL);

while (total_data_to_read) {
    data_read = i2s_read(mic, data_ptr, total_data_to_read);
    total_data_to_read -= data_read;
    data_ptr += data_read;
}
i2s_close(spkr);
````
#####3. Reading without dedicated task
Example shows how to read I2S data stream without blocking calls.
````c

struct i2s mic;

void
buffer_ready_event_cb(struct os_event *ev)
{
    struct i2s_sample_buffer *buffer;

    /* This function is called so there is buffer ready. i2s_buffer_get() could be called
     * with timeout 0 */
    buffer = i2s_buffer_get(mic, OS_WAIT_FOREVER);
    /* Handle microphone data */
    ...
    /* Microphone samples processed, send buffer back to i2s device */
    i2s_buffer_put(mic, buffer);
}

struct os_event buffer_ready_event = {
    .ev_cb = buffer_ready_event_cb,
};

/* Function called from interrupt telling client that new buffer with samples is available */
static int
more_data(struct i2s *i2s, struct i2s_sample_buffer *sample_buffer)
{
    /* Handle incoming data in main task queue */
    os_eventq_put(os_eventq_dflt_get(), &buffer_ready_event);

    return 0;
}

static void state_changed(struct i2s *i2s, enum i2s_state state) {}

struct i2s_client client = {
    .sample_buffer_ready_cb = more_data,
    .state_changed_cb = state_changed,
};

void start_microphone(void)
{
    mic = i2s_open("mic", OS_WAIT_FOREVER, client);
    /* I2S input device is not started when device is opened */
    i2s_start(mic);
}
````