/**
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
#include <hal/hal_dac.h>
#include <hal/hal_dac_int.h>

struct hal_dac *
hal_dac_init(enum system_device_id pin) 
{
    return bsp_get_hal_dac(pin);
}

int 
hal_dac_write(struct hal_dac *pdac, int val) 
{
    if (pdac && pdac->driver_api && pdac->driver_api->hdac_write) {
        return pdac->driver_api->hdac_write(pdac, val);
    }
    return -1;
}

int 
hal_dac_get_bits(struct hal_dac *pdac) 
{
    if (pdac && pdac->driver_api && pdac->driver_api->hdac_get_bits) {
        return pdac->driver_api->hdac_get_bits(pdac);
    }
    return -1;
}

int 
hal_dac_get_ref_mv(struct hal_dac *pdac) 
{
    if (pdac && pdac->driver_api && pdac->driver_api->hdac_get_ref_mv) {
        return pdac->driver_api->hdac_get_ref_mv(pdac);
    }
    return -1;
}

/* gets the current DAC value */
int 
hal_dac_get_current(struct hal_dac *pdac) 
{
    if (pdac && pdac->driver_api && pdac->driver_api->hdac_get_ref_mv) {
        return pdac->driver_api->hdac_current(pdac);
    }
    return -1;
}

/* Converts a millivolt value to the right DAC settings for this DAC.
 */
int 
hal_dac_to_val(struct hal_dac *pdac, int mvolts) 
{
    int rc = -1;
    int bits = hal_dac_get_bits(pdac);
    int ref = hal_dac_get_ref_mv(pdac);
            
    if ((bits > 0) && (ref >= 0)) 
    {
        /* assume that 2^N -1 is full scale. This line multiples by 2^N-1 
         * and then rounds up */
        rc = (mvolts << bits) - mvolts + (ref >> 1);
        rc /= ref;
        
        /* don't let this exceed the DAC max */
        if(rc >= (1 << bits)) {
            rc = (1 << bits) - 1;
        }
    }
    return rc;
}

int 
hal_dac_disable(struct hal_dac *pdac) 
{
    if (pdac && pdac->driver_api && pdac->driver_api->hdac_disable) {
        return pdac->driver_api->hdac_disable(pdac);
    }
    return -1;
}
