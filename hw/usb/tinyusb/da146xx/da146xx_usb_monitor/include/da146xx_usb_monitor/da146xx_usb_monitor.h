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

#ifndef H_DA146XX_USB_MONITOR_
#define H_DA146XX_USB_MONITOR_

#include <stdint.h>
#include <stdbool.h>
#include <os/os.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * USB monitor state detection callback type
 *
 * @param connected true when USB is connected, false when disconnected
 */
typedef void (*da146xx_usb_monitor_cb_t)(bool connected);

/**
 * Initialize USB monitor state
 *
 * @return 0 on success, error code on failure
 */
int da146xx_usb_monitor_init(void);

/**
 * Register callback for USB connection status changes
 *
 * @param cb Callback function
 */
void da146xx_usb_monitor_register_cb(da146xx_usb_monitor_cb_t cb);

/**
 * Check if USB is connected (received setup packet)
 *
 * @return true if USB is connected, false otherwise
 */
bool da146xx_usb_monitor_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif /* H_DA146XX_USB_MONITOR_ */
