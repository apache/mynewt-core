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

#ifndef __ADC_NRF51_H__
#define __ADC_NRF51_H__

#include <adc/adc.h>

#include <nrfx.h>
#include <nrf_adc.h>

#ifdef __cplusplus
extern "C" {
#endif

struct nrf51_adc_dev_cfg {
    uint16_t nadc_refmv0;	/* reference mV in AREF0 */
    uint16_t nadc_refmv1;	/* reference mV in AREF1 */
    uint16_t nadc_refmv_vdd;	/* reference mV in VDD */
};

int nrf51_adc_dev_init(struct os_dev *, void *);

#ifdef __cplusplus
}
#endif

#endif /* __ADC_NRF51_H__ */
