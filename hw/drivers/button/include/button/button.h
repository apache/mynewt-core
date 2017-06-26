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

#ifndef __BUTTON_H__
#define __BUTTON_H__

#include <os/os_dev.h>
#include <hal/hal_gpio.h>

struct button_dev;

typedef void (*button_notify_cb_t)(struct button_dev *, void *, uint8_t);

struct button_cfg {
    int bc_pin;
    hal_gpio_pull_t bc_pull;
    uint8_t bc_invert;
    uint32_t bc_debounce_time_us;
};

#define BUTTON_STATE_NOT_PRESSED (0)
#define BUTTON_STATE_PRESSED     (1)

struct button_dev {
    struct os_dev bd_dev;
    struct button_cfg *bd_cfg;
    struct hal_timer bd_timer;
    uint8_t bd_state;
    button_notify_cb_t bd_notify_cb;
    void *bd_notify_arg;
};

int button_init(struct os_dev *, void *);

int button_notify(struct button_dev *, button_notify_cb_t, void *);

uint8_t button_read(struct button_dev *);

#endif /* __BUTTON_H__ */
