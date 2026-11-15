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
#include <mcu/pic32_isr.h>

#include <device/usbd.h>

/* USB interrupt, priority (USB_IPL) is set by pic32_isr_init() */
static void
USBD_IRQHandler(void)
{
    IFS4CLR = _IFS4_USBIF_MASK;
    tud_int_handler(0);
}
PIC32_ISR_NO_FPU(_USB_VECTOR, MYNEWT_VAL(USB_IPL), USBD_IRQHandler);

void
tinyusb_hardware_init(void)
{
    USBCRCONbits.USBIE = 0;
    IFS4CLR = _IFS4_USBIF_MASK;
    IEC4SET = _IEC4_USBIE_MASK;

    /* Force device mode by overriding USB ID and settings it to 1 */
    USBCRCONbits.PHYIDEN = 0;
    USBCRCONbits.USBIDVAL = 1;
    USBCRCONbits.USBIDOVEN = 1;
#if !MYNEWT_VAL(OS_SCHEDULING)
    /*
     * OS is not started (e.g. DFU in bootloader) so interrupts were not
     * enabled by os_start(). Without them USB interrupt is never serviced.
     */
    __builtin_enable_interrupts();
#endif
}
