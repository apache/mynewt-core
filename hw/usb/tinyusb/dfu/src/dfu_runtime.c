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

/* Invoked when a DFU_DETACH request is received and bitWillDetach is set */
void
tud_dfu_runtime_reboot_to_dfu_cb(void)
{
    _Static_assert(MYNEWT_VAL_USBD_DFU_MAGIC_NVREG >= 0, "No NVReg specified");
    /* Write magic value to NVReg so bootloader will start in USB DFU mode */
    hal_nvreg_write(MYNEWT_VAL_USBD_DFU_MAGIC_NVREG, MYNEWT_VAL_USBD_DFU_MAGIC_VALUE);
    hal_system_reset();
}
