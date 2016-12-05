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

#ifndef __SENSOR_H__
#define __SENSOR_H__

#include <os/os_dev.h>

#ifdef __cplusplus
extern "C" {
#endif

struct sensor_dev;

typedef enum {
    /* No sensor type, used for queries */
    SENSOR_TYPE_NONE                 = 0,
    /* Accellerometer functionality supported */
    SENSOR_TYPE_ACCELLEROMETER       = (1 << 0),
    /* Ambient temperature supported */
    SENSOR_TYPE_AMBIENT_TEMPERATURE  = (1 << 1),
    /* Gravity supported */
    SENSOR_TYPE_GRAVITY              = (1 << 2),
    /* Gyroscope supported */
    SENSOR_TYPE_GYROSCOPE            = (1 << 3),
    /* Light supported */
    SENSOR_TYPE_LIGHT                = (1 << 4),
    /* Linear accelleration supported */
    SENSOR_TYPE_LINEAR_ACCELLERATION = (1 << 5),
    /* Magnetic field supported */
    SENSOR_TYPE_MAGNETIC_FIELD       = (1 << 6),
    /* Orientation sensor supported */
    SENSOR_TYPE_ORIENTATION          = (1 << 7),
    /* Pressure sensor supported */
    SENSOR_TYPE_PRESSURE             = (1 << 8),
    /* Proximity sensor supported */
    SENSOR_TYPE_PROXIMITY            = (1 << 9),
    /* Relative humidity supported */
    SENSOR_TYPE_RELATIVE_HUMIDITY    = (1 << 10),
    /* Rotation Vector supported */
    SENSOR_TYPE_ROTATION_VECTOR      = (1 << 11),
    /* Temperature supported */
    SENSOR_TYPE_TEMPERATURE          = (1 << 12),
    /* Altitude Supported */
    SENSOR_TYPE_ALTITUDE             = (1 << 13),
    /* User defined sensor type 1 */
    SENSOR_TYPE_USER_DEFINED_1       = (1 << 26),
    /* User defined sensor type 2 */
    SENSOR_TYPE_USER_DEFINED_2       = (1 << 27),
    /* User defined sensor type 3 */
    SENSOR_TYPE_USER_DEFINED_3       = (1 << 28),
    /* User defined sensor type 4 */
    SENSOR_TYPE_USER_DEFINED_4       = (1 << 29),
    /* User defined sensor type 5 */
    SENSOR_TYPE_USER_DEFINED_5       = (1 << 30),
    /* User defined sensor type 6 */
    SENSOR_TYPE_USER_DEFINED_6       = (1 << 31),
    /* A selector, describes all sensors */
    SENSOR_TYPE_ALL                  = 0xFFFFFFFF
} sensor_type_t;


#define SENSOR_LISTENER_TYPE_NOTIFY (0)
#define SENSOR_LISTENER_TYPE_POLL   (1)

struct sensor_reading {
    /* The timestamp, in CPU time ticks that the sensor value
     * was read.
     */
    uint32_t sv_ts;
    /* The value of the sensor, based on the type being read.
     */
    void *sv_value;
};

#define SENSOR_VALUE_TYPE_OPAQUE (0)
#define SENSOR_VALUE_TYPE_INT32  (1)
#define SENSOR_VALUE_TYPE_FLOAT  (2)

struct sensor_cfg {
    uint8_t sc_valtype;
    uint8_t _reserved[3];
};

/**
 * Callback for handling sensor data, specified in a sensor listener.
 *
 * @param The sensor for which data is being returned
 * @param The sensor listener that's configured to handle this data.
 * @param A single sensor reading
 *
 * @return 0 on succes, non-zero error code on failure.
 */
typedef int (*sensor_data_func_t)(struct sensor *,
        struct sensor_listener *,
        struct sensor_reading *v);

/**
 *
 */
struct sensor_listener {
    /* The type of sensor listener, either notification based "tell me
     * when there is a value," or polled - collect this value for me
     * every 'n' seconds and notify me.
     */
    uint8_t sl_type;
    /* Padding */
    uint8_t _pad1;
    /* Poll rate in miliseconds. */
    uint16_t sl_poll_rate_ms;
    /* The type of sensor data to listen for */
    sensor_type_t sl_sensor_type;
    /* Sensor data handler function, called when has data */
    sensor_data_func_t sl_func;

    SLIST_ENTRY(sensor_listener) sl_next;
};

struct sensor;

/**
 * Get a more specific interface on this sensor object (e.g. Gyro, Magnometer),
 * which has additional functions for managing the sensor.
 *
 * @param The sensor to get the interface on
 * @param The type of sensor interface to get.
 *
 * @return void * A pointer to the specific interface requested, or NULL on failure.
 */
typedef void *(*sensor_get_interface_func_t)(struct sensor *, sensor_type_t);

/**
 * Read a value from a sensor, given a specific sensor type (e.g. SENSOR_TYPE_PROXIMITY).
 *
 * @param The sensor to read from
 * @param The type of sensor value to read
 * @param A pointer to the sensor value to place the returned result into.
 *
 * @return 0 on success, non-zero error code on failure.
 */
typedef int (*sensor_read_func_t)(struct sensor *, sensor_type_t, void *);

/**
 * Get the configuration of the sensor for the sensor type.  This includes
 * the value type of the sensor.
 *
 * @param The type of sensor value to get configuration for
 * @param A pointer to the sensor value to place the returned result into.
 *
 * @return 0 on success, non-zero error code on failure.
 */
typedef int (*sensor_get_config_func_t)(struct sensor *, sensor_type_t,
        struct sensor_cfg *);

struct sensor_driver_funcs {
    sensor_get_interface_func_t sd_get_interface;
    sensor_read_func_t sd_read;
    sensor_get_config_func_t sd_get_config;
};

/* XXX: Make me a syscfg */
#define SENSOR_DEFAULT_POLL_RATE (10 * (OS_TICKS_PER_SEC/1000))

struct sensor {
    /* The OS device this sensor inherits from, this is typically a sensor specific
     * driver.
     */
    struct os_dev *s_dev;
    /* A bit mask describing the types of sensor objects available from this sensor.
     * If the bit corresponding to the sensor_type_t is set, then this sensor supports
     * that variable.
     */
    sensor_type_t s_types;
    /* Sensor driver specific functions, created by the device registering the sensor.
     */
    const struct sensor_driver_funcs s_funcs;
    /* A list of listeners that are registered to receive data off of this sensor
     */
    SLIST_HEAD(, struct sensor_listener) s_listener_list;
    /* The next sensor in the global sensor list. */
    SLIST_ENTRY(sensor) s_next;
};

int sensor_init(struct sensor *);
int sensor_register(struct sensor *);
int sensor_lock(struct sensor *);
void sensor_unlock(struct sensor *);
int sensor_register_listener(struct sensor *, struct sensor_listener *);
struct sensor *sensor_find_next(sensor_type_t, struct sensor *);


/**
 * Read the configuration for the sensor type "type," and return the
 * configuration into "cfg."
 *
 * @param sensor The sensor to read configuration for
 * @param type The type of sensor configuration to read
 * @param cfg The configuration structure to point to.
 *
 * @return 0 on success, non-zero error code on failure.
 */
static inline int
sensor_get_config(struct sensor *sensor, sensor_type_t type, struct sensor_config *cfg)
{
    return (sensor->s_funcs.sd_get_config(sensor, type, cfg));
}

/**
 * Read the data for sensor type "type," from the sensor, "sensor" and
 * return the result into the "value" parameter.
 *
 * @param The senssor to read data from
 * @param The type of sensor data to read from the sensor
 * @param The sensor value to place data into.
 *
 * @return 0 on success, non-zero on failure.
 */
static inline int
sensor_read(struct sensor *sensor, sensor_type_t type, struct sensor_value *value)
{
    int rc;

    rc = sensor_lock(sensor);
    if (rc != 0) {
        goto done;
    }

    rc = sensor->s_funcs.sd_read(sensor, type, value);

    sensor_unlock(sensor);

done:
    return (rc);
}

/**
 * Get a more specific interface for this sensor, like accellerometer, or gyro.
 *
 * @param The sensor to get the interface from
 * @param The type of interface to get from this sensor
 *
 * @return A pointer to the more specific sensor interface on success, or NULL if
 *         not found.
 */
static inline void *
sensor_get_interface(struct sensor *sensor, sensor_type_t type)
{
    int rc;

    rc = sensor_lock(sensor);
    if (rc != 0) {
        goto done;
    }

    rc = sensor->s_funcs.sd_get_interface(sensor, type);

    sensor_unlock(sensor);

done:
    return (rc);
}


#ifdef __cplusplus
}
#endif

#endif /* __SENSOR_H__ */
