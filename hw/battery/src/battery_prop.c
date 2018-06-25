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
#include <battery/battery_prop.h>
#include <battery/battery_drv.h>
#include <battery/battery.h>

int battery_prop_get_value(struct battery_property *prop)
{
    struct battery_driver *drv;
    struct battery *bat = (struct battery *)battery_get_battery(prop->bp_bat_num);
    int rc = -1;

    if (driver_property(prop)) {
        drv = bat->b_drivers[prop->bp_drv_num];
        rc = drv->bd_funcs->bdf_property_get(drv, prop, OS_WAIT_FOREVER);
    }
    return rc;
}

int
battery_prop_get_value_float(struct battery_property *prop, float *value) {
    int rc = battery_prop_get_value(prop);
    if (rc == 0 && prop->bp_valid) {
        *value = prop->bp_value.bpv_flt;
    }
    return rc;
}

int
battery_prop_get_value_uint8(struct battery_property *prop, uint8_t *value)
{
    int rc = battery_prop_get_value(prop);
    if (rc == 0 && prop->bp_valid) {
        *value = prop->bp_value.bpv_u8;
    }
    return rc;
}

int
battery_prop_get_value_int8(struct battery_property *prop, int8_t *value)
{
    int rc = battery_prop_get_value(prop);
    if (rc == 0 && prop->bp_valid) {
        *value = prop->bp_value.bpv_i8;
    }
    return rc;
}

int
battery_prop_get_value_uint16(struct battery_property *prop, uint16_t *value)
{
    int rc = battery_prop_get_value(prop);
    if (rc == 0 && prop->bp_valid) {
        *value = prop->bp_value.bpv_u16;
    }
    return rc;
}

int
battery_prop_get_value_int16(struct battery_property *prop, int16_t *value)
{
    int rc = battery_prop_get_value(prop);
    if (rc == 0 && prop->bp_valid) {
        *value = prop->bp_value.bpv_i16;
    }
    return rc;
}

int
battery_prop_get_value_uint32(struct battery_property *prop, uint32_t *value)
{
    int rc = battery_prop_get_value(prop);
    if (rc == 0 && prop->bp_valid) {
        *value = prop->bp_value.bpv_u32;
    }
    return rc;
}

int
battery_prop_get_value_int32(struct battery_property *prop, int32_t *value)
{
    int rc = battery_prop_get_value(prop);
    if (rc == 0 && prop->bp_valid) {
        *value = prop->bp_value.bpv_i32;
    }
    return rc;
}

static struct battery_driver *
get_property_driver(struct battery_property *prop)
{
    struct battery *bat;
    bat = (struct battery *)battery_get_battery(prop->bp_bat_num);
    return bat->b_drivers[prop->bp_drv_num];
}

int
battery_prop_set_value(struct battery_property *prop,
        const battery_property_value_t *value) {
    struct battery_driver *drv = get_property_driver(prop);
    int rc = 0;
    /* Driver provided property */
    if (drv) {
        prop->bp_value = *value;
        rc = drv->bd_funcs->bdf_property_set(drv, prop);
    } else {
        prop->bp_value = *value;
        prop->bp_valid = 1;
    }
    if (prop->bp_base) {
        // TODO: Search for complex properties
    }
    return rc;
}

int
battery_prop_set_value_float(struct battery_property *prop, float value) {
    battery_property_value_t v;
    v.bpv_flt = value;
    return battery_prop_set_value(prop, &v);
}

int
battery_prop_set_value_uint8(struct battery_property *prop, uint8_t value)
{
    battery_property_value_t v;
    v.bpv_u8 = value;
    return battery_prop_set_value(prop, &v);
}

int
battery_prop_set_value_int8(struct battery_property *prop, int8_t value)
{
    battery_property_value_t v;
    v.bpv_i8 = value;
    return battery_prop_set_value(prop, &v);
}

int
battery_prop_set_value_uint16(struct battery_property *prop, uint16_t value)
{
    battery_property_value_t v;
    v.bpv_u16 = value;
    return battery_prop_set_value(prop, &v);
}

int
battery_prop_set_value_int16(struct battery_property *prop, int16_t value)
{
    battery_property_value_t v;
    v.bpv_i16 = value;
    return battery_prop_set_value(prop, &v);
}

int
battery_prop_set_value_uint32(struct battery_property *prop, uint32_t value)
{
    battery_property_value_t v;
    v.bpv_u32 = value;
    return battery_prop_set_value(prop, &v);
}

int
battery_prop_set_value_int32(struct battery_property *prop, int32_t value)
{
    battery_property_value_t v;
    v.bpv_i32 = value;
    return battery_prop_set_value(prop, &v);
}
