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

TAILQ_HEAD(, struct sensor) g_sensor_list =
    TAILQ_HEAD_INITIALIZER(&g_sensor_list);

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
sensor_find_next(sensor_type_t type, struct sensor *prev_cursor)
{
    struct sensor *cursor;

    cursor = prev_cursor;
    if (cursor == NULL) {
        cursor = TAILQ_FIRST(&g_sensor_list);
    }

    while (cursor != NULL) {
        if ((cursor->s_type & type) != 0) {
            return (cursor);
        }
        cursor = TAILQ_NEXT(cursor);
    }

    return (NULL);
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
 * @param sensor The sensor to initialize
 *
 * @return 0 on success, non-zero error code on failure.
 */
int
sensor_init(struct sensor *sensor)
{
    memset(sensor, 0, sizeof(*sensor));

    rc = os_mutex_init(&sensor->s_lock);
    if (rc != 0) {
        goto err;
    }
    return (0);
err:
    return (rc);
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
sensor_register(struct sensor *sensor)
{
    return (0);
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

static os_time_t
sensor_poll_run(struct sensor *sensor, os_time_t now)
{
    struct sensor_listener *listener;
    struct sensor_value value;
    os_time_t next_run;
    int rc;

    next_run = OS_TIMEOUT_NEVER;

    SLIST_FOREACH(listener, &sensor->s_listener_list, sl_next) {
        if (listener->sl_type == SENSOR_LISTENER_TYPE_NOTIFY) {
            continue;
        }
        if (listener->sl_next_run >= now) {
            rc |= sensor_read(sensor, type, &value);
            rc |= listener->sl_func(sensor, listener, value);

            listener->sl_next_run = os_time_get() +
                os_time_ms_to_ticks(sl->sl_poll_rate_ms);
            if (listener->sl_next_run < next_run) {
                next_run = listener->sl_next_run;
            }
        }
    }
    sensor->s_next_wakeup = next_run;

    return (next_run);
}


static void
sensor_poll_event(struct os_event *ev)
{
    struct sensor *cursor;
    os_time_t time;
    os_time_t next_wakeup;
    os_time_t sensor_task_next_wakeup;

    time = os_time_get();

    sensor_next_wakeup = time + SENSOR_DEFAULT_POLL_RATE;

    TAILQ_FOREACH(cursor, &g_sensor_list, s_next) {
        if (cursor->s_next_wakeup == OS_TIMEOUT_NEVER) {
            break;
        }

        /* Sort lowest to highest, so when we encounter a entry in the list
         * that isn't ready, we can exit.
         */
        if (cursor->s_poll_expire < time) {
            break;
        }

        next_wakeup = sensor_poll_run(cursor, now);
        if (next_wakeup < sensor_task_next_wakeup) {
            sensor_task_next_wakeup = next_wakeup;
        }
    }

    os_callout_reset(&sensor_callout, sensor_task_next_wakeup);
}

void
sensor_pkg_init(void)
{
    /* Ensure this is only called by sysinit */
    SYSINIT_ASSERT_ACTIVE();

    /**
     * Initialize callout and set it to fire on boot.
     */
    os_callout_init(&sensors_callout, sensor_evq_get(), sensors_poll_event,
            NULL);
    os_callout_reset(&sensors_callout, 0);
}
