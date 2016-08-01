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


#include <adc/adc.h>

/* Nordic headers */
#include <nrf.h>
#include <nrf_drv_saadc.h>
#include <app_error.h>


static int
nrf52_adc_configure(struct adc_dev *dev, void *cfgdata)
{

    return (0);
}

static int
nrf52_adc_sample(struct adc_dev *dev)
{
    nrf_drv_saadc_sample();

    return (0);
}

/**
 * Callback to initialize an adc_dev structure from the os device
 * initialization callback.  This sets up a nrf52_adc_device(), so
 * that subsequent lookups to this device allow us to manipulate it.
 */
int
nrf52_adc_dev_init(struct os_dev *odev)
{
    struct adc_dev *dev;
    struct adc_driver_funcs *af;

    dev = (struct adc_dev *) odev;

    af = &dev->ad_funcs;

    af->af_config = nrf52_adc_configure;
    af->af_sample = nrf52_adc_sample;

    return (0);
}


