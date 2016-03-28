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


/* These are defined for testing in native */

/* all ADCS below have a reference voltage of 5 V */

/* This ADC had two channels (0-1) that return 8-bit 
 * random numbers */
extern struct hal_adc_s *pnative_random_adc;

/* This ADC has three 12-bit channels (0-2) that return, min,
 * mid, max, respectively  */
extern struct hal_adc_s *pnative_min_mid_max_adc;

/* this ADC has one channel (0) that reads bytes from 
 * a file and returns 8-bit ADC values .  When the file
 * ends, the reads will return -1  */
extern struct hal_adc_s *pnative_file_adc;

#endif /* H_NATIVE_BSP_ */
