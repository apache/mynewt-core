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

#include "os/os.h"
#include "os/os_dev.h"
#include "syscfg/syscfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Package init function.  Remove when we have post-kernel init stages.
 */
void sensor_pkg_init(void);


/* Forward declare sensor structure defined below. */
struct sensor;

/**
 * @{ Sensor API
 */

typedef enum {
 /* No sensor type, used for queries */
    SENSOR_TYPE_NONE                 = 0,
    /* Accelerometer functionality supported */
    SENSOR_TYPE_ACCELEROMETER        = (1 << 0),
    /* Magnetic field supported */
    SENSOR_TYPE_MAGNETIC_FIELD       = (1 << 1),
    /* Gyroscope supported */
    SENSOR_TYPE_GYROSCOPE            = (1 << 2),
    /* Light supported */
    SENSOR_TYPE_LIGHT                = (1 << 3),
    /* Temperature supported */
    SENSOR_TYPE_TEMPERATURE          = (1 << 4),
    /* Ambient temperature supported */
    SENSOR_TYPE_AMBIENT_TEMPERATURE  = (1 << 5),
    /* Pressure sensor supported */
    SENSOR_TYPE_PRESSURE             = (1 << 6),
    /* Proximity sensor supported */
    SENSOR_TYPE_PROXIMITY            = (1 << 7),
    /* Relative humidity supported */
    SENSOR_TYPE_RELATIVE_HUMIDITY    = (1 << 8),
    /* Rotation vector (quaternion) supported */
    SENSOR_TYPE_ROTATION_VECTOR      = (1 << 9),
    /* Altitude Supported */
    SENSOR_TYPE_ALTITUDE             = (1 << 10),
    /* Weight Supported */
    SENSOR_TYPE_WEIGHT               = (1 << 11),
    /* Linear Accelerometer (Without Gravity) */
    SENSOR_TYPE_LINEAR_ACCEL         = (1 << 12),
    /* Gravity Sensor */
    SENSOR_TYPE_GRAVITY              = (1 << 13),
    /* Euler Orientation Sensor */
    SENSOR_TYPE_EULER                = (1 << 14),
    /* Color Sensor */
    SENSOR_TYPE_COLOR                = (1 << 15),

    /* Standard sensor types to be defined here */

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


/**
 * Opaque 32-bit value, must understand underlying sensor type
 * format in order to interpret.
 */
#define SENSOR_VALUE_TYPE_OPAQUE (0)
/**
 * 32-bit signed integer
 */
#define SENSOR_VALUE_TYPE_INT32  (1)
/**
 * 32-bit floating point
 */
#define SENSOR_VALUE_TYPE_FLOAT  (2)
/**
 * 32-bit integer triplet.
 */
#define SENSOR_VALUE_TYPE_INT32_TRIPLET (3)
/**
 * 32-bit floating point number triplet.
 */
#define SENSOR_VALUE_TYPE_FLOAT_TRIPLET (4)


/**
 * Configuration structure, describing a specific sensor type off of
 * an existing sensor.
 */
struct sensor_cfg {
    /* The value type for this sensor (e.g. SENSOR_VALUE_TYPE_INT32).
     * Used to describe the result format for the value corresponding
     * to a specific sensor type.
     */
    uint8_t sc_valtype;
    /* Reserved for future usage */
    uint8_t _reserved[3];
};

/**
 * Callback for handling sensor data, specified in a sensor listener.
 *
 * @param The sensor for which data is being returned
 * @param The argument provided to sensor_read() function.
 * @param A single sensor reading for that sensor listener
 *
 * @return 0 on success, non-zero error code on failure.
 */
typedef int (*sensor_data_func_t)(struct sensor *, void *, void *);

/**
 *
 */
struct sensor_listener {
    /* The type of sensor data to listen for, this is interpreted as a
     * mask, and this listener is called for all sensor types on this
     * sensor that match the mask.
     */
    sensor_type_t sl_sensor_type;

    /* Sensor data handler function, called when has data */
    sensor_data_func_t sl_func;

    /* Argument for the sensor listener */
    void *sl_arg;

    /* Next item in the sensor listener list.  The head of this list is
     * contained within the sensor object.
     */
    SLIST_ENTRY(sensor_listener) sl_next;
};

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
 * Read a single value from a sensor, given a specific sensor type
 * (e.g. SENSOR_TYPE_PROXIMITY).
 *
 * @param The sensor to read from
 * @param The type(s) of sensor values to read.  Mask containing that type, provide
 *        all, to get all values.
 * @param The function to call with each value read.  If NULL, it calls all
 *        sensor listeners associated with this function.
 * @param The argument to pass to the read callback.
 * @param Timeout.  If block until result, specify OS_TIMEOUT_NEVER, 0 returns
 *        immediately (no wait.)
 *
 * @return 0 on success, non-zero error code on failure.
 */
typedef int (*sensor_read_func_t)(struct sensor *, sensor_type_t,
        sensor_data_func_t, void *, uint32_t);

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

struct sensor_driver {
    sensor_get_interface_func_t sd_get_interface;
    sensor_read_func_t sd_read;
    sensor_get_config_func_t sd_get_config;
};

struct sensor_timestamp {
    struct os_timeval st_ostv;
    struct os_timezone st_ostz;
    uint32_t st_cputime;
};

/*
 * Return the OS device structure corresponding to this sensor
 */
#define SENSOR_GET_DEVICE(__s) ((__s)->s_dev)

struct sensor {
    /* The OS device this sensor inherits from, this is typically a sensor
     * specific driver.
     */
    struct os_dev *s_dev;

    /* The lock for this sensor object */
    struct os_mutex s_lock;


    /* A bit mask describing the types of sensor objects available from this
     * sensor. If the bit corresponding to the sensor_type_t is set, then this
     * sensor supports that variable.
     */
    sensor_type_t s_types;
    /**
     * Poll rate in MS for this sensor.
     */
    uint32_t s_poll_rate;

    /* The next time at which we want to poll data from this sensor */
    os_time_t s_next_run;

    /* Sensor driver specific functions, created by the device registering the
     * sensor.
     */
    struct sensor_driver *s_funcs;

    /* Sensor last reading timestamp */
    struct sensor_timestamp s_sts;

    /* A list of listeners that are registered to receive data off of this
     * sensor
     */
    SLIST_HEAD(, sensor_listener) s_listener_list;
    /* The next sensor in the global sensor list. */
    TAILQ_ENTRY(sensor) s_next;
};

int sensor_init(struct sensor *, struct os_dev *dev);
int sensor_lock(struct sensor *);
void sensor_unlock(struct sensor *);
int sensor_register_listener(struct sensor *, struct sensor_listener *);
int sensor_unregister_listener(struct sensor *, struct sensor_listener *);

int sensor_read(struct sensor *, sensor_type_t, sensor_data_func_t, void *,
        uint32_t);

/**
 * Set the driver functions for this sensor, along with the type of sensor
 * data available for the given sensor.
 *
 * @param The sensor to set the driver information for
 * @param The types of sensor data available for this sensor
 * @param The driver functions for this sensor
 *
 * @return 0 on success, non-zero error code on failure
 */
static inline int
sensor_set_driver(struct sensor *sensor, sensor_type_t type,
        struct sensor_driver *driver)
{
    sensor->s_funcs = driver;
    sensor->s_types = type;

    return (0);
}

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
sensor_get_config(struct sensor *sensor, sensor_type_t type,
        struct sensor_cfg *cfg)
{
    return (sensor->s_funcs->sd_get_config(sensor, type, cfg));
}

/**
 * Get a more specific interface for this sensor, like accelerometer, or gyro.
 *
 * @param The sensor to get the interface from
 * @param The type of interface to get from this sensor
 *
 * @return A pointer to the more specific sensor interface on success, or NULL
 * if not found.
 */
static inline void *
sensor_get_interface(struct sensor *sensor, sensor_type_t type)
{
    return (sensor->s_funcs->sd_get_interface(sensor, type));
}

/**
 * @{ Sensor Manager API
 */


/* Default number of ticks to wakeup the sensor task.  If sensor
 * polling has been configured more frequently than this, it will
 * be triggered instead.
 */
#define SENSOR_MGR_WAKEUP_TICKS (MYNEWT_VAL(SENSOR_MGR_WAKEUP_RATE) * \
        (OS_TICKS_PER_SEC / 1000))

int sensor_mgr_lock(void);
void sensor_mgr_unlock(void);
int sensor_mgr_register(struct sensor *);
struct os_eventq *sensor_mgr_evq_get(void);


typedef int (*sensor_mgr_compare_func_t)(struct sensor *, void *);
struct sensor *sensor_mgr_find_next(sensor_mgr_compare_func_t, void *,
        struct sensor *);
struct sensor *sensor_mgr_find_next_bytype(sensor_type_t, struct sensor *);
struct sensor *sensor_mgr_find_next_bydevname(char *, struct sensor *);

#if MYNEWT_VAL(SENSOR_CLI)
char*
sensor_ftostr(float num, char *fltstr, int len);
int
sensor_shell_stol(char *param_val, long min, long max, long *output);
#endif

#if MYNEWT_VAL(SENSOR_OIC)
void sensor_oic_init(void);
#endif

/**
 * }@
 */



/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* __SENSOR_H__ */
