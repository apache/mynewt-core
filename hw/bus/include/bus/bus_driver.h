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

#ifdef __cplusplus
extern "C" {
#endif

struct bus_dev;
struct bus_node;

/**
 * Bus device operations
 *
 * These operations shall be defined by bus driver
 */
struct bus_dev_ops {
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

#if MYNEWT_VAL(BUS_DEBUG)
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
    struct bus_dev *parent_bus;

#if MYNEWT_VAL(BUS_DEBUG)
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
