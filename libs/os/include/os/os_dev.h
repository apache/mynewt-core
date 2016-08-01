/**
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

#ifndef _OS_DEV_H
#define _OS_DEV_H

#include <os/os.h>

#include "os/queue.h"

struct os_dev;

/*
 * Initialization order, defines when a device should be initialized
 * by the Mynewt kernel.
 *
 */
#define OS_DEV_INIT_KERNEL    (1)

#define OS_DEV_INIT_F_CRITICAL (1 << 0)


#define OS_DEV_INIT_PRIO_DEFAULT (0xff)

/**
 * Device status, so functions can ensure device is called in a
 * consistent state.
 */
#define OS_DEV_STATUS_BASE    (1 << 0)
#define OS_DEV_STATUS_INITING (1 << 1)
#define OS_DEV_STATUS_READY   (1 << 2)

typedef int (*os_dev_init_func_t)(struct os_dev *);
typedef int (*os_dev_open_func_t)(struct os_dev *);
typedef int (*os_dev_close_func_t)(struct os_dev *);

struct os_dev_handlers {
    os_dev_open_func_t od_open;
    os_dev_close_func_t od_close;
};

/*
 * Device structure.
 *
 */
struct os_dev {
    struct os_dev_handlers od_handlers;
    os_dev_init_func_t od_init;
    uint8_t od_stage;
    uint8_t od_priority;
    uint8_t od_init_flags;
    uint8_t od_status;
    char *od_name;
    STAILQ_ENTRY(os_dev) od_next;
};

#define OS_DEV_SETHANDLERS(__dev, __open, __close)          \
    (__dev)->od_handlers.od_open = (__open);                \
    (__dev)->od_handlers.od_close = (__close);

int os_dev_create(struct os_dev *dev, char *name, uint8_t stage,
        uint8_t priority, os_dev_init_func_t od_init);
int os_dev_initialize_all(uint8_t stage);
struct os_dev *os_dev_lookup(char *name);
int os_dev_open(struct os_dev *dev);
int os_dev_close(struct os_dev *dev);

#endif /* _OS_DEV_H */
