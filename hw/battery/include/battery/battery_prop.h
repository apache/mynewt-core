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
 * @brief Battery property
 *
 * The Batter property provides software view to data provided by fuel gauge
 * ADC or charge controllers.
 */

#ifndef __BATTERY_PROP_H__
#define __BATTERY_PROP_H__

#include "os/mynewt.h"

#ifdef __cplusplus
extern "C" {
#endif

#if MYNEWT_VAL(BATTERY_MAX_PROPERTY_COUNT)
#define BATTERY_MAX_PROPERTY_COUNT MYNEWT_VAL(BATTERY_MAX_PROPERTY_COUNT)
#else
#define BATTERY_MAX_PROPERTY_COUNT 32
#endif

#define BATTERY_PROPERTY_MASK_SIZE (((BATTERY_MAX_PROPERTY_COUNT) + 31) / 32)

/* Forward declaration of battery structure. */
struct battery;
struct battery_property;
struct battery_prop_listener;

/* Comments above intentionally not in Doxygen format */

/* =================================================================
 * ====================== DEFINES/MACROS ===========================
 * =================================================================
 */

/**
 * @{ Battery properties API
 */

// ---------------------- TYPE -------------------------------------

typedef enum {
    BATTERY_STATUS_UNKNOWN = 0,
    /* Charger connected, battery charging */
    BATTERY_STATUS_CHARGING,
    /* Charger not connected, battery discharging */
    BATTERY_STATUS_DISCHARGING,
    /* Charger connected, not charging */
    BATTERY_STATUS_NOT_CHARGING,
    /* Charger connected, not charging - battery full */
    BATTERY_STATUS_FULL,
} battery_status_t;

typedef enum {
    BATTERY_CAPACITY_LEVEL_UNKNOWN = 0,
    BATTERY_CAPACITY_LEVEL_CRITICAL,
    BATTERY_CAPACITY_LEVEL_LOW,
    BATTERY_CAPACITY_LEVEL_NORMAL,
    BATTERY_CAPACITY_LEVEL_HIGH,
    BATTERY_CAPACITY_LEVEL_FULL,
} battery_capacity_level_t;

typedef union battery_property_value {
    float bpv_flt;
    uint32_t bpv_u32;
    int32_t bpv_i32;
    uint16_t bpv_u16;
    int16_t bpv_i16;
    uint8_t bpv_u8;
    int8_t bpv_i8;
    /* in mV */
    int32_t bpv_voltage;
    /* in mA */
    int32_t bpv_current;
    /* in mAh */
    uint32_t bpv_capacity;
    /* SOC in % 0..100 */
    uint8_t bpv_soc;
    /* SOH in % 0..100 */
    uint8_t bpv_soh;
    /* Temperature in deg C */
    float bpv_temperature;
    /* Time in s */
    uint32_t bpv_time_in_s;
    /* Number of charge cycles */
    uint16_t bpv_cycle_count;
    battery_status_t bpv_status;
    battery_capacity_level_t bpv_capacity_level;
    uint8_t bpv_base_prop[4];
} battery_property_value_t;

/**
 * Battery properties.
 * Fuel gauge can exposes subset of those properties.
 */
typedef enum {
    BATTERY_PROP_NONE,
    /* Battery status supported */
    BATTERY_PROP_STATUS,
    /* Battery capacity level supported */
    BATTERY_PROP_CAPACITY_LEVEL,
    /* Battery capacity in mAh */
    BATTERY_PROP_CAPACITY,
    /* Predicted full battery capacity in mAh */
    BATTERY_PROP_CAPACITY_FULL,
    /* Current battery temperature in C */
    BATTERY_PROP_TEMP_NOW,
    /* Ambient temperature in C */
    BATTERY_PROP_TEMP_AMBIENT,
    /* Minimum voltage in V */
    BATTERY_PROP_VOLTAGE_MIN,
    /* Maximum voltage in V */
    BATTERY_PROP_VOLTAGE_MAX,
    /* Minimum designed voltage in V */
    BATTERY_PROP_VOLTAGE_MIN_DESIGN,
    /* Maximum designed voltage in V */
    BATTERY_PROP_VOLTAGE_MAX_DESIGN,
    /* Current voltage in V */
    BATTERY_PROP_VOLTAGE_NOW,
    /* Current average voltage in V */
    BATTERY_PROP_VOLTAGE_AVG,
    /* Maximum current in mAh */
    BATTERY_PROP_CURRENT_MAX,
    /* Current level at this time in mAh */
    BATTERY_PROP_CURRENT_NOW,
    /* Average current level in mAh */
    BATTERY_PROP_CURRENT_AVG,
    /* State-Of-Charge, current capacity 0-100 % */
    BATTERY_PROP_SOC,
    /* State-Of-Health, current battery state of health 0-100 % */
    BATTERY_PROP_SOH,
    /* Predicted time to complete discharge in seconds */
    BATTERY_PROP_TIME_TO_EMPTY_NOW,
    /* Predicted time to full capacity when charging in seconds */
    BATTERY_PROP_TIME_TO_FULL_NOW,
    /* Number of full discharge/charge cycles */
    BATTERY_PROP_CYCLE_COUNT,
} battery_property_type_t;

typedef enum {
    BATTERY_PROPERTY_FLAGS_NONE = 0,
    /* Set or get refers to property value that sets the alarm when
     * property value goes below specified value
     */
    BATTERY_PROPERTY_FLAGS_LOW_ALARM_SET_THRESHOLD      = 0x01,
    /* Set or get refers to property value that sets the alarm when
     * property value goes below specified value
     */
    BATTERY_PROPERTY_FLAGS_LOW_ALARM_CLEAR_THRESHOLD    = 0x02,
    BATTERY_PROPERTY_FLAGS_LOW_ALARM                    = 0x03,
    /* Set or get refers to property value that sets the alarm when
     * property value goes above specified value
     */
    BATTERY_PROPERTY_FLAGS_HIGH_ALARM_SET_THRESHOLD     = 0x04,
    /* Set or get refers to property value that clears the alarm when
     * property value goes below specified value.
     */
    BATTERY_PROPERTY_FLAGS_HIGH_ALARM_CLEAR_THRESHOLD   = 0x08,
    /* Property is not supported by hardware
     * This is specially important for threshold levels,
     * battery manager can create software only threshold levels.
     */
    BATTERY_PROPERTY_FLAGS_HIGH_ALARM                   = 0x0C,
    BATTERY_PROPERTY_FLAGS_ALARM_THREASH                = 0x0F,
    BATTERY_PROPERTY_FLAGS_ALARM                        = 0x10,
    BATTERY_PROPERTY_FLAGS_DERIVED                      = 0x80,
    /* Flag only needed for battery_mgr_get_property() function.
     * When property is requested that is not supported by hardware
     * it can be synthesized as software only and used for alarms
     */
    BATTERY_PROPERTY_FLAGS_CREATE                       = 0x40,
} battery_property_flags_t;

/**
 * Battery driver property declaration struct
 *
 * Driver provides array of those to declare what kind of properties
 * are supported.
 */
struct battery_driver_property
{
    battery_property_type_t   bdp_type;
    battery_property_flags_t  bdp_flags;
    const char *              bdp_name;
};

/**
 * Battery property
 * It has type that specifies property type it can also have
 * BATTERY_PROPERY_NOT_HARDWARE bit set if property was created
 * by battery manager not a driver.
 * It can also have threshold bits set if property is one of
 * the threshold values.
 */
struct battery_property {
    battery_property_type_t   bp_type;
    battery_property_flags_t  bp_flags;
    /* Field managed by battery manager */
    /* Battery property number */
    uint8_t                   bp_prop_num;
    /* Battery property number in driver property array */
    uint8_t                   bp_drv_prop_num;
    /* Driver number, 1 based index */
    uint8_t                   bp_drv_num:2;
    /* Battery number, 0 based index */
    uint8_t                   bp_bat_num:2;
    uint8_t                   bp_comp:1;
    uint8_t                   bp_base:1;
    /* Value of property is valid */
    uint8_t                   bp_valid:1;
    battery_property_value_t  bp_value;
};

static inline int driver_property(const struct battery_property *prop)
{
    return 0 == (prop->bp_flags & BATTERY_PROPERTY_FLAGS_DERIVED);
}

int battery_prop_get_value(struct battery_property *prop);

int battery_prop_get_value_float(struct battery_property *prop, float *value);
int battery_prop_get_value_uint8(struct battery_property *prop, uint8_t *val);
int battery_prop_get_value_int8(struct battery_property *prop, int8_t *val);
int battery_prop_get_value_uint16(struct battery_property *prop, uint16_t *val);
int battery_prop_get_value_int16(struct battery_property *prop, int16_t *val);
int battery_prop_get_value_uint32(struct battery_property *prop, uint32_t *val);
int battery_prop_get_value_int32(struct battery_property *prop, int32_t *val);

int battery_prop_set_value_float(struct battery_property *prop, float value);
int battery_prop_set_value_uint8(struct battery_property *prop, uint8_t val);
int battery_prop_set_value_int8(struct battery_property *prop, int8_t val);
int battery_prop_set_value_uint16(struct battery_property *prop, uint16_t val);
int battery_prop_set_value_int16(struct battery_property *prop, int16_t val);
int battery_prop_set_value_uint32(struct battery_property *prop, uint32_t val);
int battery_prop_set_value_int32(struct battery_property *prop, int32_t val);

/**
 * Register a property change listener. This allows a calling application to
 * receive callbacks when property changes.
 * Same listener can be used for many properties.
 *
 * @param The listener to register
 * @param The battery property to register a listener on
 *
 * @return 0 on success, non-zero error code on failure.
 */
int battery_prop_change_subscribe(struct battery_prop_listener *listener,
        struct battery_property *prop);

/**
 * Unregister a property change listener.
 *
 * @param The listener to unregister
 * @param The battery property to unregister a listener from
 *        if prop is NULL, listener is unregistered from all properties
 *
 * @return 0 on success, non-zero error code on failure.
 */
int battery_prop_change_unsubscribe(struct battery_prop_listener *listener,
        struct battery_property *prop);

/**
 * Register a battery listener. This allows a calling application to
 * receive callbacks when new data was read from fuel gauge.
 *
 * @param The battery property to register a listener on
 * @param The listener to register
 *
 * @return 0 on success, non-zero error code on failure.
 */
int battery_prop_poll_subscribe(struct battery_prop_listener *listener,
        struct battery_property *prop);

/**
 * Unregister a property read listener.
 *
 * @param The listener to unregister
 * @param The battery property to unregister a listener from
 *        if prop is NULL, listener is unregistered from all properties
 *
 * @return 0 on success, non-zero error code on failure.
 */
int battery_prop_poll_unsubscribe(struct battery_prop_listener *listener,
        struct battery_property *prop);

typedef int (*battery_prop_read_t)(struct battery_prop_listener *listener,
        const struct battery_property *prop);

typedef int (*battery_prop_changed_t)(struct battery_prop_listener *listener,
        const struct battery_property *prop);

/**
 * Listener structure which may be registered to battery properties.
 * User bpl_prop_read function will be called for every property that
 * this listener is register for just after data is received.
 */
struct battery_prop_listener {
    /*
     * Battery data handler function, called when data was read.
     */
    battery_prop_read_t bpl_prop_read;
    /*
     * Property changed function
     */
    battery_prop_changed_t bpl_prop_changed;
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

#endif /* __BATTERY_PROP_H__ */
