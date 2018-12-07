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

#include "os/mynewt.h"
#include <sys/queue.h>
#include <battery/battery.h>
#include <battery/battery_prop.h>
#include <battery/battery_drv.h>

#define BATTERY_MAX_COUNT 1

#define GET_BIT(arr, bit)   ((arr)[(bit) / 32] & (1 << ((bit) % 32)))

/*
 * Structure holds information about listener and what is it listening for.
 */
struct listener_data
{
    /* The properties to monitor for change */
    uint32_t ld_prop_change_mask[BATTERY_PROPERTY_MASK_SIZE];
    /* The properties to monitor for periodic read */
    uint32_t ld_prop_read_mask[BATTERY_PROPERTY_MASK_SIZE];
    /* Pointer to listener provided by the application. */
    struct battery_prop_listener *ld_listener;
};

struct battery_manager
{
    /* The lock for battery object */
    struct os_mutex bm_lock;

    struct battery *bm_batteries[BATTERY_MAX_COUNT];

    /** Event queue */
    struct os_eventq *bm_eventq;

    /** Manages poll rates of the batteries */
    struct os_callout bm_poll_callout;

} battery_manager;

struct os_eventq *
battery_mgr_evq_get(void)
{
    return (battery_manager.bm_eventq);
}

static void
battery_mgr_evq_set(struct os_eventq *evq)
{
    assert(evq != NULL);
    battery_manager.bm_eventq = evq;
}

static void battery_mgr_poll_battery(struct battery *battery);

static void
battery_poll_event_cb(struct os_event *ev)
{
    int i;
    int pflag;
    os_time_t next_poll = 0;
    os_time_t now = os_time_get();
    struct battery *bat;
    os_stime_t ticks;

    pflag = 0;
    for (i = 0; i < BATTERY_MAX_COUNT; ++i) {
        bat = battery_manager.bm_batteries[i];
        if (bat) {
            if (OS_TIME_TICK_GEQ(now, bat->b_next_run)) {
                bat->b_last_read_time = now;
                battery_mgr_poll_battery(battery_manager.bm_batteries[i]);
                bat->b_next_run = now + os_time_ms_to_ticks32(bat->b_poll_rate);
            }
            if ((pflag == 0) || OS_TIME_TICK_LT(bat->b_next_run, next_poll)) {
                pflag = 1;
                next_poll = bat->b_next_run;
            }
        }
    }

    if (pflag) {
        ticks = (os_stime_t)(next_poll - os_time_get());
        if (ticks < 0) {
            ticks = 1;
        }
        os_callout_reset(&battery_manager.bm_poll_callout, (os_time_t)ticks);
    }
}

void
battery_mgr_process_event(struct os_event *event)
{
    os_eventq_put(battery_manager.bm_eventq, event);
}

static void
battery_mgr_init(void)
{
#ifdef MYNEWT_VAL_BATTERY_MGR_EVQ
    battery_mgr_evq_set(MYNEWT_VAL(BATTERY_MGR_EVQ));
#else
    battery_mgr_evq_set(os_eventq_dflt_get());
#endif

    /**
     * Initialize battery manager polling callout.
     */
    os_callout_init(&battery_manager.bm_poll_callout,
            battery_mgr_evq_get(), battery_poll_event_cb,
            NULL);

    os_mutex_init(&battery_manager.bm_lock);
}

/* =================================================================
 * ====================== PKG ======================================
 * =================================================================
 */

void
battery_pkg_init(void)
{
    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    battery_mgr_init();

#if MYNEWT_VAL(BATTERY_SHELL)
    battery_shell_register();
#endif
}

int
battery_mgr_get_battery_count(void)
{
    int i;

    for (i = 0; i < BATTERY_MAX_COUNT; ++i) {
        if (battery_manager.bm_batteries[i] == NULL) {
            break;
        }
    }

    return i;
}

struct os_dev *
battery_get_battery(int bat_num)
{
    assert(bat_num < BATTERY_MAX_COUNT);
    assert(battery_manager.bm_batteries[bat_num]);

    return &battery_manager.bm_batteries[bat_num]->b_dev;
}

static int
battery_get_num(struct battery *battery)
{
    int i;

    /* For one battery, there is no need to specify battery at all */
    if (battery == NULL) {
        return 0;
    }
    for (i = 0; i < BATTERY_MAX_COUNT; ++i) {
        if (battery_manager.bm_batteries[i] == battery) {
            return i;
        }
    }
    assert(0);
    return 0;
}

struct battery_driver *
battery_get_driver(struct os_dev *battery,
        const char *dev_name)
{
    int i;
    assert(battery);
    struct battery *bat = (struct battery *)battery;
    for (i = 0; i < BATTERY_DRIVERS_MAX && bat->b_drivers[i]; ++i) {
        if (strcmp(bat->b_drivers[i]->dev.od_name, dev_name) == 0) {
            return bat->b_drivers[i];
        }
    }
    /* No device found */
    assert(0);
    return NULL;
}

static void
set_bit(uint32_t *mask, int bit)
{
    mask[bit / (sizeof(*mask) * 8)] |=
        1 << (bit & ((sizeof(*mask) * 8) - 1));
}

static void
clear_bit(uint32_t *mask, int bit)
{
    mask[bit / (sizeof(*mask) * 8)] &=
        ~(1 << (bit & ((sizeof(*mask) * 8) - 1)));
}

static struct battery_property *
find_driver_property(struct battery *bat, struct battery_driver *driver,
        battery_property_type_t type, battery_property_flags_t flags)
{
    struct battery_property *prop = bat->b_properties + driver->bd_first_property;
    int i;
    assert(driver);

    for (i = 0; driver->bd_property_count; ++i, ++prop) {
        if (prop->bp_type == type && prop->bp_flags == flags)
            return prop;
    }
    return NULL;
}

static struct battery_property *
find_hardware_property(struct battery *battery, struct battery_driver *driver,
        battery_property_type_t type, battery_property_flags_t flags)
{
    struct battery_property *res = NULL;
    int i;

    /* Search driver properties first */
    if (driver) {
        res = find_driver_property(battery, driver, type, flags);
    } else {
        for (i = 0; res == NULL && i < BATTERY_DRIVERS_MAX; ++i) {
            res = find_driver_property(battery, battery->b_drivers[i],
                                       type, flags);
        }
    }
    return res;
}

struct battery_property *
battery_find_property(struct os_dev *battery,
        battery_property_type_t type, battery_property_flags_t flags,
        const char *dev_name)
{
    struct battery_property *res = NULL;
    struct battery_property *prop;
    struct battery *bat = (struct battery *)battery;
    int i;
    struct battery_driver *driver = NULL;

    if (dev_name) {
        driver = battery_get_driver(battery, dev_name);
        assert(driver);
    }
    /* Search hardware properties first */
    res = find_hardware_property(bat, driver, type, flags);
    if (!res) {
        /* Search battery manager created properties */
        for (i = 0; i < bat->b_all_property_count; ++i) {
            prop =  &bat->b_properties[i];
            if (prop->bp_type == type &&
                prop->bp_flags == flags &&
                (driver == NULL ||
                 bat->b_drivers[prop->bp_drv_num] == driver)) {
                res = prop;
                break;
            }
        }
    }
    if (!res) {
        /* Create software threshold */
        if ((flags & BATTERY_PROPERTY_FLAGS_CREATE) != 0 &&
            (flags & BATTERY_PROPERTY_FLAGS_ALARM_THREASH) != 0) {
            /* Find base property that threshold is for */
            prop = find_hardware_property(bat, driver, type,
                    BATTERY_PROPERTY_FLAGS_NONE);
            /* Base property found, create derived one */
            if (prop) {
                struct battery_property *p =
                    calloc(1, sizeof(struct battery_property));
                if (prop->bp_prop_num == 0) {
                    prop->bp_prop_num = ++bat->b_all_property_count;
                    assert(bat->b_all_property_count <= BATTERY_MAX_PROPERTY_COUNT);
                }
                /* Base property is marked as base for others
                 */
                prop->bp_base = 1;
                p->bp_type = type;
                p->bp_flags = flags;
                p->bp_bat_num = prop->bp_bat_num;
                p->bp_drv_num = prop->bp_drv_num;
                p->bp_prop_num = ++bat->b_all_property_count;
                assert(bat->b_all_property_count <= BATTERY_MAX_PROPERTY_COUNT);
                res = p;
            }
        }
    }
    return res;
}

int
battery_get_property_count(struct os_dev *battery,
        struct battery_driver *driver)
{
    struct battery *bat = (struct battery *)battery;
    int i;

    if (driver == NULL) {
        return bat->b_all_property_count;
    } else {
        for (i = 0; i < BATTERY_DRIVERS_MAX && bat->b_drivers[i]; ++i) {
            if (bat->b_drivers[i] == driver) {
                return bat->b_drivers[i]->bd_property_count;
            }
        }
    }

    return 0;
}

struct battery_property *
battery_enum_property(struct os_dev *battery, struct battery_driver *driver,
        uint8_t prop_num)
{
    struct battery *bat = (struct battery *)battery;
    struct battery_property *prop = NULL;
    int i;

    if (driver == NULL) {
        if (prop_num < bat->b_all_property_count) {
            prop = &bat->b_properties[prop_num];
        }
    } else {
        for (i = 0; i < BATTERY_DRIVERS_MAX && bat->b_drivers[i]; ++i) {
            if (bat->b_drivers[i] == driver) {
                if (prop_num < driver->bd_property_count) {
                    prop = &bat->b_properties[prop_num + driver->bd_first_property];
                }
            }
        }
    }

    return prop;
}

char *
battery_prop_get_name(const struct battery_property *prop, char *buf,
        size_t buf_size)
{
    struct battery *bat = battery_manager.bm_batteries[prop->bp_bat_num];
    struct battery_driver *driver = bat->b_drivers[prop->bp_drv_num];
    const struct battery_driver_property *driver_prop =
            &driver->bd_driver_properties[prop->bp_prop_num - driver->bd_first_property];

    strncpy(buf, driver_prop->bdp_name, buf_size);

    return buf;
}

struct battery_property *
battery_find_property_by_name(struct os_dev *battery, const char *name)
{
    struct battery *bat = (struct battery *)battery;
    char buf[20];
    int i;

    for (i = 0; i < bat->b_all_property_count; ++i) {
        battery_prop_get_name(&bat->b_properties[i], buf, 20);
        if (strcmp(buf, name) == 0) {
            return &bat->b_properties[i];
        }
    }
    return NULL;
}

static void
battery_mgr_poll_battery_driver(struct battery *bat,
        struct battery_driver *drv, uint32_t changed[], uint32_t queried[])
{
    int i;
    battery_property_value_t old_val;
    struct battery_property *prop = &bat->b_properties[drv->bd_first_property];

    /* Read requested properties */
    for (i = 0; i < drv->bd_property_count; ++i, ++prop) {
        if (!GET_BIT(bat->b_polled_properties, i)) {
            continue;
        }

        if (!driver_property(prop)) {
            continue;
        }

        old_val = prop->bp_value;
        if (drv->bd_funcs->bdf_property_get(drv, prop, 100)) {
            prop->bp_valid = 0;
        } else {
            prop->bp_valid = 1;
            if (memcmp(&old_val, &prop->bp_value, sizeof(old_val))) {
                set_bit(changed, prop->bp_prop_num);
            }
            set_bit(queried, prop->bp_prop_num);
        }
    }
}

static void
battery_mgr_poll_battery(struct battery *battery)
{
    uint32_t changed[BATTERY_PROPERTY_MASK_SIZE] = {0};
    uint32_t queried[BATTERY_PROPERTY_MASK_SIZE] = {0};
    uint32_t masked;
    int first_one;
    struct listener_data *ld;
    struct battery_driver *driver;
    int i;
    int j;

    /* Poll battery drivers */
    for(i = 0; i < BATTERY_DRIVERS_MAX; ++i) {
        driver = battery->b_drivers[i];
        if (driver) {
            battery_mgr_poll_battery_driver(battery, driver, changed, queried);
        }
    }

    /* Notify listeners about property changes */
    for (i = 0; i < battery->b_listener_count; ++i) {
        ld = &battery->b_listeners[i];
        for (j = 0; j < BATTERY_PROPERTY_MASK_SIZE; ++j) {
            masked = changed[j] & ld->ld_prop_change_mask[j];
            while (masked) {
                /* Find first set bit */
                first_one = __builtin_ffs(masked) - 1;
                /* Clear it for next loop */
                masked ^= (1 << first_one);
                driver = battery->b_drivers[i];
                /* Notify listener */
                ld->ld_listener->bpl_prop_changed(ld->ld_listener,
                                                  battery->b_properties +
                                                  first_one + j * 32);
            }
        }
    }

    /* Notify listeners about periodic reads */
    for (i = 0; i < battery->b_listener_count; ++i) {
        ld = &battery->b_listeners[i];
        for (j = 0; j < BATTERY_PROPERTY_MASK_SIZE; ++j) {
            masked = queried[j] & ld->ld_prop_read_mask[j];
            while (masked) {
                /* Find first set bit */
                first_one = __builtin_ffs(masked) - 1;
                /* Clear it for next loop */
                masked ^= (1 << first_one);
                driver = battery->b_drivers[i];
                /* Notify listener */
                ld->ld_listener->bpl_prop_read(ld->ld_listener,
                                          battery->b_properties +
                                          first_one + j * 32);
            }
        }
    }
}

int
battery_set_poll_rate_ms(struct os_dev *battery, uint32_t poll_rate)
{
    return battery_set_poll_rate_ms_delay(battery, poll_rate, 0);
}

int
battery_set_poll_rate_ms_delay(struct os_dev *battery, uint32_t poll_rate,
    uint32_t start_delay)
{
    struct battery *bat = (struct battery *)battery;

    if (bat == NULL) {
        return -1;
    }

    if (poll_rate == 0) {
        bat->b_poll_rate = 0;
        os_callout_stop(&battery_manager.bm_poll_callout);
        return 0;
    }

    bat->b_poll_rate = poll_rate;
    bat->b_next_run = os_time_get();
    os_callout_reset(&battery_manager.bm_poll_callout,
                     os_time_ms_to_ticks32(start_delay));

    return 0;
}

int
battery_add_driver(struct os_dev *battery, struct battery_driver *driver)
{
    struct battery *bat = (struct battery *)battery;
    const struct battery_driver_property *drv_prop;
    struct battery_property *prop;
    int bat_num;
    int drv_num;
    int i;
    int j;

    assert(battery);
    assert(driver);

    bat_num = battery_get_num(bat);
    /* Find first slot */
    for (i = 0; i < BATTERY_DRIVERS_MAX; ++i) {
        /* Just make sure that driver is not added twice */
        assert(bat->b_drivers[i] != driver);
        if (bat->b_drivers[i] == NULL) {
            break;
        }
    }
    assert(i < BATTERY_DRIVERS_MAX);
    drv_num = i;
    bat->b_drivers[i] = driver;

    drv_prop = driver->bd_driver_properties;
    for (i = 0; drv_prop->bdp_type; ++i) {
        ++drv_prop;
    }
    driver->bd_property_count = i;
    bat->b_properties = realloc(bat->b_properties,
                                ((i + bat->b_all_property_count) *
                                 sizeof(struct battery_property)));

    j = bat->b_all_property_count;
    driver->bd_first_property = (uint8_t)j;
    /* Initialize driver properties with battery manager data */
    for (i = 0; i < driver->bd_property_count; ++i, ++j) {
        prop = bat->b_properties + j;
        prop->bp_type = driver->bd_driver_properties[i].bdp_type;
        prop->bp_flags = driver->bd_driver_properties[i].bdp_flags;
        prop->bp_valid = 0;
        prop->bp_value.bpv_i32 = 0;
        prop->bp_drv_prop_num = (uint8_t)i;
        prop->bp_drv_num = drv_num;
        prop->bp_bat_num = bat_num;
        prop->bp_prop_num = j;
    }
    bat->b_all_property_count = j;

    return 0;
}

static int
get_listener_index(struct battery *battery,
        struct battery_prop_listener *listener)
{
    int i;

    assert(listener);
    assert(battery);

    /* Find existing listener */
    for (i = 0; i < battery->b_listener_count; ++i) {
        if (battery->b_listeners[i].ld_listener == listener) {
            break;
        }
    }
    /* If listener was not yet added, create space for it and fill
     * masks with zeros so they will be filled when listener is actually
     * added to properties.
     */
    if (i >= battery->b_listener_count) {
        i = battery->b_listener_count++;
        battery->b_listeners = realloc(battery->b_listeners,
                sizeof(struct listener_data) * battery->b_listener_count);
        if (battery->b_listeners == NULL) {
            return -1;
        }
        battery->b_listeners[i].ld_listener = listener;
        memset(battery->b_listeners[i].ld_prop_change_mask, 0,
               sizeof(battery->b_listeners[i].ld_prop_change_mask));
        memset(battery->b_listeners[i].ld_prop_read_mask, 0,
               sizeof(battery->b_listeners[i].ld_prop_read_mask));
    }

    return i;
}

static int
battery_update_polled_properties(struct battery *battery)
{
    struct listener_data *ld;
    int i;
    int j;

    for (i = 0; i < BATTERY_PROPERTY_MASK_SIZE; ++i) {
        battery->b_polled_properties[i] = 0;
        for (j = 0; j < battery->b_listener_count; ++j) {
            ld = &battery->b_listeners[j];
            battery->b_polled_properties[i] |= ld->ld_prop_read_mask[i];
            battery->b_polled_properties[i] |= ld->ld_prop_change_mask[i];
        }
    }

    return 0;
}

int
battery_prop_change_subscribe(struct battery_prop_listener *listener,
        struct battery_property *prop)
{
    struct battery *battery;
    int listener_index;

    assert(listener);
    assert(prop);

    battery = (struct battery *)battery_get_battery(prop->bp_bat_num);
    listener_index = get_listener_index(battery, listener);

    set_bit(battery->b_listeners[listener_index].ld_prop_change_mask,
            prop->bp_prop_num);

    battery_update_polled_properties(battery);

    return 0;
}

int
battery_prop_change_unsubscribe(struct battery_prop_listener *listener,
        struct battery_property *prop)
{
    struct battery *battery;
    int i;
    int j;

    if (prop) {
        /* Single propety supplied */
        battery = (struct battery *)battery_get_battery(prop->bp_bat_num);
        for (i = 0; i < battery->b_listener_count; ++i) {
            if (listener == battery->b_listeners[i].ld_listener) {
                break;
            }
        }
        if (i >= battery->b_listener_count) {
            return -1;
        }

        clear_bit(battery->b_listeners[i].ld_prop_change_mask,
                  prop->bp_prop_num);
    } else {
        /* No property supplied, remove listener from all subscriptions */
        for (i = 0; i < BATTERY_MAX_COUNT; ++i) {
            battery =  battery_manager.bm_batteries[i];
            for (j = 0; j < battery->b_listener_count; ++i) {
                if (battery->b_listeners[j].ld_listener == listener) {
                    /* Reduce number of listeners */
                    battery->b_listener_count--;
                    if (j < battery->b_listener_count) {
                        /* It was not last listener,
                         * move last to emptied space */
                        battery->b_listeners[j] =
                                battery->b_listeners[battery->b_listener_count];
                    }
                    battery->b_listeners =
                        realloc(battery->b_listeners,
                                sizeof(struct listener_data) *
                                battery->b_listener_count);
                    break;
                }
            }
        }
    }

    battery_update_polled_properties(battery);

    return 0;
}

int
battery_prop_poll_subscribe(struct battery_prop_listener *listener,
        struct battery_property *prop)
{
    struct battery *battery;
    int listener_index;

    assert(listener);
    assert(prop);

    battery = (struct battery *)battery_get_battery(prop->bp_bat_num);
    listener_index = get_listener_index(battery, listener);

    set_bit(battery->b_listeners[listener_index].ld_prop_read_mask,
            prop->bp_prop_num);

    battery_update_polled_properties(battery);

    return 0;
}

int
battery_prop_poll_unsubscribe(struct battery_prop_listener *listener,
        struct battery_property *prop)
{
    struct battery *battery;
    int i;
    int j;

    if (prop) {
        battery = (struct battery *)battery_get_battery(prop->bp_bat_num);
        for (i = 0; i < battery->b_listener_count; ++i) {
            if (listener == battery->b_listeners[i].ld_listener) {
                break;
            }
        }
        if (i >= battery->b_listener_count) {
            return -1;
        }

        clear_bit(battery->b_listeners[i].ld_prop_read_mask,
                  prop->bp_prop_num);
    } else {
        for (i = 0; i < BATTERY_MAX_COUNT; ++i) {
            battery =  battery_manager.bm_batteries[i];
            for (j = 0; j < battery->b_listener_count; ++i) {
                if (battery->b_listeners[j].ld_listener == listener) {
                    battery->b_listener_count--;
                    if (j < battery->b_listener_count) {
                        battery->b_listeners[j] =
                                battery->b_listeners[battery->b_listener_count];
                    }
                    battery->b_listeners =
                        realloc(battery->b_listeners,
                                sizeof(struct listener_data) *
                                battery->b_listener_count);
                    break;
                }
            }
        }
    }

    battery_update_polled_properties(battery);

    return 0;
}

static int
battery_open(struct os_dev *dev, uint32_t timeout, void *arg)
{
    struct battery *bat = (struct battery *)dev;
    struct os_dev *drv_dev;
    int rollback = -1;
    int i;
    int rc = 0;

    for (i = 0; i < BATTERY_DRIVERS_MAX && bat->b_drivers[i]; ++i) {
        drv_dev = os_dev_open(bat->b_drivers[i]->dev.od_name, 0, NULL);
        assert(drv_dev == &bat->b_drivers[i]->dev);
        if (drv_dev != &bat->b_drivers[i]->dev) {
            rollback = i - 1;
            rc = -1;
            break;
        }
    }
    for (i = rollback; i >= 0; --i) {
        os_dev_close(&bat->b_drivers[i]->dev);
    }
    return rc;
}

static int
battery_close(struct os_dev *dev)
{
    struct battery *bat = (struct battery *)dev;
    int i;

    for (i = 0; i < BATTERY_DRIVERS_MAX && bat->b_drivers[i]; ++i) {
        os_dev_close(&bat->b_drivers[i]->dev);
    }
    return 0;
}

int
battery_init(struct os_dev *dev, void *arg)
{
    int i;
    struct battery *bat = (struct battery *)dev;

    OS_DEV_SETHANDLERS(dev, battery_open, battery_close);

    os_mutex_pend(&battery_manager.bm_lock, OS_WAIT_FOREVER);
    for (i = 0; i < BATTERY_MAX_COUNT; ++i) {
        if (battery_manager.bm_batteries[i] == NULL) {
            break;
        }
    }
    os_mutex_release(&battery_manager.bm_lock);

    assert(i < BATTERY_MAX_COUNT);
    battery_manager.bm_batteries[i] = bat;

    memset(bat->b_drivers, 0, sizeof(bat->b_drivers));

    return 0;
}
