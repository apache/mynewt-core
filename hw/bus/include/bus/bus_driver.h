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

#ifndef HW_BUS_DRIVER_H_
#define HW_BUS_DRIVER_H_

#include <stdint.h>
#include "os/mynewt.h"
#if MYNEWT_VAL(BUS_STATS)
#include "stats/stats.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct bus_dev;
struct bus_node;

#if MYNEWT_VAL(BUS_STATS)
STATS_SECT_START(bus_stats_section)
    STATS_SECT_ENTRY(lock_timeouts)
    STATS_SECT_ENTRY(read_ops)
    STATS_SECT_ENTRY(read_errors)
    STATS_SECT_ENTRY(write_ops)
    STATS_SECT_ENTRY(write_errors)
STATS_SECT_END
#endif

/**
 * Bus device operations
 *
 * These operations shall be defined by bus driver
 */
struct bus_dev_ops {
    /* Initialize node */
    int (* init_node)(struct bus_dev *bus, struct bus_node *node, void *arg);
    /* Enable bus device */
    int (* enable)(struct bus_dev *bus);
    /* Configure bus for node */
    int (* configure)(struct bus_dev *bus, struct bus_node *node);
    /* Read data from node */
    int (* read)(struct bus_dev *dev, struct bus_node *node, uint8_t *buf,
                 uint16_t length, os_time_t timeout, uint16_t flags);
    /* Write data to node */
    int (* write)(struct bus_dev *dev, struct bus_node *node, const uint8_t *buf,
                  uint16_t length, os_time_t timeout,  uint16_t flags);
    /* Disable bus device */
    int (* disable)(struct bus_dev *bus);
};

/**
 * Bus node callbacks
 *
 * Node can use these callbacks to receive notifications from bus driver.
 * All callbacks are optional.
 */
struct bus_node_callbacks {
    /* Called when os_dev is initialized */
    void (* init)(struct bus_node *node, void *arg);
    /* Called when first reference to node object is opened */
    void (* open)(struct bus_node *node);
    /* Called when last reference to node object is closed */
    void (* close)(struct bus_node *node);
};

/**
 * Bus node configuration
 *
 * This can be wrapped in configuration structure defined by bus driver.
 */
struct bus_node_cfg {
    /** Bus device name where node is attached */
    const char *bus_name;
    /** Lock timeout [ms], 0 = default timeout */
    uint16_t lock_timeout_ms;
};

/**
 * Bus device object state
 *
 * Contents of these objects are managed internally by bus driver and shall not
 * be accessed directly.
 */
struct bus_dev {
    struct os_dev odev;
    const struct bus_dev_ops *dops;

    struct os_mutex lock;
    struct bus_node *configured_for;

#if MYNEWT_VAL(BUS_PM)
    bus_pm_mode_t pm_mode;
    union bus_pm_options pm_opts;
    struct os_callout inactivity_tmo;
#endif

#if MYNEWT_VAL(BUS_STATS)
    STATS_SECT_DECL(bus_stats_section) stats;
#endif

    bool enabled;

#if MYNEWT_VAL(BUS_DEBUG_OS_DEV)
    uint32_t devmagic;
#endif
};

/**
 * Bus node object state
 *
 * Contents of these objects are managed internally by bus driver and shall not
 * be accessed directly.
 */
struct bus_node {
    struct os_dev odev;
    struct bus_node_callbacks callbacks;

    /*
     * init_arg is valid until os_dev is initialized (bus_node_init_func called)
     * parent_bus is valid afterwards
     */
    union {
        struct bus_dev *parent_bus;
        void *init_arg;
    };

    os_time_t lock_timeout;

#if MYNEWT_VAL(BUS_STATS_PER_NODE)
    STATS_SECT_DECL(bus_stats_section) stats;
#endif

#if MYNEWT_VAL(BUS_DEBUG_OS_DEV)
    uint32_t nodemagic;
#endif
};

/**
 * Initialize os_dev as bus device
 *
 * This can be passed as a parameter to os_dev_create() when creating os_dev
 * object for bus, however it's recommended to create devices using specialized
 * APIs provided by bus drivers.
 *
 * @param bus  Bus device object
 * @param arg  Bus device operations struct (struct bus_dev_ops)
 */
int
bus_dev_init_func(struct os_dev *bus, void *arg);

/**
 * Initialize os_dev as bus node
 *
 * This can be passed as a parameter to os_dev_create() when creating os_dev
 * object for node, however it's recommended to create devices using specialized
 * APIs provided by bus drivers.
 *
 * @param node  Node device object
 * @param arg   Node configuration struct (struct bus_node_cfg)
 */
int
bus_node_init_func(struct os_dev *node, void *arg);

/**
 * Set driver callbacks for node
 *
 * This should be used before node device is initialized to set callbacks for
 * receiving notifications from bus driver. It shall be called no more than once
 * on any bus_node object.
 *
 * @param node  Node device object;
 * @param cbs   Callbacks
 *
 */
void
bus_node_set_callbacks(struct os_dev *node, struct bus_node_callbacks *cbs);

#ifdef __cplusplus
}
#endif

#endif /* HW_BUS_DRIVER_H_ */
