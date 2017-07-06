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


#include <pwm/pwm.h>
#include <errno.h>
#include <assert.h>

/**
 * Configure a channel on the ADC device.
 *
 * @param dev The device to configure
 * @param cnum The channel number to configure
 * @param data Driver specific configuration data for this channel.
 *
 * @return 0 on success, non-zero error code on failure.
 */
int
pwm_chan_config(struct pwm_dev *dev, uint8_t cnum, void *data)
{
    assert(dev->pwm_funcs.pwm_configure_channel != NULL);

    if (cnum > dev->pwm_chan_count) {
        return (EINVAL);
    }

    return (dev->pwm_funcs.pwm_configure_channel(dev, cnum, data));
}
