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

#include <device/usbd.h>
#include <nrfx/hal/nrf_usbreg.h>

/* same value as NRFX_POWER_USB_EVT_* in nrfx_power */
enum {
    USB_EVT_DETECTED = 0,
    USB_EVT_REMOVED = 1,
    USB_EVT_READY = 2
};

/**
 * tinyusb function that handles power event (0: detected, 1: remove, 2: ready)
 * We must call it within SD's SOC event handler, or set it as power event handler if SD is not enabled.
 */
extern void tusb_hal_nrf_power_event(uint32_t event);

static void
USBD_IRQHandler(void)
{
    tud_int_handler(0);
}

static bool
nrf_usbreg_event_get_and_clear(NRF_USBREG_Type *p_reg,
                               nrf_usbreg_event_t event)
{
    bool ret = nrf_usbreg_event_check(p_reg, event);
    if (ret) {
        nrf_usbreg_event_clear(p_reg, event);
    }
    return ret;
}


/* Power ISR to detect USB VBUS state */
static void
USBREG_IRQHandler(void)
{
    if (nrf_usbreg_int_enable_check(NRF_USBREGULATOR, NRF_USBREG_INT_USBDETECTED) &&
        (nrf_usbreg_event_get_and_clear(NRF_USBREGULATOR, NRF_USBREG_EVENT_USBDETECTED))) {
        tusb_hal_nrf_power_event(USB_EVT_DETECTED);
    }

    if (nrf_usbreg_int_enable_check(NRF_USBREGULATOR, NRF_USBREG_INT_USBREMOVED) &&
        nrf_usbreg_event_get_and_clear(NRF_USBREGULATOR,NRF_USBREG_EVENT_USBREMOVED)) {
        tusb_hal_nrf_power_event(USB_EVT_REMOVED);
    }

    if (nrf_usbreg_int_enable_check(NRF_USBREGULATOR, NRF_USBREG_INT_USBPWRRDY) &&
        nrf_usbreg_event_get_and_clear(NRF_USBREGULATOR, NRF_USBREG_EVENT_USBPWRRDY)) {
        tusb_hal_nrf_power_event(USB_EVT_READY);
    }
}

void
tinyusb_hardware_init(void)
{
    /* Setup USB IRQ */
    NVIC_SetVector(USBD_IRQn, (uint32_t)USBD_IRQHandler);
    NVIC_SetPriority(USBD_IRQn, 2);

    /* Setup Power IRQ to detect USB VBUS state (detected, ready, removed) */
    NVIC_SetVector(USBREGULATOR_IRQn, (uint32_t)USBREG_IRQHandler);
    NVIC_SetPriority(USBREGULATOR_IRQn, 7);
    nrf_usbreg_int_enable(NRF_USBREGULATOR,
                          USBREG_INTEN_USBDETECTED_Msk |
                          USBREG_INTEN_USBREMOVED_Msk |
                          USBREG_INTEN_USBPWRRDY_Msk);

    NVIC_EnableIRQ(USBREGULATOR_IRQn);

    /*
     * USB power may already be ready at this time -> no event generated
     * We need to invoke the handler based on the status initially
     */
    uint32_t usb_reg = NRF_USBREGULATOR->USBREGSTATUS;

    if (usb_reg & USBREG_USBREGSTATUS_VBUSDETECT_Msk) {
        tusb_hal_nrf_power_event(USB_EVT_DETECTED);
    }

    if (usb_reg & USBREG_USBREGSTATUS_OUTPUTRDY_Msk) {
        tusb_hal_nrf_power_event(USB_EVT_READY);
    }
}
