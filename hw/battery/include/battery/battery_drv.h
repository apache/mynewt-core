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

/**
 * @file battery.h
 * @brief Hardware-agnostic interface for fuel gauge ICs
 *
 * The Batter interface provides a hardware-agnostic layer for reading
 * fuel gauge controller ICs.
 */

#ifndef __BATTERY_DRV_H__
#define __BATTERY_DRV_H__

#include "os/mynewt.h"
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
#include "bus/drivers/i2c_common.h"
#include "bus/drivers/spi_common.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Package init function.  Remove when we have post-kernel init stages.
 */
void battery_pkg_init(void);


/* Forward declaration of battery structure. */
struct battery;
struct battery_driver;
struct battery_property;

/* Comments above intentionally not in Doxygen format */

/* =================================================================
 * ====================== DEFINES/MACROS ===========================
 * =================================================================
 */

/**
 * @{ Fuel Gauge API
 */


/* ---------------------- DRIVER FUNCTIONS ------------------------- */

/**
 * Set the driver functions for this battery, along with the type of
 * data available for the given battery.
 *
 * @param The battery to set the driver information for
 * @param The battery driver
 * @param The battery properties provided by driver
 *
 * @return 0 on success, non-zero error code on failure
 */
int battery_add_driver(struct os_dev *battery, struct battery_driver *driver);

/**
 * Get value of battery property
 * (e.g. BATTERY_PROP_STATUS).
 *
 * @param The battery to get property from
 * @param The property to read. The type of property should have type
 *        supported by fuel gauge. Data filed in property will be filled
 *        by the driver.
 * @param Timeout. If block until result, specify OS_TIMEOUT_NEVER, 0 returns
 *        immediately (no wait.)
 *
 * @return 0 on success, non-zero error code on failure.
 */
typedef int (*battery_property_get_func_t)(struct battery_driver *driver,
        struct battery_property *property, uint32_t timeout);

/**
 * Set value of battery property
 * Setting value is supported for certain values only.
 * Usually threshold properties can be set.
 *
 * @param The desired battery
 * @param The new value of battery property, type must be known to driver
 *        and data files should be set to correct value.
 *
 * @return 0 on success, non-zero error code on failure.
 */
typedef int (*battery_property_set_func_t)(struct battery_driver *driver,
        struct battery_property *property);

/**
 * Enable a fuel gauge
 *
 * @param The battery to enable fuel gauge functionality
 *
 * @return 0 on success, non-zero error code on failure.
 */
typedef int (*battery_enable_func_t)(struct battery *battery);

/**
 * Disable a fuel gauge
 *
 * @param The battery to disable fuel gauge
 *
 * @return 0 on success, non-zero error code on failure
 */
typedef int (*battery_disable_func_t)(struct battery *battery);

/**
 * Let driver handle interrupt in the battery manager context
 *
 * @param The battery.
 * @param Interrupt argument
 *
 * @return 0 on success, non-zero error code on failure.
 */
typedef int (*battery_handle_interrupt_t)(struct battery *battery, void *arg);

/**
 * Pointers to battery-specific driver functions.
 */
struct battery_driver_functions {
    battery_property_get_func_t  bdf_property_get;
    battery_property_set_func_t  bdf_property_set;

    battery_enable_func_t        bdf_enable;
    battery_disable_func_t       bdf_disable;
};

struct battery_driver {
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    /* Underlying bus node */
    union {
        struct os_dev dev;
        struct bus_i2c_node i2c_node;
        struct bus_spi_node spi_node;
    };
#else
    /* Underlying OS device */
    struct os_dev dev;
#endif

    const struct battery_driver_functions *bd_funcs;
    /* Array of properties supported by this battery */
    const struct battery_driver_property  *bd_driver_properties;
    void                                  *bd_driver_data;

    /*
     * Fields below are managed by battery manager, they don't need
     * to be filled by the driver.
     */

    /* Number of elements in b_driver_properties array
     * Battery manager will fill this field using data from
     * bd_driver_properties field
     */
    uint8_t                                bd_property_count;
    uint8_t                                bd_first_property;
};

/**
 * }@
 */

/* End Battery Manager API */


/**
 * @}
 */

/* End Battery API */

#ifdef __cplusplus
}
#endif

#endif /* __BATTERY_DRV_H__ */
