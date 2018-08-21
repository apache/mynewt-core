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

#ifndef __SYS_ID_H__
#define __SYS_ID_H__

#ifdef __cplusplus
extern "C" {
#endif

#if MYNEWT_VAL(ID_SERIAL_PRESENT)
/*
 * Maximum expected serial number string length.
 */
#define ID_SERIAL_MAX_LEN       MYNEWT_VAL(ID_SERIAL_MAX_LEN)
extern char id_serial[];
#endif

#if MYNEWT_VAL(ID_MANUFACTURER_LOCAL)
/*
 * Maximum expected manufacturer string length.
 */
#define ID_MANUFACTURER_MAX_LEN 64
extern char id_manufacturer[];
#else
extern const char id_manufacturer[];
#endif

#if MYNEWT_VAL(ID_MODEL_LOCAL)
/*
 * Maximum expected model name string length.
 */
#define ID_MODEL_MAX_LEN        64
extern char id_model[];
#elif MYNEWT_VAL(ID_MODEL_PRESENT)
extern const char id_model[];
#endif

extern char id_mfghash[];
extern const char *id_bsp_str;
extern const char *id_app_str;

/*
 * Initialize manufacturing info storage/reporting.
 */
void id_init(void);

#ifdef __cplusplus
}
#endif

#endif
