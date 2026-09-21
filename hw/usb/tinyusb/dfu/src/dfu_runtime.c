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

#include <os/mynewt.h>
#include <bsp/bsp.h>

#include <class/dfu/dfu_device.h>
#include <tinyusb/tinyusb.h>
#include <hal/hal_nvreg.h>

/*
 * DFU callbacks
 * Note: alt is used as the partition number, in order to support multiple
 * partitions like FLASH, EEPROM, etc.
 */

static struct os_callout delayed_reboot_callout;

static void
delayed_reboot_cb(struct os_event *event)
{
    /* Write magic value to NVReg so bootloader will start in USB DFU mode */
    hal_nvreg_write(MYNEWT_VAL_USBD_DFU_MAGIC_NVREG, MYNEWT_VAL_USBD_DFU_MAGIC_VALUE);

    hal_system_reset();
}

/* Invoked when a DFU_DETACH request is received and bitWillDetach is set */
void
tud_dfu_runtime_reboot_to_dfu_cb(void)
{
    _Static_assert(MYNEWT_VAL_USBD_DFU_MAGIC_NVREG >= 0, "No NVReg specified");

    os_callout_init(&delayed_reboot_callout, os_eventq_dflt_get(),
                    delayed_reboot_cb, NULL);
    os_callout_reset(&delayed_reboot_callout, os_time_ms_to_ticks32(10));
}
