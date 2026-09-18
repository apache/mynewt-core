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

#ifndef _MN_PIN_H
#define _MN_PIN_H

#include <stdint.h>

#ifdef __cplusplus
// extern "C" {
#endif

typedef struct mn_pin mn_pin_t;
typedef struct mn_pin_ops mn_pin_ops_t;

struct mn_pin_ops {
    void (*init)(const mn_pin_t *pin);
    void (*deinit)(const mn_pin_t *pin);
    uint8_t (*read)(const mn_pin_t *pin);
    void (*write)(const mn_pin_t *pin, uint8_t data);
    void (*set)(const mn_pin_t *pin);
    void (*clear)(const mn_pin_t *pin);
};

static inline void
mn_pin_init(const mn_pin_t *pin)
{
    pin->ops->init(pin);
}

static inline void
mn_pin_deinit(const mn_pin_t *pin)
{
    pin->ops->deinit(pin);
}

static inline uint8_t
mn_pin_read(const mn_pin_t *pin)
{
    return pin->ops->read(pin);
}

static inline void
mn_pin_write(const mn_pin_t *pin, uint8_t data)
{
    pin->ops->write(pin, data);
}

static inline void
mn_pin_set(const mn_pin_t *pin)
{
    pin->ops->set(pin);
}

static inline void
mn_pin_clear(const mn_pin_t *pin)
{
    pin->ops->clear(pin);
}



#ifdef __cplusplus
//}
#endif

#endif /* _MN_PIN_H */
