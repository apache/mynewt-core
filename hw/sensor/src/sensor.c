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

#include <string.h>
#include <errno.h>
#include <assert.h>

#include "os/os.h"
#include "sysinit/sysinit.h"

#include "sensor/sensor.h"

#include "sensor_priv.h"
#include "os/os_time.h"
#include "os/os_cputime.h"
#include "defs/error.h"
#include "console/console.h"
#include "syscfg/syscfg.h"
#include "sensor/accel.h"
#include "sensor/mag.h"
#include "sensor/light.h"
#include "sensor/quat.h"
#include "sensor/euler.h"
#include "sensor/color.h"
#include "sensor/temperature.h"
#include "sensor/pressure.h"
#include "sensor/humidity.h"
#include "sensor/gyro.h"

struct {
    struct os_mutex mgr_lock;

    struct os_callout mgr_wakeup_callout;
    struct os_eventq *mgr_eventq;

    SLIST_HEAD(, sensor) mgr_sensor_list;
} sensor_mgr;

struct sensor_read_ctx {
    sensor_data_func_t user_func;
    void *user_arg;
};

struct sensor_timestamp sensor_base_ts;
struct os_callout st_up_osco;

static void sensor_read_ev_cb(struct os_event *ev);

  /** OS event - for doing a sensor read */
static struct os_event sensor_read_event = {
    .ev_cb = sensor_read_ev_cb,
};

/**
 * Lock sensor manager to access the list of sensors
 */
int
sensor_mgr_lock(void)
{
    int rc;

    rc = os_mutex_pend(&sensor_mgr.mgr_lock, OS_TIMEOUT_NEVER);
    if (rc == 0 || rc == OS_NOT_STARTED) {
        return (0);
    }
    return (rc);
}

/**
 * Unlock sensor manager once the list of sensors has been accessed
 */
void
sensor_mgr_unlock(void)
{
    (void) os_mutex_release(&sensor_mgr.mgr_lock);
}

static void
sensor_mgr_remove(struct sensor *sensor)
{
    SLIST_REMOVE(&sensor_mgr.mgr_sensor_list, sensor, sensor, s_next);
}

static void
sensor_mgr_insert(struct sensor *sensor)
{
    struct sensor *cursor, *prev;

    prev = cursor = NULL;
    SLIST_FOREACH(cursor, &sensor_mgr.mgr_sensor_list, s_next) {
        if (cursor->s_next_run == OS_TIMEOUT_NEVER) {
            break;
        }

        if (OS_TIME_TICK_LT(sensor->s_next_run, cursor->s_next_run)) {
            break;
        }

        prev = cursor;
    }

    if (prev == NULL) {
        SLIST_INSERT_HEAD(&sensor_mgr.mgr_sensor_list, sensor, s_next);
    } else {
        SLIST_INSERT_AFTER(prev, sensor, s_next);
    }
}

/**
 * Set the sensor poll rate based on teh device name
 *
 * @param The devname
 * @param The poll rate in milli seconds
 */
int
sensor_set_poll_rate_ms(char *devname, uint32_t poll_rate)
{
    struct sensor *sensor;
    int rc;

    sensor = sensor_mgr_find_next_bydevname(devname, NULL);

    if (!sensor) {
        rc = SYS_EINVAL;
        goto err;
    }

    sensor_lock(sensor);

    sensor->s_poll_rate = poll_rate;

    sensor_unlock(sensor);

    return 0;
err:
    return rc;
}

/**
 * Register the sensor with the global sensor list. This makes the sensor
 * searchable by other packages, who may want to look it up by type.
 *
 * @param The sensor to register
 *
 * @return 0 on success, non-zero error code on failure.
 */
int
sensor_mgr_register(struct sensor *sensor)
{
    int rc;

    rc = sensor_mgr_lock();
    if (rc != 0) {
        goto err;
    }

    rc = sensor_lock(sensor);
    if (rc != 0) {
        goto err;
    }

    sensor_mgr_insert(sensor);

    sensor_unlock(sensor);

    sensor_mgr_unlock();

    return (0);
err:
    return (rc);
}


static os_time_t
sensor_mgr_poll_one(struct sensor *sensor, os_time_t now)
{
    uint32_t sensor_ticks;
    int rc;

    rc = sensor_lock(sensor);
    if (rc != 0) {
        goto err;
    }

    /* Sensor read results.  Every time a sensor is read, all of its
     * listeners are called by default.  Specify NULL as a callback,
     * because we just want to run all the listeners.
     */
    sensor_read(sensor, sensor->s_mask, NULL, NULL, OS_TIMEOUT_NEVER);

    /* Remove the sensor from the sensor list for insert. */
    sensor_mgr_remove(sensor);

    /* Set next wakeup, and insert the sensor back into the
     * list.
     */
    os_time_ms_to_ticks(sensor->s_poll_rate, &sensor_ticks);
    sensor->s_next_run = sensor_ticks + now;

    /* Re-insert the sensor manager, with the new wakeup time. */
    sensor_mgr_insert(sensor);

    /* Unlock the sensor to allow other access */
    sensor_unlock(sensor);

    return (sensor_ticks);
err:
    /* Couldn't lock it.  Re-run task and spin until we get result. */
    return (0);
}

/**
 * Event that wakes up the sensor manager, this goes through the sensor
 * list and polls any active sensors.
 *
 * @param OS event
 */
static void
sensor_mgr_wakeup_event(struct os_event *ev)
{
    struct sensor *cursor;
    os_time_t now;
    os_time_t task_next_wakeup;
    os_time_t next_wakeup;
    int rc;

    now = os_time_get();

    task_next_wakeup = SENSOR_MGR_WAKEUP_TICKS;

    rc = sensor_mgr_lock();
    if (rc != 0) {
        /* Schedule again in 1 tick, see if we can reacquire the lock */
        task_next_wakeup = 1;
        goto done;
    }

    SLIST_FOREACH(cursor, &sensor_mgr.mgr_sensor_list, s_next) {
        /* Sensors that are not periodic are inserted at the end of the sensor
         * list.
         */
        if (cursor->s_next_run == OS_TIMEOUT_NEVER) {
            break;
        }

        /* List is sorted by what runs first.  If we reached the first element that
         * doesn't run, break out.
         */
        if (OS_TIME_TICK_LT(now, cursor->s_next_run)) {
            break;
        }

        /* Sensor poll one completes the poll, updates the sensor's "next run,"
         * and re-inserts it into the list.  It returns the next wakeup time in ticks
         * from now ticks for this sensor.
         */
        next_wakeup = sensor_mgr_poll_one(cursor, now);

        /* If the next wakeup time for this sensor is before the task's next
         * scheduled wakeup, move that forward, so we can collect data from that
         * sensor
         */
        if (task_next_wakeup > next_wakeup) {
            task_next_wakeup = next_wakeup;
        }

    }

done:
    os_callout_reset(&sensor_mgr.mgr_wakeup_callout, task_next_wakeup);
}

/**
 * Event that wakes up timestamp update procedure, this updates the base
 * os_timeval in the global structure along with the base cputime
 * @param OS event
 */
static void
sensor_base_ts_update_event(struct os_event *ev)
{
    os_time_t ticks;
    int rc;
    struct os_timeval ostv;
    struct os_timezone ostz;

    ticks = os_time_get();

    rc = os_gettimeofday(&ostv, &ostz);
    if (rc) {
        /* There is nothing we can do here
         * just reset the timer frequently if we
         * fail to get time, till then we will keep using
         * old timestamp values.
         */
        ticks += OS_TICKS_PER_SEC * 600;
        goto done;
    }

    /* CPU time gets wrapped in 4295 seconds since it is uint32_t,
     * hence the hardcoded value of 3600 seconds, We want to make
     * sure that the cputime never gets wrapped more than once.
     * os_timeval usecs value gets wrapped in 2147 secs since it is int32_t.
     * Hence, we take 2000 secs so that we update before it gets wrapped
     * without cutting it too close.
     */
    ticks += OS_TICKS_PER_SEC * 2000;

    sensor_base_ts.st_ostv = ostv;
    sensor_base_ts.st_ostz = ostz;
    sensor_base_ts.st_cputime = os_cputime_get32();

done:
    os_callout_reset(&st_up_osco, ticks);
}

/**
 * Get the current eventq, the system is misconfigured if there is still
 * no parent eventq.
 */
struct os_eventq *
sensor_mgr_evq_get(void)
{
    return (sensor_mgr.mgr_eventq);
}

static void
sensor_mgr_evq_set(struct os_eventq *evq)
{
    sensor_mgr.mgr_eventq = evq;
}

static void
sensor_mgr_init(void)
{
    struct os_timeval ostv;
    struct os_timezone ostz;

#ifdef MYNEWT_VAL_SENSOR_MGR_EVQ
    sensor_mgr_evq_set(MYNEWT_VAL(SENSOR_MGR_EVQ));
#else
    sensor_mgr_evq_set(os_eventq_dflt_get());
#endif

    /**
     * Initialize sensor polling callout and set it to fire on boot.
     */
    os_callout_init(&sensor_mgr.mgr_wakeup_callout, sensor_mgr_evq_get(),
            sensor_mgr_wakeup_event, NULL);
    os_callout_reset(&sensor_mgr.mgr_wakeup_callout, 0);

    /* Initialize sensor cputime update callout and set it to fire after an
     * hour, CPU time gets wrapped in 4295 seconds,
     * hence the hardcoded value of 3600 seconds, We make sure that the
     * cputime never gets wrapped more than once.
     */
    os_gettimeofday(&ostv, &ostz);

    sensor_base_ts.st_ostv = ostv;
    sensor_base_ts.st_ostz = ostz;
    sensor_base_ts.st_cputime = os_cputime_get32();

    os_callout_init(&st_up_osco, sensor_mgr_evq_get(),
            sensor_base_ts_update_event, NULL);
    os_callout_reset(&st_up_osco, OS_TICKS_PER_SEC);

    os_mutex_init(&sensor_mgr.mgr_lock);
}

/**
 * The sensor manager contains a list of sensors, this function returns
 * the next sensor in that list, for which compare_func() returns successful
 * (one).  If prev_cursor is provided, the function starts at that point
 * in the sensor list.
 *
 * @warn This function MUST be locked by sensor_mgr_lock/unlock() if the goal is
 * to iterate through sensors (as opposed to just finding one.)  As the
 * "prev_cursor" may be resorted in the sensor list, in between calls.
 *
 * @param The comparison function to use against sensors in the list.
 * @param The argument to provide to that comparison function
 * @param The previous sensor in the sensor manager list, in case of
 *        iteration.  If desire is to find first matching sensor, provide a
 *        NULL value.
 *
 * @return A pointer to the first sensor found from prev_cursor, or
 *         NULL, if none found.
 *
 */
struct sensor *
sensor_mgr_find_next(sensor_mgr_compare_func_t compare_func, void *arg,
        struct sensor *prev_cursor)
{
    struct sensor *cursor;
    int rc;

    cursor = NULL;

    /* Couldn't acquire lock of sensor list, exit */
    rc = sensor_mgr_lock();
    if (rc != 0) {
        goto done;
    }

    cursor = prev_cursor;
    if (cursor == NULL) {
        cursor = SLIST_FIRST(&sensor_mgr.mgr_sensor_list);
    } else {
        cursor = SLIST_NEXT(prev_cursor, s_next);
    }

    while (cursor != NULL) {
        if (compare_func(cursor, arg)) {
            break;
        }
        cursor = SLIST_NEXT(cursor, s_next);
    }

    sensor_mgr_unlock();

done:
    return (cursor);
}

/**
 * Check if sensor type matches
 *
 * @param The sensor object
 * @param The type to check
 *
 * @return 1 if matches, 0 if it doesn't match.
 */
int
sensor_mgr_match_bytype(struct sensor *sensor, void *arg)
{
    sensor_type_t *type;

    type = (sensor_type_t *) arg;

    /* s_types is a bitmask that contains the supported sensor types for this
     * sensor, and type is the bitmask we're searching for. We also look at
     * the mask as the driver might be configured to work in a mode where only
     * some of the sensors are supported but not all. Compare the three,
     * and if there is a match, return 1. If it is not supported, return 0.
     */
    return (*type & sensor->s_types & sensor->s_mask) ? 1 : 0;
}

/**
 * Find the "next" sensor available for a given sensor type.
 *
 * If the sensor parameter, is present find the next entry from that
 * parameter.  Otherwise, find the first matching sensor.
 *
 * @param The type of sensor to search for
 * @param The cursor to search from, or NULL to start from the beginning.
 *
 * @return A pointer to the sensor object matching that sensor type, or NULL if
 *         none found.
 */
struct sensor *
sensor_mgr_find_next_bytype(sensor_type_t type, struct sensor *prev_cursor)
{
    return (sensor_mgr_find_next(sensor_mgr_match_bytype, (void *) &type,
                prev_cursor));
}

static int
sensor_mgr_match_bydevname(struct sensor *sensor, void *arg)
{
    char *devname;

    devname = (char *) arg;

    if (!strcmp(sensor->s_dev->od_name, devname)) {
        return (1);
    }

    return (0);
}

/**
 * Search the sensor thresh list for specific type of sensor
 *
 * @param The sensor type to search for
 * @param Ptr to a sensor
 *
 * @return NULL when no sensor type is found, ptr to sensor_type_traits structure
 * when found
 */
struct sensor_type_traits *
sensor_get_type_traits_bytype(sensor_type_t type, struct sensor *sensor)
{
    struct sensor_type_traits *stt;

    stt = NULL;

    sensor_lock(sensor);

    SLIST_FOREACH(stt, &sensor->s_type_traits_list, stt_next) {
        if (stt->stt_sensor_type == type) {
            break;
        }
    }

    sensor_unlock(sensor);

    return stt;
}

/**
 * Remove a sensor type trait. This allows a calling application to unset
 * sensortype trait for a given sensor object.
 *
 * @param The sensor object
 * @param The sensor trait to remove from the sensor type trait list
 *
 * @return 0 on success, non-zero error code on failure.
 */

int
sensor_remove_type_trait(struct sensor *sensor,
                         struct sensor_type_traits *stt)
{
    int rc;

    rc = sensor_lock(sensor);
    if (rc != 0) {
        goto err;
    }

    /* Remove this entry from the list */
    SLIST_REMOVE(&sensor->s_type_traits_list, stt, sensor_type_traits,
            stt_next);

    sensor_unlock(sensor);

    return (0);
err:
    return (rc);
}

/**
 * Search the sensor list and find the next sensor that corresponds
 * to a given device name.
 *
 * @param The device name to search for
 * @param The previous sensor found with this device name
 *
 * @return 0 on success, non-zero error code on failure
 */
struct sensor *
sensor_mgr_find_next_bydevname(char *devname, struct sensor *prev_cursor)
{
    return (sensor_mgr_find_next(sensor_mgr_match_bydevname, devname,
            prev_cursor));
}

/**
 * Initialize the sensor package, called through SYSINIT.  Note, this function
 * will assert if called directly, and _NOT_ through the sysinit package.
 */
void
sensor_pkg_init(void)
{
    sensor_mgr_init();

#if MYNEWT_VAL(SENSOR_CLI)
    sensor_shell_register();
#endif
}


/**
 * Lock access to the sensor specified by sensor.  Blocks until lock acquired.
 *
 * @param The sensor to lock
 *
 * @return 0 on success, non-zero on failure.
 */
int
sensor_lock(struct sensor *sensor)
{
    int rc;

    rc = os_mutex_pend(&sensor->s_lock, OS_TIMEOUT_NEVER);
    if (rc == 0 || rc == OS_NOT_STARTED) {
        return (0);
    }
    return (rc);
}

/**
 * Unlock access to the sensor specified by sensor.
 *
 * @param The sensor to unlock access to.
 */
void
sensor_unlock(struct sensor *sensor)
{
    os_mutex_release(&sensor->s_lock);
}


/**
 * Initialize a sensor
 *
 * @param The sensor to initialize
 * @param The device to associate with this sensor.
 *
 * @return 0 on success, non-zero error code on failure.
 */
int
sensor_init(struct sensor *sensor, struct os_dev *dev)
{
    int rc;

    memset(sensor, 0, sizeof(*sensor));

    rc = os_mutex_init(&sensor->s_lock);
    if (rc != 0) {
        goto err;
    }
    sensor->s_dev = dev;

    sensor->s_poll_rate = OS_TIMEOUT_NEVER;

    return (0);
err:
    return (rc);
}


/**
 * Register a sensor listener. This allows a calling application to receive
 * callbacks for data from a given sensor object.
 *
 * For more information on the type of callbacks available, see the documentation
 * for the sensor listener structure.
 *
 * @param The sensor to register a listener on
 * @param The listener to register onto the sensor
 *
 * @return 0 on success, non-zero error code on failure.
 */
int
sensor_register_listener(struct sensor *sensor,
        struct sensor_listener *listener)
{
    int rc;

    rc = sensor_lock(sensor);
    if (rc != 0) {
        goto err;
    }

    SLIST_INSERT_HEAD(&sensor->s_listener_list, listener, sl_next);

    sensor_unlock(sensor);

    return (0);
err:
    return (rc);
}

/**
 * Un-register a sensor listener. This allows a calling application to unset
 * callbacks for a given sensor object.
 *
 * @param The sensor object
 * @param The listener to remove from the sensor listener list
 *
 * @return 0 on success, non-zero error code on failure.
 */

int
sensor_unregister_listener(struct sensor *sensor,
        struct sensor_listener *listener)
{
    int rc;

    rc = sensor_lock(sensor);
    if (rc != 0) {
        goto err;
    }

    /* Remove this entry from the list */
    SLIST_REMOVE(&sensor->s_listener_list, listener, sensor_listener,
            sl_next);

    sensor_unlock(sensor);

    return (0);
err:
    return (rc);
}

static int
sensor_read_data_func(struct sensor *sensor, void *arg, void *data,
                      sensor_type_t type)
{
    struct sensor_listener *listener;
    struct sensor_read_ctx *ctx;

    ctx = (struct sensor_read_ctx *) arg;

    if ((uint8_t)(uintptr_t)(ctx->user_arg) != SENSOR_IGN_LISTENER) {
        /* Notify all listeners first */
        SLIST_FOREACH(listener, &sensor->s_listener_list, sl_next) {
            if (listener->sl_sensor_type & type) {
                listener->sl_func(sensor, listener->sl_arg, data, type);
            }
        }
    }

    /* Call data function */
    if (ctx->user_func != NULL) {
        return (ctx->user_func(sensor, ctx->user_arg, data, type));
    } else {
        return (0);
    }
}

/**
 * Puts read event on the sensor manager evq
 *
 * @param arg
 */
void
sensor_mgr_put_read_evt(void *arg)
{
    sensor_read_event.ev_arg = arg;
    os_eventq_put(sensor_mgr_evq_get(), &sensor_read_event);
}

static void
sensor_read_ev_cb(struct os_event *ev)
{
    int rc;
    struct sensor_read_ev_ctx *srec;

    srec = ev->ev_arg;
    rc = sensor_read(srec->srec_sensor, srec->srec_type, NULL, NULL,
                     OS_TIMEOUT_NEVER);
    assert(rc == 0);
}

static void
sensor_up_timestamp(struct sensor *sensor)
{
    uint32_t curr_ts_ticks;
    uint32_t ts;

    curr_ts_ticks = os_cputime_get32();

    ts = os_cputime_ticks_to_usecs(curr_ts_ticks -
             sensor_base_ts.st_cputime);

    /* Updating cputime */
    sensor_base_ts.st_cputime = sensor->s_sts.st_cputime = curr_ts_ticks;

    /* Updating seconds */
    sensor_base_ts.st_ostv.tv_sec  = sensor_base_ts.st_ostv.tv_sec + (ts +
        sensor_base_ts.st_ostv.tv_usec)/1000000;
    sensor->s_sts.st_ostv.tv_sec = sensor_base_ts.st_ostv.tv_sec;

    /* Updating Micro seconds */
    sensor_base_ts.st_ostv.tv_usec  =
        (sensor_base_ts.st_ostv.tv_usec + ts)%1000000;
    sensor->s_sts.st_ostv.tv_usec = sensor_base_ts.st_ostv.tv_usec;

}

/**
 * Get the type traits for a sensor
 *
 * @param name of the sensor
 * @param Ptr to sensor types trait struct
 * @param type of sensor
 *
 * @return NULL on failure, sensor struct on success
 */
struct sensor *
sensor_get_type_traits_byname(char *devname, struct sensor_type_traits **stt,
                              sensor_type_t type)
{
    struct sensor *sensor;

    sensor = sensor_mgr_find_next_bydevname(devname, NULL);
    if (!sensor) {
        goto err;
    }

    *stt = sensor_get_type_traits_bytype(type, sensor);

err:
    return sensor;
}

static uint8_t
sensor_window_cmp_quat(struct sensor_quat_data *d_sqd,
                       struct sensor_quat_data *h_sqd,
                       struct sensor_quat_data *l_sqd)
{
    uint8_t trigger;

    trigger = 0;

    trigger =
        (SENSOR_DATA_CMP_LT(d_sqd, h_sqd, sqd_x) &&
         SENSOR_DATA_CMP_GT(d_sqd, l_sqd, sqd_x));
    trigger |=
        (SENSOR_DATA_CMP_LT(d_sqd, h_sqd, sqd_y) &&
         SENSOR_DATA_CMP_GT(d_sqd, l_sqd, sqd_y));
    trigger |=
        (SENSOR_DATA_CMP_LT(d_sqd, h_sqd, sqd_z) &&
         SENSOR_DATA_CMP_GT(d_sqd, l_sqd, sqd_z));

    return trigger;
}

static uint8_t
sensor_window_cmp_accel(struct sensor_accel_data *d_sad,
                        struct sensor_accel_data *h_sad,
                        struct sensor_accel_data *l_sad)
{
    uint8_t trigger;

    trigger = 0;

    trigger =
        (SENSOR_DATA_CMP_LT(d_sad, h_sad, sad_x) &&
         SENSOR_DATA_CMP_GT(d_sad, l_sad, sad_x));
    trigger |=
        (SENSOR_DATA_CMP_LT(d_sad, h_sad, sad_y) &&
         SENSOR_DATA_CMP_GT(d_sad, l_sad, sad_y));
    trigger |=
        (SENSOR_DATA_CMP_LT(d_sad, h_sad, sad_z) &&
         SENSOR_DATA_CMP_GT(d_sad, l_sad, sad_z));

    return trigger;
}

static uint8_t
sensor_window_cmp_euler(struct sensor_euler_data *d_sed,
                        struct sensor_euler_data *h_sed,
                        struct sensor_euler_data *l_sed)
{
    uint8_t trigger;

    trigger = 0;

    trigger =
        (SENSOR_DATA_CMP_LT(d_sed, h_sed, sed_h) &&
         SENSOR_DATA_CMP_GT(d_sed, l_sed, sed_h));
    trigger |=
        (SENSOR_DATA_CMP_LT(d_sed, h_sed, sed_r) &&
         SENSOR_DATA_CMP_GT(d_sed, l_sed, sed_r));
    trigger |=
        (SENSOR_DATA_CMP_LT(d_sed, h_sed, sed_p) &&
         SENSOR_DATA_CMP_GT(d_sed, l_sed, sed_p));

    return trigger;
}

static uint8_t
sensor_window_cmp_gyro(struct sensor_gyro_data *d_sgd,
                       struct sensor_gyro_data *h_sgd,
                       struct sensor_gyro_data *l_sgd)
{
    uint8_t trigger;

    trigger = 0;

    trigger =
        (SENSOR_DATA_CMP_LT(d_sgd, h_sgd, sgd_x) &&
         SENSOR_DATA_CMP_GT(d_sgd, l_sgd, sgd_x));
    trigger |=
        (SENSOR_DATA_CMP_LT(d_sgd, h_sgd, sgd_y) &&
         SENSOR_DATA_CMP_GT(d_sgd, l_sgd, sgd_y));
    trigger |=
        (SENSOR_DATA_CMP_LT(d_sgd, h_sgd, sgd_z) &&
         SENSOR_DATA_CMP_GT(d_sgd, l_sgd, sgd_z));

    return trigger;
}

static uint8_t
sensor_window_cmp_mag(struct sensor_mag_data *d_smd,
                      struct sensor_mag_data *h_smd,
                      struct sensor_mag_data *l_smd)
{
    uint8_t trigger;

    trigger = 0;

    trigger =
         (SENSOR_DATA_CMP_LT(d_smd, h_smd, smd_x) &&
          SENSOR_DATA_CMP_GT(d_smd, l_smd, smd_x));
    trigger |=
         (SENSOR_DATA_CMP_LT(d_smd, h_smd, smd_y) &&
          SENSOR_DATA_CMP_GT(d_smd, l_smd, smd_y));
    trigger |=
         (SENSOR_DATA_CMP_LT(d_smd, h_smd, smd_z) &&
          SENSOR_DATA_CMP_GT(d_smd, l_smd, smd_z));

    return trigger;
}

static uint8_t
sensor_window_cmp_temp(struct sensor_temp_data *d_std,
                       struct sensor_temp_data *h_std,
                       struct sensor_temp_data *l_std)
{
    uint8_t trigger;

    trigger = 0;

    trigger |=
        (SENSOR_DATA_CMP_LT(d_std, h_std, std_temp) &&
         SENSOR_DATA_CMP_GT(d_std, l_std, std_temp));

    return trigger;
}

static uint8_t
sensor_window_cmp_light(struct sensor_light_data *d_sld,
                        struct sensor_light_data *h_sld,
                        struct sensor_light_data *l_sld)
{
    uint8_t trigger;

    trigger = 0;

    trigger =
        (SENSOR_DATA_CMP_LT(d_sld, h_sld, sld_full) &&
         SENSOR_DATA_CMP_GT(d_sld, l_sld, sld_full));
    trigger |=
        (SENSOR_DATA_CMP_LT(d_sld, h_sld, sld_ir) &&
         SENSOR_DATA_CMP_GT(d_sld, l_sld, sld_ir));
    trigger |=
        (SENSOR_DATA_CMP_LT(d_sld, h_sld, sld_lux) &&
         SENSOR_DATA_CMP_GT(d_sld, l_sld, sld_lux));

    return trigger;
}

static uint8_t
sensor_window_cmp_color(struct sensor_color_data *d_scd,
                        struct sensor_color_data *h_scd,
                        struct sensor_color_data *l_scd)
{
    uint8_t trigger;

    trigger = 0;

    trigger =
        (SENSOR_DATA_CMP_LT(d_scd, h_scd, scd_r) &&
         SENSOR_DATA_CMP_GT(d_scd, l_scd, scd_r));
    trigger |=
        (SENSOR_DATA_CMP_LT(d_scd, h_scd, scd_g) &&
         SENSOR_DATA_CMP_GT(d_scd, l_scd, scd_g));
    trigger |=
        (SENSOR_DATA_CMP_LT(d_scd, h_scd, scd_b) &&
         SENSOR_DATA_CMP_GT(d_scd, l_scd, scd_b));
    trigger |=
        (SENSOR_DATA_CMP_LT(d_scd, h_scd, scd_c) &&
         SENSOR_DATA_CMP_GT(d_scd, l_scd, scd_c));
    trigger |=
        (SENSOR_DATA_CMP_LT(d_scd, h_scd, scd_lux) &&
         SENSOR_DATA_CMP_GT(d_scd, l_scd, scd_lux));
    trigger |=
        (SENSOR_DATA_CMP_LT(d_scd, h_scd, scd_colortemp) &&
         SENSOR_DATA_CMP_GT(d_scd, l_scd, scd_colortemp));
    trigger |=
        (SENSOR_DATA_CMP_LT(d_scd, h_scd, scd_ir) &&
         SENSOR_DATA_CMP_GT(d_scd, l_scd, scd_ir));

    return trigger;
}

static uint8_t
sensor_window_cmp_press(struct sensor_press_data *d_spd,
                        struct sensor_press_data *h_spd,
                        struct sensor_press_data *l_spd)
{
    uint8_t trigger;

    trigger = 0;

    trigger =
        (SENSOR_DATA_CMP_LT(d_spd, h_spd, spd_press) &&
         SENSOR_DATA_CMP_GT(d_spd, l_spd, spd_press));

    return trigger;
}

static uint8_t
sensor_window_cmp_humid(struct sensor_humid_data *d_shd,
                        struct sensor_humid_data *h_shd,
                        struct sensor_humid_data *l_shd)
{
    uint8_t trigger;

    trigger = 0;

    trigger =
        (SENSOR_DATA_CMP_LT(d_shd, h_shd, shd_humid) &&
         SENSOR_DATA_CMP_GT(d_shd, l_shd, shd_humid));

    return trigger;
}

static int
sensor_window_cmp(sensor_type_t type, sensor_data_t *low, sensor_data_t *high,
                  void *data)
{
    uint8_t trigger;
    sensor_data_t dptr;

    trigger = 0;

    switch(type) {
        case SENSOR_TYPE_ROTATION_VECTOR:
            dptr.sqd = data;
            trigger = sensor_window_cmp_quat(dptr.sqd, high->sqd, low->sqd);
            break;
        case SENSOR_TYPE_ACCELEROMETER:
            dptr.sad = data;
            trigger = sensor_window_cmp_accel(dptr.sad, high->sad, low->sad);
            break;
        case SENSOR_TYPE_LINEAR_ACCEL:
            dptr.slad = data;
            trigger = sensor_window_cmp_accel(dptr.slad, high->slad, low->slad);
            break;
        case SENSOR_TYPE_EULER:
            dptr.sed = data;
            trigger = sensor_window_cmp_euler(dptr.sed, high->sed, low->sed);
            break;
        case SENSOR_TYPE_GYROSCOPE:
            dptr.sgd = data;
            trigger = sensor_window_cmp_gyro(dptr.sgd, high->sgd, low->sgd);
            break;
        case SENSOR_TYPE_GRAVITY:
            dptr.sgrd = data;
            trigger = sensor_window_cmp_accel(dptr.sgrd, high->sgrd, low->sgrd);
            break;
        case SENSOR_TYPE_MAGNETIC_FIELD:
            dptr.smd = data;
            trigger = sensor_window_cmp_mag(dptr.smd, high->smd, low->smd);
            break;
        case SENSOR_TYPE_TEMPERATURE:
            dptr.std = data;
            trigger = sensor_window_cmp_temp(dptr.std, high->std, low->std);
            break;
        case SENSOR_TYPE_AMBIENT_TEMPERATURE:
            dptr.satd = data;
            trigger = sensor_window_cmp_temp(dptr.satd, high->satd, low->satd);
            break;
        case SENSOR_TYPE_LIGHT:
            dptr.sld = data;
            trigger = sensor_window_cmp_light(dptr.sld, high->sld, low->sld);
            break;
        case SENSOR_TYPE_COLOR:
            dptr.scd = data;
            trigger = sensor_window_cmp_color(dptr.scd, high->scd, low->scd);
            break;
        case SENSOR_TYPE_PRESSURE:
            dptr.spd = data;
            trigger = sensor_window_cmp_press(dptr.spd, high->spd, low->spd);
            break;
        case SENSOR_TYPE_RELATIVE_HUMIDITY:
            dptr.srhd = data;
            trigger = sensor_window_cmp_humid(dptr.srhd, high->srhd, low->srhd);
            break;
        case SENSOR_TYPE_PROXIMITY:
            /* Falls Through */
        case SENSOR_TYPE_WEIGHT:
            /* Falls Through */
        case SENSOR_TYPE_ALTITUDE:
            /* Falls Through */
        case SENSOR_TYPE_NONE:
            /* Falls Through */
        default:
            break;
    }

    return trigger;
}

static uint8_t
sensor_watermark_cmp_quat(struct sensor_quat_data *d_sqd,
                          struct sensor_quat_data *h_sqd,
                          struct sensor_quat_data *l_sqd)
{
    uint8_t trigger;

    trigger = 0;

    trigger =
        (SENSOR_DATA_CMP_LT(d_sqd, l_sqd, sqd_x) ||
         SENSOR_DATA_CMP_GT(d_sqd, h_sqd, sqd_x));
    trigger |=
        (SENSOR_DATA_CMP_LT(d_sqd, l_sqd, sqd_y) ||
         SENSOR_DATA_CMP_GT(d_sqd, h_sqd, sqd_y));
    trigger |=
        (SENSOR_DATA_CMP_LT(d_sqd, l_sqd, sqd_z) ||
         SENSOR_DATA_CMP_GT(d_sqd, h_sqd, sqd_z));

    return trigger;
}

static uint8_t
sensor_watermark_cmp_accel(struct sensor_accel_data *d_sad,
                           struct sensor_accel_data *h_sad,
                           struct sensor_accel_data *l_sad)
{
    uint8_t trigger;

    trigger = 0;

    trigger =
        (SENSOR_DATA_CMP_LT(d_sad, l_sad, sad_x) ||
         SENSOR_DATA_CMP_GT(d_sad, h_sad, sad_x));
    trigger |=
        (SENSOR_DATA_CMP_LT(d_sad, l_sad, sad_y) ||
         SENSOR_DATA_CMP_GT(d_sad, h_sad, sad_y));
    trigger |=
        (SENSOR_DATA_CMP_LT(d_sad, l_sad, sad_z) ||
         SENSOR_DATA_CMP_GT(d_sad, h_sad, sad_z));

    return trigger;
}

static uint8_t
sensor_watermark_cmp_euler(struct sensor_euler_data *d_sed,
                           struct sensor_euler_data *h_sed,
                           struct sensor_euler_data *l_sed)
{
    uint8_t trigger;

    trigger = 0;

    trigger =
        (SENSOR_DATA_CMP_LT(d_sed, l_sed, sed_h) ||
         SENSOR_DATA_CMP_GT(d_sed, h_sed, sed_h));
    trigger |=
        (SENSOR_DATA_CMP_LT(d_sed, l_sed, sed_r) ||
         SENSOR_DATA_CMP_GT(d_sed, h_sed, sed_r));
    trigger |=
        (SENSOR_DATA_CMP_LT(d_sed, l_sed, sed_p) ||
         SENSOR_DATA_CMP_GT(d_sed, h_sed, sed_p));

    return trigger;
}

static uint8_t
sensor_watermark_cmp_gyro(struct sensor_gyro_data *d_sgd,
                          struct sensor_gyro_data *h_sgd,
                          struct sensor_gyro_data *l_sgd)
{
    uint8_t trigger;

    trigger = 0;

    trigger =
        (SENSOR_DATA_CMP_LT(d_sgd, l_sgd, sgd_x) ||
         SENSOR_DATA_CMP_GT(d_sgd, h_sgd, sgd_x));
    trigger |=
        (SENSOR_DATA_CMP_LT(d_sgd, l_sgd, sgd_y) ||
         SENSOR_DATA_CMP_GT(d_sgd, h_sgd, sgd_y));
    trigger |=
        (SENSOR_DATA_CMP_LT(d_sgd, l_sgd, sgd_z) ||
         SENSOR_DATA_CMP_GT(d_sgd, h_sgd, sgd_z));

    return trigger;
}

static uint8_t
sensor_watermark_cmp_mag(struct sensor_mag_data *d_smd,
                         struct sensor_mag_data *h_smd,
                         struct sensor_mag_data *l_smd)
{
    uint8_t trigger;

    trigger = 0;

    trigger =
         (SENSOR_DATA_CMP_LT(d_smd, l_smd, smd_x) ||
          SENSOR_DATA_CMP_GT(d_smd, h_smd, smd_x));
    trigger |=
         (SENSOR_DATA_CMP_LT(d_smd, l_smd, smd_y) ||
          SENSOR_DATA_CMP_GT(d_smd, h_smd, smd_y));
    trigger |=
         (SENSOR_DATA_CMP_LT(d_smd, l_smd, smd_z) ||
          SENSOR_DATA_CMP_GT(d_smd, h_smd, smd_z));

    return trigger;
}

static uint8_t
sensor_watermark_cmp_temp(struct sensor_temp_data *d_std,
                          struct sensor_temp_data *h_std,
                          struct sensor_temp_data *l_std)
{
    uint8_t trigger;

    trigger = 0;

    trigger =
        (SENSOR_DATA_CMP_LT(d_std, l_std, std_temp) ||
         SENSOR_DATA_CMP_GT(d_std, h_std, std_temp));

    return trigger;
}

static uint8_t
sensor_watermark_cmp_light(struct sensor_light_data *d_sld,
                           struct sensor_light_data *h_sld,
                           struct sensor_light_data *l_sld)
{
    uint8_t trigger;

    trigger = 0;

    trigger =
        (SENSOR_DATA_CMP_LT(d_sld, l_sld, sld_full) ||
         SENSOR_DATA_CMP_GT(d_sld, h_sld, sld_full));
    trigger |=
        (SENSOR_DATA_CMP_LT(d_sld, l_sld, sld_ir) ||
         SENSOR_DATA_CMP_GT(d_sld, h_sld, sld_ir));
    trigger |=
        (SENSOR_DATA_CMP_LT(d_sld, l_sld, sld_lux) ||
         SENSOR_DATA_CMP_GT(d_sld, h_sld, sld_lux));

    return trigger;
}

static uint8_t
sensor_watermark_cmp_color(struct sensor_color_data *d_scd,
                           struct sensor_color_data *h_scd,
                           struct sensor_color_data *l_scd)
{
    uint8_t trigger;

    trigger = 0;

    trigger =
        (SENSOR_DATA_CMP_LT(d_scd, l_scd, scd_r) ||
         SENSOR_DATA_CMP_GT(d_scd, h_scd, scd_r));
    trigger |=
        (SENSOR_DATA_CMP_LT(d_scd, l_scd, scd_g) ||
         SENSOR_DATA_CMP_GT(d_scd, h_scd, scd_g));
    trigger |=
        (SENSOR_DATA_CMP_LT(d_scd, l_scd, scd_b) ||
         SENSOR_DATA_CMP_GT(d_scd, h_scd, scd_b));
    trigger |=
        (SENSOR_DATA_CMP_LT(d_scd, l_scd, scd_c) ||
         SENSOR_DATA_CMP_GT(d_scd, h_scd, scd_c));
    trigger |=
        (SENSOR_DATA_CMP_LT(d_scd, l_scd, scd_lux) ||
         SENSOR_DATA_CMP_GT(d_scd, h_scd, scd_lux));
    trigger |=
        (SENSOR_DATA_CMP_LT(d_scd, l_scd, scd_colortemp) ||
         SENSOR_DATA_CMP_GT(d_scd, h_scd, scd_colortemp));
    trigger |=
        (SENSOR_DATA_CMP_LT(d_scd, l_scd, scd_ir) ||
         SENSOR_DATA_CMP_GT(d_scd, h_scd, scd_ir));

    return trigger;
}

static uint8_t
sensor_watermark_cmp_press(struct sensor_press_data *d_spd,
                           struct sensor_press_data *h_spd,
                           struct sensor_press_data *l_spd)
{
    uint8_t trigger;

    trigger = 0;

    trigger =
        (SENSOR_DATA_CMP_LT(d_spd, l_spd, spd_press) ||
         SENSOR_DATA_CMP_GT(d_spd, h_spd, spd_press));

    return trigger;
}

static uint8_t
sensor_watermark_cmp_humid(struct sensor_humid_data *d_shd,
                           struct sensor_humid_data *h_shd,
                           struct sensor_humid_data *l_shd)
{
    uint8_t trigger;

    trigger = 0;

    trigger =
        (SENSOR_DATA_CMP_LT(d_shd, l_shd, shd_humid) ||
         SENSOR_DATA_CMP_GT(d_shd, h_shd, shd_humid));

    return trigger;
}

static int
sensor_watermark_cmp(sensor_type_t type, sensor_data_t *low, sensor_data_t *high,
                     void *data)
{
    uint8_t trigger;
    sensor_data_t dptr;

    trigger = 0;

    switch(type) {
        case SENSOR_TYPE_ROTATION_VECTOR:
            dptr.sqd = data;
            trigger = sensor_watermark_cmp_quat(dptr.sqd, high->sqd,
                                                low->sqd);
            break;
        case SENSOR_TYPE_ACCELEROMETER:
            dptr.sad = data;
            trigger = sensor_watermark_cmp_accel(dptr.sad, high->sad,
                                                 low->sad);
            break;
        case SENSOR_TYPE_LINEAR_ACCEL:
            dptr.slad = data;
            trigger = sensor_watermark_cmp_accel(dptr.slad, high->slad,
                                                 low->slad);
            break;
        case SENSOR_TYPE_EULER:
            dptr.sed = data;
            trigger = sensor_watermark_cmp_euler(dptr.sed, high->sed,
                                                 low->sed);
            break;
        case SENSOR_TYPE_GYROSCOPE:
            dptr.sgd = data;
            trigger = sensor_watermark_cmp_gyro(dptr.sgd, high->sgd,
                                                low->sgd);
            break;
        case SENSOR_TYPE_GRAVITY:
            dptr.sgrd = data;
            trigger = sensor_watermark_cmp_accel(dptr.sgrd, high->sgrd,
                                                 low->sgrd);
            break;
        case SENSOR_TYPE_MAGNETIC_FIELD:
            dptr.smd = data;
            trigger = sensor_watermark_cmp_mag(dptr.smd, high->smd,
                                               low->smd);
            break;
        case SENSOR_TYPE_TEMPERATURE:
            dptr.std = data;
            trigger = sensor_watermark_cmp_temp(dptr.std, high->std,
                                                low->std);
            break;
        case SENSOR_TYPE_AMBIENT_TEMPERATURE:
            dptr.satd = data;
            trigger = sensor_watermark_cmp_temp(dptr.satd, high->satd,
                                                low->satd);
            break;
        case SENSOR_TYPE_LIGHT:
            dptr.sld = data;
            trigger = sensor_watermark_cmp_light(dptr.sld, high->sld,
                                                 low->sld);
            break;
        case SENSOR_TYPE_COLOR:
            dptr.scd = data;
            trigger = sensor_watermark_cmp_color(dptr.scd, high->scd,
                                                 low->scd);
            break;
        case SENSOR_TYPE_PRESSURE:
            dptr.spd = data;
            trigger = sensor_watermark_cmp_press(dptr.spd, high->spd,
                                                 low->spd);
            break;
        case SENSOR_TYPE_RELATIVE_HUMIDITY:
            dptr.srhd = data;
            trigger = sensor_watermark_cmp_humid(dptr.srhd, high->srhd,
                                                 low->srhd);
            break;
        case SENSOR_TYPE_PROXIMITY:
            /* Falls Through */
        case SENSOR_TYPE_WEIGHT:
            /* Falls Through */
        case SENSOR_TYPE_ALTITUDE:
            /* Falls Through */
        case SENSOR_TYPE_NONE:
            /* Falls Through */
        default:
            break;
    }

    return trigger;
}

static void
sensor_set_trigger_cmp_algo(struct sensor *sensor, struct sensor_type_traits *stt)
{
    sensor_lock(sensor);
    if (stt->stt_algo == SENSOR_THRESH_ALGO_WATERMARK) {
        /* select watermark comparison algo */
        stt->stt_trigger_cmp_algo = sensor_watermark_cmp;
    } else if (stt->stt_algo == SENSOR_THRESH_ALGO_WINDOW) {
        /* select window comparison algo */
        stt->stt_trigger_cmp_algo = sensor_window_cmp;
    } else if (stt->stt_algo == SENSOR_THRESH_ALGO_USERDEF) {
        /* select user defined comparison algo if any */
        stt->stt_trigger_cmp_algo = stt->stt_trigger_cmp_algo;
    }
    sensor_unlock(sensor);
}

/**
 * Set the thresholds along with comparison algo for a sensor
 *
 * @param name of the sensor
 * @param Ptr to sensor threshold
 *
 * @return 0 on success, non-zero on failure
 */
int
sensor_set_thresh(char *devname, struct sensor_type_traits *stt)
{
    struct sensor_type_traits *stt_tmp;
    struct sensor *sensor;
    int rc;

    sensor = sensor_get_type_traits_byname(devname, &stt_tmp,
                                           stt->stt_sensor_type);
    if (!sensor) {
        rc = SYS_EINVAL;
        goto err;
    }

    if (!stt_tmp && stt) {
        sensor_lock(sensor);
        SLIST_INSERT_HEAD(&sensor->s_type_traits_list, stt, stt_next);
        stt_tmp = stt;
        sensor_unlock(sensor);
    } else if (stt_tmp) {
        sensor_lock(sensor);
        stt_tmp->stt_low_thresh = stt->stt_low_thresh;
        stt_tmp->stt_high_thresh = stt->stt_high_thresh;
        stt_tmp->stt_algo = stt->stt_algo;
        sensor_unlock(sensor);
    } else {
        rc = SYS_EINVAL;
        goto err;
    }

    sensor_set_trigger_cmp_algo(sensor, stt_tmp);

    if (sensor->s_funcs->sd_set_trigger_thresh) {
        rc = sensor->s_funcs->sd_set_trigger_thresh(sensor,
                                                    stt_tmp->stt_sensor_type,
                                                    stt_tmp);
        if (rc) {
            goto err;
        }
    }

    return 0;
err:
    return rc;
}

static int
sensor_generate_trig(struct sensor *sensor,
                     void *arg, void *data,
                     sensor_type_t type)
{
    struct sensor_type_traits *stt;
    sensor_data_t low_thresh;
    sensor_data_t high_thresh;
    uint8_t tx_trigger;
    sensor_trigger_notify_func_t notify;

    if (!arg) {
        return SYS_EINVAL;
    }

    notify = arg;
    stt = sensor_get_type_traits_bytype(type, sensor);

    tx_trigger = 0;

    memcpy(&low_thresh, &stt->stt_low_thresh, sizeof(low_thresh));
    memcpy(&high_thresh, &stt->stt_high_thresh, sizeof(high_thresh));

    if (stt->stt_trigger_cmp_algo) {
        tx_trigger = stt->stt_trigger_cmp_algo(type, &low_thresh,
                                               &high_thresh, data);
    }

    return tx_trigger ? notify(sensor, data, type): 0;
}

/**
 * Sensor trigger initialization
 *
 * @param ptr to the sensor sturucture
 * @param sensor type to enable trigger for
 * @param the function to call if the trigger condition is satisfied
 */
void
sensor_trigger_init(struct sensor *sensor, sensor_type_t type,
                    sensor_trigger_notify_func_t notify)
{
    int rc;
    struct sensor_listener *sensor_trig_lner;

    sensor_trig_lner = malloc(sizeof(struct sensor_listener));
    assert(sensor_trig_lner != NULL);

    sensor_trig_lner->sl_func = sensor_generate_trig;
    sensor_trig_lner->sl_sensor_type = type;
    sensor_trig_lner->sl_arg = (void *)notify;

    rc = sensor_register_listener(sensor, sensor_trig_lner);
    if (rc) {
        return;
    }
}

/**
 * Read the data for sensor type "type," from the given sensor and
 * return the result into the "value" parameter.
 *
 * @param The sensor to read data from
 * @param The type of sensor data to read from the sensor
 * @param The callback to call for data returned from that sensor
 * @param The argument to pass to this callback.
 * @param Timeout before aborting sensor read
 *
 * @return 0 on success, non-zero on failure.
 */
int
sensor_read(struct sensor *sensor, sensor_type_t type,
        sensor_data_func_t data_func, void *arg, uint32_t timeout)
{
    struct sensor_read_ctx src;
    int rc;

    rc = sensor_lock(sensor);
    if (rc) {
        goto err;
    }

    src.user_func = data_func;
    src.user_arg = arg;

    if (!sensor_mgr_match_bytype(sensor, (void *)&type)) {
        rc = SYS_ENOENT;
        goto err;
    }

    sensor_up_timestamp(sensor);

    rc = sensor->s_funcs->sd_read(sensor, type, sensor_read_data_func, &src,
                                  timeout);
    if (rc) {
        goto err;
    }

err:
    sensor_unlock(sensor);
    return (rc);
}

