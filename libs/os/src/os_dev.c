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

#include "os/os.h"
#include "os/queue.h"
#include "os/os_dev.h"

#include <string.h>

static STAILQ_HEAD(, os_dev) g_os_dev_list =
    STAILQ_HEAD_INITIALIZER(g_os_dev_list);

static int
os_dev_init(struct os_dev *dev, char *name, uint8_t stage,
        uint8_t priority, os_dev_init_func_t od_init, void *arg)
{
    dev->od_name = name;
    dev->od_stage = stage;
    dev->od_priority = priority;
    /* assume these are set after the fact. */
    dev->od_init_flags = 0;
    dev->od_init = od_init;
    memset(&dev->od_handlers, 0, sizeof(dev->od_handlers));

    return (0);
}

/**
 * Add the device to the device tree.  This is a private function.
 *
 * @param dev The device to add to the device tree.
 *
 * @return 0 on success, non-zero on failure.
 */
static int
os_dev_add(struct os_dev *dev)
{
    struct os_dev *cur_dev;

    /* If no devices present, insert into head */
    if (STAILQ_FIRST(&g_os_dev_list) == NULL) {
        STAILQ_INSERT_HEAD(&g_os_dev_list, dev, od_next);
        return (0);
    }

    /* Add devices to the list, sorted first by stage, then by
     * priority.  Keep sorted in this order for initialization
     * stage.
     */
    cur_dev = NULL;
    STAILQ_FOREACH(cur_dev, &g_os_dev_list, od_next) {
        if (cur_dev->od_stage > dev->od_stage) {
            continue;
        }

        if (dev->od_priority >= cur_dev->od_priority) {
            break;
        }
    }

    if (cur_dev) {
        STAILQ_INSERT_AFTER(&g_os_dev_list, cur_dev, dev, od_next);
    } else {
        STAILQ_INSERT_TAIL(&g_os_dev_list, dev, od_next);
    }

    return (0);
}


/**
 * Create a new device in the kernel.
 *
 * @param dev The device to create.
 * @param name The name of the device to create.
 * @param stage The stage to initialize that device to.
 * @param priority The priority of initializing that device
 * @param od_init The initialization function to call for this
 *                device.
 * @param arg The argument to provide this device initialization
 *            function.
 *
 * @return 0 on success, non-zero on failure.
 */
int
os_dev_create(struct os_dev *dev, char *name, uint8_t stage,
        uint8_t priority, os_dev_init_func_t od_init, void *arg)
{
    int rc;

    rc = os_dev_init(dev, name, stage, priority, od_init, arg);
    if (rc != 0) {
        goto err;
    }

    rc = os_dev_add(dev);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

/**
 * Initialize all devices for a given state.
 *
 * @param stage The stage to initialize.
 *
 * @return 0 on success, non-zero on failure.
 */
int
os_dev_initialize_all(uint8_t stage)
{
    struct os_dev *dev;
    int rc;

    STAILQ_FOREACH(dev, &g_os_dev_list, od_next) {
        if (dev->od_stage == stage) {
            rc = dev->od_init(dev, dev->od_init_arg);
            if (rc != 0) {
                if (dev->od_init_flags & OS_DEV_INIT_F_CRITICAL) {
                    goto err;
                }
            } else {
                dev->od_status |= OS_DEV_STATUS_READY;
            }
        }
    }

    return (0);
err:
    return (rc);
}

/**
 * Lookup a device by name, internal function only.
 *
 * @param name The name of the device to look up.
 *
 * @return A pointer to the device corresponding to name, or NULL if not found.
 */
static struct os_dev *
os_dev_lookup(char *name)
{
    struct os_dev *dev;

    dev = NULL;
    STAILQ_FOREACH(dev, &g_os_dev_list, od_next) {
        if (!strcmp(dev->od_name, name)) {
            break;
        }
    }
    return (dev);
}

/**
 * Open a device.
 *
 * @param dev The device to open
 * @param timo The timeout to open the device, if not specified.
 * @param arg The argument to the device open() call.
 *
 * @return 0 on success, non-zero on failure.
 */
struct os_dev *
os_dev_open(char *devname, uint32_t timo, void *arg)
{
    struct os_dev *dev;
    int rc;

    dev = os_dev_lookup(devname);
    if (dev == NULL) {
        return (NULL);
    }

    /* Device is not ready to be opened. */
    if ((dev->od_status & OS_DEV_STATUS_READY) == 0) {
        return (NULL);
    }

    if (dev->od_handlers.od_open) {
        rc = dev->od_handlers.od_open(dev, timo, arg);
        if (rc != 0) {
            goto err;
        }
    }

    return (dev);
err:
    return (NULL);
}

/**
 * Close a device.
 *
 * @param dev The device to close
 *
 * @return 0 on success, non-zero on failure.
 */
int
os_dev_close(struct os_dev *dev)
{
    int rc;

    if (dev->od_handlers.od_close) {
        rc = dev->od_handlers.od_close(dev);
        if (rc != 0) {
            goto err;
        }
    }

    return (0);
err:
    return (rc);
}

