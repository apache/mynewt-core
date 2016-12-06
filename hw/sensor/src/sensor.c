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

#include <errno.h>
#include <assert.h>

#include "sensor/sensor.h"

struct os_callout sensors_callout;

struct {
    struct os_mutex mgr_lock;

    struct os_callout mgr_poll_callout;

    TAILQ_HEAD(, struct sensor) mgr_sensor_list;
} sensor_mgr;

static int
sensor_mgr_lock(void)
{
    int rc;

    rc = os_mutex_pend(&sensor_mgr.mgr_lock);
    if (rc == 0 || rc == OS_NOT_STARTED) {
        return (0);
    }
    return (rc);
}

static void
sensor_mgr_unlock(void)
{
    (void) os_mutex_release(&sensor_mgr.mgr_lock);
}

static void
sensor_mgr_remove(struct sensor *sensor)
{
    TAILQ_REMOVE(&sensor_mgr.mgr_sensor_list, sensor, s_next);
}

static void
sensor_mgr_insert(struct sensor *sensor)
{
    struct sensor *cursor;

    cursor = NULL;
    TAILQ_FOREACH(cursor, &sensor_mgr.mgr_sensor_list, s_next) {
        if (cursor->s_next_wakeup == OS_TIMEOUT_NEVER) {
            break;
        }

        if (OS_TIME_TICK_LT(sensor->s_next_wakeup, cursor->s_next_wakeup)) {
            break;
        }
    }

    if (cursor) {
        TAILQ_INSERT_BEFORE(cursor, sensor, s_next);
    } else {
        TAILQ_INSERT_TAIL(&sensor_mgr.mgr_sensor_list, s_next);
    }
}

static os_time_t
sensor_mgr_poll_one(struct sensor *sensor, os_time_t now)
{
    int rc;

    rc = sensor_lock(sensor);
    if (rc != 0) {
        goto err;
    }

    /* Sensor read results.  Every time a sensor is read, all of its
     * listeners are called by default.  Specify NULL as a callback,
     * because we just want to run all the listeners.
     */
    sensor_read(sensor, SENSOR_TYPE_ALL, NULL, NULL, OS_TIMEOUT_NEVER);

    /* Remove the sensor from the sensor list for insertion sort. */
    sensor_mgr_remove(sensor);

    /* Set next wakeup, and insertion sort the sensor back into the
     * list.
     */
    sensor->s_next_run = now + os_time_ms_to_ticks(sensor->s_poll_rate);

    /* Re-insert the sensor manager, with the new wakeup time. */
    sensor_mgr_insert(sensor);

    /* Unlock the sensor to allow other access */
    sensor_unlock(sensor);

    return (sensor->s_next_run);
err:
    /* Couldn't lock it.  Re-run task and spin until we get result. */
    return (0);
}

/**
 * Event that wakes up the sensor manager, this goes through the sensor
 * list and polls any active sensors.
 */
static void
sensor_mgr_wakeup_event(struct os_event *ev)
{
    struct sensor *cursor;
    os_time_t now;
    os_time_t task_next_wakeup;
    int rc;

    rc = sensor_mgr_lock();
    if (rc != 0) {
        goto err;
    }

    now = os_time_get();
    task_next_wakeup = now + SENSOR_MGR_WAKEUP_TICKS;

    TAILQ_FOREACH(cursor, &sensor_mgr.mgr_sensor_list, s_next) {
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
         * and re-inserts it into the list.  It returns the next wakeup time
         * for this sensor.
         */
        next_wakeup = sensor_poll_one(cursor, now);

        /* If the next wakeup time for this sensor is before the task's next
         * scheduled wakeup, move that forward, so we can collect data from that
         * sensor
         */
        if (task_next_wakeup > next_wakeup) {
            task_next_wakeup = next_wakeup;
        }
    }

    os_callout_reset(&sensor_mgr.mgr_wakeup_callout, task_next_wakeup);
}




static void
sensor_mgr_init(void)
{
    memset(&sensor_mgr, 0, sizeof(sensor_mgr));
    TAILQ_INIT(&sensor_mgr.mgr_sensor_list);
    /**
     * Initialize sensor polling callout and set it to fire on boot.
     */
    os_callout_init(&sensor_mgr.mgr_wakeup_callout, sensor_mgr_evq_get(),
            sensor_mgr_wakeup_event, NULL);
    os_callout_reset(&sensor_mgr.mgr_wakeup_callout, 0);

    os_mutex_init(&sensor_mgr.mgr_lock);
}


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
        cursor = TAILQ_FIRST(&g_sensor_list);
    }

    while (cursor != NULL) {
        if (compare_func(cursor, arg)) {
            break;
        }
    }

    sensor_mgr_unlock();

done:
    return (cursor);
}


static int
sensor_mgr_match_bytype(struct sensor *sensor, void *arg)
{
    sensor_type_t *type;

    type = (sensor_type_t *) arg;

    if ((*type & sensor->s_type) != 0) {
        return (1);
    } else {
        return (0);
    }
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
    return (sensor_mgr_find_next(sensor_mgr_match_bytype, type, prev_cursor));
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
 *
 */
struct sensor *
sensor_mgr_find_next_bydevname(char *devname, struct sensor *prev_cursor)
{
    return (sensor_mgr_find_next(sensor_mgr_match_bydevname, devname,
            prev_cursor));
}


/**
 * Register the sensor with the global sensor list.   This makes the sensor searchable
 * by other packages, who may want to look it up by type.
 *
 * @param The sensor to register
 *
 * @return 0 on success, non-zero error code on failure.
 */
int
sensor_mgr_register(struct sensor *sensor)
{
    return (0);
}


void
sensor_pkg_init(void)
{
    /* Ensure this is only called by sysinit */
    SYSINIT_ASSERT_ACTIVE();

    sensor_mgr_init();
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
    if (rc != 0) {
        goto err;
    }
    return (0);
err:
    return (rc);
}

/**
 * Unlock access to the sensor specified by sensor.  Blocks until lock acquired.
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
    memset(sensor, 0, sizeof(*sensor));

    rc = os_mutex_init(&sensor->s_lock);
    if (rc != 0) {
        goto err;
    }
    sensor->s_dev = dev;

    return (0);
err:
    return (rc);
}



/**
 * Register a sensor listener.  This allows a calling application to receive
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

struct sensor_read_ctx {
    sensor_data_func_t user_func;
    void *user_arg;
};

static int
sensor_read_data_func(struct sensor *sensor, void *arg, void *data)
{
    struct sensor_listener *listener;
    struct sensor_read_ctx *ctx;

    /* Notify all listeners first */
    SLIST_FOREACH(listener, &sensor->s_listener_list, sl_next) {
        listener->sl_func(sensor, listener->sl_arg, data);
    }

    /* Call data function */
    ctx = (struct sensor_read_ctx *) arg;
    if (ctx->user_func != NULL) {
        return (ctx->user_func(sensor, ctx->user_arg, data));
    } else {
        return (0);
    }
}

/**
 * Read the data for sensor type "type," from the sensor, "sensor" and
 * return the result into the "value" parameter.
 *
 * @param The senssor to read data from
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
    if (rc != 0) {
        goto done;
    }

    src.user_func = data_func;
    src.user_arg = arg;

    rc = sensor->s_funcs.sd_read(sensor, type, data_func, arg, timeout);

    sensor_unlock(sensor);

done:
    return (rc);
}

