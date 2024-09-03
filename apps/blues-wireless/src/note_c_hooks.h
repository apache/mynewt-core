/**
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

#ifndef NOTE_C_HOOKS_H
#define NOTE_C_HOOKS_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

void platform_delay(uint32_t ms);
uint32_t platform_millis(void);

const char *note_i2c_receive(uint16_t device_address, uint8_t *buffer,
                             uint16_t size, uint32_t *available);
bool note_i2c_reset(uint16_t device_address);
const char *note_i2c_transmit(uint16_t device_address, uint8_t *buffer, uint16_t size);

size_t note_log_print(const char *message);

#endif /* NOTE_C_HOOKS_H */
