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
#ifndef H_NATIVE_BSP_
#define H_NATIVE_BSP_

extern const struct hal_flash native_flash_dev;

#define MAX_RANDOM_ADC_DEVICES (2)
#define MAX_MMM_ADC_DEVICES    (3)
#define MAX_FILE_ADC_DEVICES   (1)

/* This ADC had two channels (0-1) that return 8-bit 
 * random numbers */
extern const struct hal_adc_funcs_s random_adc_funcs;


/* This ADC has three 12-bit channels (0-2) that return, min,
 * mid, max, respectively . */
extern const struct hal_adc_funcs_s mmm_adc_funcs;


/* this ADC has one channel (0) that reads bytes from 
 * a file and returns 8-bit ADC values .  When the file
 * ends, the reads will return -1  */
extern const struct hal_adc_funcs_s file_adc_funcs;

#endif /* H_NATIVE_BSP_ */
