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

#ifndef __BATTERY_H__
#define __BATTERY_H__

#include "os/mynewt.h"
#include "battery/battery_prop.h"

#ifdef __cplusplus
extern "C" {
#endif

#if MYNEWT_VAL(BATTERY_DRIVERS_MAX)
#define BATTERY_DRIVERS_MAX MYNEWT_VAL(BATTERY_DRIVERS_MAX)
#else
#define BATTERY_DRIVERS_MAX 2
#endif

/* Package init function.  Remove when we have post-kernel init stages.
 */
void battery_pkg_init(void);


/* Forward declaration of battery structure. */
struct battery;
struct battery_driver;
struct battery_property;
struct battery_driver_data;

struct battery_init_cfg {
    /* Empty structure for future use */
};

struct battery_open_arg {
    const char **devices;
};

/* Comments above intentionally not in Doxygen format */

/* =================================================================
 * ====================== DEFINES/MACROS ===========================
 * =================================================================
 */

/**
 * @{ Fuel Gauge API
 */

// ------------------------ BATTERY OBJECT -------------------------

/* Private struct */
struct listener_data;

struct battery {
    /* The OS device this battery inherits from, this is typically a
     * fuel gauge specific driver.
     */
    struct os_dev b_dev;
    /*
     * Driver data, set by driver init function
     */
    struct battery_driver *b_drivers[BATTERY_DRIVERS_MAX];

    /*
     * Battery manager managed fields
     */

    /* The lock for battery object */
    struct os_mutex b_lock;

    /* All propertied are numbered for battery manager internal use */
    uint8_t b_all_property_count;

    /* Number of registered listeners */
    uint8_t b_listener_count;

    /* Array of properties created by battery manager
     * for alarms that are not fully supported by hardware.
     * Filled by battery manager.
     */
    struct battery_property *b_properties;

    /* Bitmask of properties which needs polling (for poll and change
     * notifications).
     */
    uint32_t b_polled_properties[BATTERY_PROPERTY_MASK_SIZE];

    /**
     * Poll rate in ms for this battery.
     * Field managed by battery manager.
     */
    uint32_t b_poll_rate;

    /* The next time at which we will poll data from this fuel gauge
     * Field managed by battery manager.
     */
    os_time_t b_next_run;

    /* Battery last reading time stamp. */
    os_time_t b_last_read_time;

    /* A list of listeners that are registered to receive data from this
     * battery.
     */
    struct listener_data *b_listeners;
};

/* =================================================================
 * ========================== BATTERY ==============================
 * =================================================================
 */

/**
 * Initialize battery structure data and mutex and associate it with an
 * os_dev.
 *
 * @param os_dev to associate with the battery struct
 * @param Battery struct to initialize
 *
 * @return 0 on success, non-zero error code on failure.
 */
int battery_init(struct os_dev *dev, void *cfg);

/**
 * Find battery property or create one
 *
 * @param The battery to get property from
 * @param The flags to select property
 * @param The battery property flags, if BATTERY_PROPERY_CREATE
 *        is specified property can be created if not existed before.
 * @param The driver name to get property from, this parameter is optional
 *        and can be NULL.
 *
 * @return NULL if property could not be found
 */
struct battery_property *battery_find_property(struct os_dev *battery,
        battery_property_type_t type, battery_property_flags_t flags,
        const char *dev_name);

/**
 * Find battery property by name
 *
 * @param The battery to get property from
 * @param The name of property
 *
 * @return NULL if property could not be found
 */
struct battery_property *battery_find_property_by_name(struct os_dev *battery,
        const char *name);

/**
 * Returns battery property count
 *
 * @param The battery to get property count from
 * @param Battery driver to get property from, can be NULL.
 *
 * @return number of properties register to battery
 */
int battery_get_property_count(struct os_dev *battery,
        struct battery_driver *driver);

/**
 * Returns battery property specified by number
 * It can be used to enumerate properties.
 *
 * @param The battery to enum property from
 * @param Battery driver to get property from, can be NULL.
 * @param Index of property, should be less that value returned
 *        by bettery_get_property_couunt
 *
 * @return pointer to property for valid index, or NULL if prop_num is to high
 */
struct battery_property *
battery_enum_property(struct os_dev *battery, struct battery_driver *driver,
        uint8_t prop_num);

/**
 * Gets battery property name
 *
 * @param The battery property
 * @param Buffer for property name
 * @param Size of the buffer for property
 *
 * @return pointer buffer for easy usage in printf like functions
 */
char *battery_prop_get_name(const struct battery_property *prop, char *buf,
        size_t buf_size);

/**
 * Set the battery poll rate
 *
 * @param The battery
 * @param The poll rate in milliseconds
 *
 * @return 0 on success, non-zero error code on failure.
 */
int battery_set_poll_rate_ms(struct os_dev *battery, uint32_t poll_rate);

/**
 * Set the battery poll rate with a starting delay
 *
 * @param The battery
 * @param The poll rate in milliseconds
 * @param Start delay before beginning to poll in milliseconds
 *
 * @return 0 on success, non-zero error code on failure.
 */
int battery_set_poll_rate_ms_delay(struct os_dev *battery, uint32_t poll_rate,
        uint32_t start_delay);

// =================================================================
// ========================== BATTERY MANAGER ======================
// =================================================================

/**
 * @{ Battery Manager API
 */

/**
 * Registers a battery with the battery manager.
 * This function should be called from driver init function.
 *
 * @param The battery to register
 *
 * @return 0 on success, non-zero error code on failure.
 */
int battery_mgr_register_battery(struct os_dev *battery);

/**
 * Search the battery list and find the next battery that
 * corresponds to a given device name.
 *
 * @param The battery to search for, if NULL first registered
 *        battery is returned.
 *
 * @return battery pointer, NULL if such battery is not registered
 */
struct os_dev *battery_mgr_find_by_name(const char *name);

/**
 * Add interrupt from driver so it can be processed in user context.
 *
 * @param The event passed from driver to be processed in battery
 *        manager task.
 */
void battery_mgr_process_event(struct os_event *event);

/**
 * Returns number of batteries in the system.
 *
 * @return number of batteries.
 */
int battery_mgr_get_battery_count(void);

/**
 * Get battery by its number
 */
struct os_dev *battery_get_battery(int bat_num);

/**
 * Returns number of battery driver for battery instance
 *
 * There may be several drivers serving battery information (fuel gauge, adc),
 * they can provide separate properties for same information like voltage.
 * This function returns number of configured drivers.
 *
 * @return number of drivers for battery.
 */
int battery_get_driver_count(struct os_dev *battery);

/**
 * Returns battery driver specified by it device name.
 *
 * @return battery driver.
 */
struct battery_driver *get_battery_driver(struct os_dev *battery,
        const char *dev_name);

#if MYNEWT_VAL(BATTERY_SHELL)
void battery_shell_register(void);
#endif

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

#endif /* __BATTERY_H__ */
