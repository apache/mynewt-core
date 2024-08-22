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
#include <mcu/mcu.h>

#include <tusb.h>

#include <clock_config.h>
#include <fsl_clock.h>
#include <fsl_iocon.h>
#include <fsl_power.h>

void USB0_IRQHandler(void) {
    tud_int_handler(0);
}

void USB1_IRQHandler(void) {
    tud_int_handler(1);
}

void
tinyusb_hardware_init(void)
{
    CLOCK_EnableClock(kCLOCK_Iocon);

    NVIC_SetVector(USB0_IRQn, (uint32_t)USB0_IRQHandler);
    NVIC_SetPriority(USB0_IRQn, 2);
    NVIC_SetVector(USB1_IRQn, (uint32_t)USB1_IRQHandler);
    NVIC_SetPriority(USB1_IRQn, 2);

    /* PORT0 PIN22 configured as USB0_VBUS */
    IOCON_PinMuxSet(IOCON, 0U, 22U, IOCON_FUNC7 | IOCON_DIGITAL_EN);

#if MYNEWT_VAL_CHOICE(USBD_RHPORT, USB0)
    /* Port0 is Full Speed */

    /* Turn on USB0 Phy */
    POWER_DisablePD(kPDRUNCFG_PD_USB0_PHY);

    /* reset the IP to make sure it's in reset state. */
    RESET_PeripheralReset(kUSB0D_RST_SHIFT_RSTn);
    RESET_PeripheralReset(kUSB0HSL_RST_SHIFT_RSTn);
    RESET_PeripheralReset(kUSB0HMR_RST_SHIFT_RSTn);

    /* Enable USB Clock Adjustments to trim the FRO for the full speed controller */
    ANACTRL->FRO192M_CTRL |= ANACTRL_FRO192M_CTRL_USBCLKADJ_MASK;
    CLOCK_SetClkDiv(kCLOCK_DivUsb0Clk, 1, false);
    CLOCK_AttachClk(kFRO_HF_to_USB0_CLK);

    /* According to reference manual, device mode setting has to be set by access usb host register */
    CLOCK_EnableClock(kCLOCK_Usbhsl0);  /* enable usb0 host clock */
    USBFSH->PORTMODE |= USBFSH_PORTMODE_DEV_ENABLE_MASK;
    CLOCK_DisableClock(kCLOCK_Usbhsl0); /* disable usb0 host clock */

    /* enable USB Device clock */
    CLOCK_EnableUsbfs0DeviceClock(kCLOCK_UsbfsSrcFro, CLOCK_GetFreq(kCLOCK_FroHf));
#endif

#if MYNEWT_VAL_CHOICE(USBD_RHPORT, USB1)
    /* Port1 is High Speed */

    /* Turn on USB1 Phy */
    POWER_DisablePD(kPDRUNCFG_PD_USB1_PHY);

    /* reset the IP to make sure it's in reset state. */
    RESET_PeripheralReset(kUSB1H_RST_SHIFT_RSTn);
    RESET_PeripheralReset(kUSB1D_RST_SHIFT_RSTn);
    RESET_PeripheralReset(kUSB1_RST_SHIFT_RSTn);
    RESET_PeripheralReset(kUSB1RAM_RST_SHIFT_RSTn);

    /* According to reference manual, device mode setting has to be set by access usb host register */
    CLOCK_EnableClock(kCLOCK_Usbh1); // enable usb0 host clock

    /* Put PHY power down under software control */
    USBHSH->PORTMODE = USBHSH_PORTMODE_SW_PDCOM_MASK;
    USBHSH->PORTMODE |= USBHSH_PORTMODE_DEV_ENABLE_MASK;

    /* disable usb0 host clock */
    CLOCK_DisableClock(kCLOCK_Usbh1);

    /* enable USB Device clock */
    CLOCK_EnableUsbhs0PhyPllClock(kCLOCK_UsbPhySrcExt, BOARD_XTAL0_CLK_HZ);
    CLOCK_EnableUsbhs0DeviceClock(kCLOCK_UsbSrcUnused, 0U);
    CLOCK_EnableClock(kCLOCK_UsbRam1);

    /* Enable PHY support for Low speed device + LS via FS Hub */
    USBPHY->CTRL |= USBPHY_CTRL_SET_ENUTMILEVEL2_MASK | USBPHY_CTRL_SET_ENUTMILEVEL3_MASK;

    /* Enable all power for normal operation */
    USBPHY->PWD = 0;

    USBPHY->CTRL_SET = USBPHY_CTRL_SET_ENAUTOCLR_CLKGATE_MASK;
    USBPHY->CTRL_SET = USBPHY_CTRL_SET_ENAUTOCLR_PHY_PWD_MASK;
#endif
}
