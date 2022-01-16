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
#include <xc.h>
#include <mcu/mcu.h>
#include <mcu/pic32.h>

#include <device/usbd.h>

void
__attribute__((interrupt(IPL2AUTO), vector(_USB_VECTOR), no_fpu))
USBD_IRQHandler(void)
{
    IFS4CLR = _IFS4_USBIF_MASK;
    tud_int_handler(0);
}

void
tinyusb_hardware_init(void)
{
    /* set interrupt priority */
    IPC33CLR = _IPC33_USBIP_MASK;
    IPC33SET = (2 << _IPC33_USBIP_POSITION); /* priority 2 */
    /* set interrupt subpriority */
    IPC33CLR = _IPC33_USBIS_MASK;
    IPC33SET = (0 << _IPC33_USBIS_POSITION); /* subpriority 0 */

    USBCRCONbits.USBIE = 0;
    IFS4CLR = _IFS4_USBIF_MASK;
    IEC4SET = _IEC4_USBIE_MASK;

    /* Force device mode by overriding USB ID and settings it to 1 */
    USBCRCONbits.PHYIDEN = 0;
    USBCRCONbits.USBIDVAL = 1;
    USBCRCONbits.USBIDOVEN = 1;
}
