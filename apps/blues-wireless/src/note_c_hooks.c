/**
 #
 # Licensed to the Apache Software Foundation (ASF) under one
 # or more contributor license agreements.  See the NOTICE file
 # distributed with this work for additional information
 # regarding copyright ownership.  The ASF licenses this file
 # to you under the Apache License, Version 2.0 (the
 # "License"); you may not use this file except in compliance
 # with the License.  You may obtain a copy of the License at
 #
 #  http://www.apache.org/licenses/LICENSE-2.0
 #
 # Unless required by applicable law or agreed to in writing,
 # software distributed under the License is distributed on an
 # "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 # KIND, either express or implied.  See the License for the
 # specific language governing permissions and limitations
 # under the License.
 #
 */


#include "note_c_hooks.h"
#include "bsp/bsp.h"
#include "hal/hal_i2c.h"
#include "os/os_time.h"
#include <console/console.h>

static const size_t REQUEST_HEADER_SIZE = 2;

const uint8_t i2c_num = 0;
bool i2c_initialized = false;

uint32_t
platform_millis(void)
{
    return (uint32_t)(os_get_uptime_usec()/1000);
}

void
platform_delay(uint32_t ms)
{
    os_time_t d_time;
    d_time = os_time_ms_to_ticks32(ms);
    os_time_delay(d_time);
}

const char *
note_i2c_receive(uint16_t device_address, uint8_t *buffer,
                 uint16_t size, uint32_t *available)
{
    uint8_t size_buf[2];
    size_buf[0] = 0;
    size_buf[1] = (uint8_t)size;

    struct hal_i2c_master_data data = {
        .address = device_address,
        .len = sizeof(size_buf),
        .buffer = size_buf,
    };

    uint8_t write_result = hal_i2c_master_write(i2c_num, &data, OS_TICKS_PER_SEC / 10, 0);

    if (write_result != 0) {
        return "i2c: unable to initiate read from the notecard\n";
    }
    /*
       Read from the Notecard and copy the response bytes into the
       response buffer
     */
    const int request_length = size + REQUEST_HEADER_SIZE;
    uint8_t read_buf[256];

    data.len = request_length;
    data.buffer = read_buf;
    int8_t read_result = hal_i2c_master_read(i2c_num, &data, OS_TICKS_PER_SEC / 10, 1);

    if (read_result != 0) {
        return "i2c: Unable to receive data from the Notecard.\n";
    } else {
        *available = (uint32_t)read_buf[0];
        uint8_t bytes_to_read = read_buf[1];
        for (size_t i = 0; i < bytes_to_read; i++) {
            buffer[i] = read_buf[i + 2];
        }
        return NULL;
    }
}

bool
note_i2c_reset(uint16_t device_address)
{
    (void)device_address;

    if (i2c_initialized) {
        return true;
    }
    if (hal_i2c_enable(i2c_num) != 0) {
        console_printf("i2c: Device not ready.\n");
        return false;
    }
    console_printf("i2c: Device is ready.\n");
    i2c_initialized = true;
    return true;
}

const char *
note_i2c_transmit(uint16_t device_address, uint8_t *buffer,
                  uint16_t size)
{
    uint8_t write_buf[size + 1];
    write_buf[0] = (uint8_t)size;
    for (size_t i = 0; i < size; i++) {
        write_buf[i + 1] = buffer[i];
    }

    struct hal_i2c_master_data data = {
        .address = device_address,
        .len = size + 1,
        .buffer = write_buf,
    };

    uint8_t write_result = hal_i2c_master_write(i2c_num, &data, OS_TICKS_PER_SEC / 5, 1);

    if (write_result != 0) {
        return "i2c: Unable to transmit data to the Notecard\n";
    } else {
        return NULL;
    }
}

size_t
note_log_print(const char *message)
{
    if (message) {
        console_printf("%s", message);
        return 1;
    }
    return 0;
}
