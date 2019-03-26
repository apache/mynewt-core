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

#include <os/mynewt.h>

#if MYNEWT_VAL(TIMEPERSIST_SYS_CONFIG)

#include <hal/hal_system.h>
#include <config/config.h>
#include <datetime/datetime.h>

static struct os_callout timepersist_timer;

static int timepersist_conf_set(int argc, char **argv, char *val);
static struct conf_handler timepersist_conf = {
    .ch_name = "time",
    .ch_set = timepersist_conf_set,
};

/*
 * Called when config is read in. This persists timezone info also.
 */
static int
timepersist_conf_set(int argc, char **argv, char *val)
{
    struct os_timeval tv;
    struct os_timezone tz;

    if (argc != 1) {
        return OS_ENOENT;
    }
    if (!conf_set_from_storage()) {
        /*
         * Only allow sys/config to set time using this interface.
         */
        return OS_ERR_PRIV;
    }
    if (!strcmp(argv[0], "s")) {
        if (!val) {
            memset(&tv, 0, sizeof(tv));
            memset(&tz, 0, sizeof(tz));
            os_settimeofday(&tv, &tz);
        } else if (!datetime_parse(val, &tv, &tz)) {
            if (hal_reset_cause() == HAL_RESET_POR) {
                /*
                 * In this case the stored time could be off considerably.
                 * Schedule an immediate callout which will clear out the
                 * stored time.
                 */
                os_callout_reset(&timepersist_timer, 0);
            } else {
                os_settimeofday(&tv, &tz);
            }
        }
        return OS_OK;
    }
    return OS_ENOENT;
}

void
timepersist(void)
{
    struct os_timeval tv;
    struct os_timezone tz;
    char str[DATETIME_BUFSIZE];

    if (os_time_is_set()) {
        os_gettimeofday(&tv, &tz);
        if (!datetime_format(&tv, &tz, str, DATETIME_BUFSIZE)) {
            conf_save_one("time/s", str);
        }
    } else if (hal_reset_cause() == HAL_RESET_POR) {
        conf_save_one("time/s", NULL);
    }
}

static void
timepersist_tmo(struct os_event *ev)
{
    timepersist();
    os_callout_reset(&timepersist_timer,
                     MYNEWT_VAL(TIMEPERSIST_FREQ) * OS_TICKS_PER_SEC);
}

/*
 * Periodically store system wallclock to non-volatile storage.
 */
void
timepersist_init(void)
{
    conf_register(&timepersist_conf);

    os_callout_init(&timepersist_timer, os_eventq_dflt_get(), timepersist_tmo,
                    NULL);
    os_callout_reset(&timepersist_timer,
                     MYNEWT_VAL(TIMEPERSIST_FREQ) * OS_TICKS_PER_SEC);
}

#endif
