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

#ifndef HAL_DAC_H
#define HAL_DAC_H 

#ifdef __cplusplus
extern "C" {
#endif

enum samd_dac_reference {
    SAMD_DAC_REFERENCE_INT1V = 0,
    SAMD_DAC_REFERENCE_AVCC = 1,
    SAMD_DAC_REFERENCE_AREF = 2,
};

struct samd21_dac_config {
    enum samd_dac_reference reference;
    int dac_reference_voltage_mvolts;
};

/* This creates a new DAC object for this DAC source */
struct hal_dac *samd21_dac_create(const struct samd21_dac_config *pconfig);
    
#ifdef __cplusplus
}
#endif

#endif /* HAL_DAC_H */

