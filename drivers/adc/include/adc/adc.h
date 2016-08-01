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

#ifndef __ADC_H__
#define __ADC_H__

#include <os/os_dev.h>

struct adc_dev;

typedef int (*adc_config_func_t)(struct adc_dev *dev, void *);
typedef int (*adc_sample_func_t)(struct adc_dev *);

struct adc_driver_funcs {
    adc_config_func_t af_config;
    adc_sample_func_t af_sample;
};

struct adc_dev {
    struct os_dev ad_dev;
    struct adc_driver_funcs ad_funcs;
};

#define adc_sample(__dev) ((__dev)->ad_funcs.af_sample((__dev)))
#define adc_configure(__dev, __b) ((__dev)->ad_funcs.af_configure((__dev), (__b)))

#endif /* __ADC_H__ */
