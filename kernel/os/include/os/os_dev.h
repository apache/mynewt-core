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

#ifndef _OS_DEV_H
#define _OS_DEV_H

/**
 * @addtogroup OSKernel
 * @{
 *   @defgroup OSDevice Device Framework
 *   @{
 */

#include <os/os.h>

#include "os/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

struct os_dev;

/*
 * Initialization order, defines when a device should be initialized
 * by the Mynewt kernel.
 *
 */
/* Primary is initialized during OS init, after the initialization
 * of OS memory and architecture specific functions, but before the
 * OS gets started.
 */
#define OS_DEV_INIT_PRIMARY   (1)
/** Secondary is initialized directly after primary. */
#define OS_DEV_INIT_SECONDARY (2)
/** Initialize device in the main task, after the kernel has started. */
#define OS_DEV_INIT_KERNEL    (3)

/**
 * This device initializing is critical, fail device init if it does
 * not successfully initialize.
 */
#define OS_DEV_INIT_F_CRITICAL (1 << 0)

#define OS_DEV_INIT_PRIO_DEFAULT (0xff)

/** Device is initialized, and ready to be accessed. */
#define OS_DEV_F_STATUS_READY     (1 << 0)
/** Device is open */
#define OS_DEV_F_STATUS_OPEN      (1 << 1)
/** Device is in suspended state. */
#define OS_DEV_F_STATUS_SUSPENDED (1 << 2)
/**
 * It is critical to the system operation that this device successfully
 * initialized.  Fail device init if it does not.
 */
#define OS_DEV_F_INIT_CRITICAL    (1 << 3)

/**
 * Initialize a device.
 *
 * @param dev The device to initialize.
 * @param arg User defined argument to pass to the device initalization
 *
 * @return 0 on success, non-zero error code on failure.
 */
typedef int (*os_dev_init_func_t)(struct os_dev *, void *);

typedef int (*os_dev_open_func_t)(struct os_dev *, uint32_t,
        void *);
typedef int (*os_dev_suspend_func_t)(struct os_dev *, os_time_t, int);
typedef int (*os_dev_resume_func_t)(struct os_dev *);
typedef int (*os_dev_close_func_t)(struct os_dev *);

/**
 * Device handlers, implementers of device drivers should fill these
 * out to control device operation.
 */
struct os_dev_handlers {
    /**
     * Device open handler, called when the user opens the device.
     * Any locking of the device should be done within the open handler.
     */
    os_dev_open_func_t od_open;
    /**
     * Suspend handler, called when the device is being suspended.
     * Up to the implementer to save device state before power down,
     * so that the device can be cleanly resumed -- or error out and
     * delay suspension.
     */
    os_dev_suspend_func_t od_suspend;
    /**
     * Resume handler, restores device state after a suspend operation.
     */
    os_dev_resume_func_t od_resume;
    /**
     * Close handler, releases the device, including any locks that
     * may have been taken by open().
     */
    os_dev_close_func_t od_close;
};

/*
 * Device structure.
 */
struct os_dev {
    /** Device handlers.  Implementation of base device functions. */
    struct os_dev_handlers od_handlers;
    /** Device initialization function. */
    os_dev_init_func_t od_init;
    /** Argument to pass to device initialization function. */
    void *od_init_arg;
    /** Stage during which to initialize this device. */
    uint8_t od_stage;
    /** Priority within a given stage to initialize a device. */
    uint8_t od_priority;
    /**
     * Number of references to a device being open before marking
     * the device closed.
     */
    uint8_t od_open_ref;
    /** Device flags.  */
    uint8_t od_flags;
    /** Device name */
    char *od_name;
    STAILQ_ENTRY(os_dev) od_next;
};

#define OS_DEV_SETHANDLERS(__dev, __open, __close)          \
    (__dev)->od_handlers.od_open = (__open);                \
    (__dev)->od_handlers.od_close = (__close);


/**
 * Suspend the operation of the device.
 *
 * @param dev The device to suspend.
 * @param suspend_t When the device should be suspended.
 * @param force Whether not the suspend operation can be overridden by the
 *        device handler.
 *
 * @return 0 on success, non-zero error code on failure.
 */
static inline int
os_dev_suspend(struct os_dev *dev, os_time_t suspend_t, uint8_t force)
{
    if (dev->od_handlers.od_suspend == NULL) {
        return (0);
    } else {
        return (dev->od_handlers.od_suspend(dev, suspend_t, force));
    }
}


/**
 * Resume the device operation.
 *
 * @param dev The device to resume
 *
 * @return 0 on success, non-zero error code on failure.
 */
static inline int
os_dev_resume(struct os_dev *dev)
{
    if (dev->od_handlers.od_resume == NULL) {
        return (0);
    } else {
        return (dev->od_handlers.od_resume(dev));
    }
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
int os_dev_create(struct os_dev *dev, char *name, uint8_t stage,
        uint8_t priority, os_dev_init_func_t od_init, void *arg);

/**
 * Lookup a device by name.
 *
 * WARNING: This should be called before any locking on the device is done, or
 * the device list itself is modified in any context.  There is no locking.
 *
 * @param name The name of the device to look up.
 *
 * @return A pointer to the device corresponding to name, or NULL if not found.
 */
struct os_dev *os_dev_lookup(char *name);

/**
 * Initialize all devices for a given state.
 *
 * @param stage The stage to initialize.
 *
 * @return 0 on success, non-zero on failure.
 */
int os_dev_initialize_all(uint8_t);


/**
 * Suspend all devices.
 *
 * @param suspend_t The number of ticks to suspend this device for
 * @param force Whether or not to force suspending the device
 *
 * @return 0 on success, or a non-zero error code if one of the devices
 *                       returned it.
 */
int os_dev_suspend_all(os_time_t, uint8_t);

/**
 * Resume all the devices that were suspended.
 *
 * @return 0 on success, -1 if any of the devices have failed to resume.
 */
int os_dev_resume_all(void);

/**
 * Open a device.
 *
 * @param dev The device to open
 * @param timo The timeout to open the device, if not specified.
 * @param arg The argument to the device open() call.
 *
 * @return 0 on success, non-zero on failure.
 */
struct os_dev *os_dev_open(char *devname, uint32_t timo, void *arg);

/**
 * Close a device.
 *
 * @param dev The device to close
 *
 * @return 0 on success, non-zero on failure.
 */
int os_dev_close(struct os_dev *dev);


/**
 * Clears the device list.  This function does not close any devices or free
 * any resources; its purpose is to allow a full system reset between unit
 * tests.
 */
void os_dev_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* _OS_DEV_H */


/**
 *   @} OSDevice
 * @} OSKernel
 */
