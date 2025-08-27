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

#ifndef H_USB_KEEPALIVE_DETECT_
#define H_USB_KEEPALIVE_DETECT_

#include <stdint.h>
#include <stdbool.h>
#include <os/os.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * USB keep-alive detection callback type
 *
 * @param connected true when USB is connected, false when disconnected
 */
typedef void (*usb_keepalive_cb_t)(bool connected);

/**
 * USB keep-alive detection callbacks
 * Platform-specific implementations must provide these functions
 */
struct usb_keepalive_cbs {
    void (*handle_sof_interrupt)(void);
    void (*check_suspend)(void);
    void (*enable_interrupts)(void);
    uint16_t (*get_frame_number)(void);
};

/**
 * Initialize USB keep-alive detection
 *
 * @return 0 on success, error code on failure
 */
int usb_keepalive_init(void);

/**
 * Set event queue for keep-alive monitoring
 *
 * @param evq Event queue to use (NULL for default)
 */
void usb_keepalive_evq_set(struct os_eventq *evq);

/**
 * Register platform-specific callbacks
 *
 * @param cbs Platform-specific callback implementation
 */
void usb_keepalive_register_cbs(const struct usb_keepalive_cbs *cbs);

/**
 * Register callback for USB connection status changes
 *
 * @param cb Callback function
 */
void usb_keepalive_register_cb(usb_keepalive_cb_t cb);

/**
 * Check if USB is active (receiving keep-alives)
 *
 * @return true if USB is active, false otherwise
 */
bool usb_keepalive_is_active(void);

/**
 * Get number of SOF packets received
 *
 * @return SOF cnt since last disconnect
 */
uint32_t usb_keepalive_get_sof_cnt(void);

/**
 * Handle SOF interrupt (called by platform code)
 */
void usb_keepalive_handle_sof(void);

/**
 * Handle suspend event (called by platform code)
 */
void usb_keepalive_handle_suspend(void);

/**
 * Handle resume event (called by platform code)
 */
void usb_keepalive_handle_resume(void);

#ifdef __cplusplus
}
#endif

#endif /* H_USB_KEEPALIVE_DETECT_ */
