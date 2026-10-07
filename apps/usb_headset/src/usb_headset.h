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

#ifndef H_USB_HEADSET_
#define H_USB_HEADSET_

#include <stdint.h>
#include <stdbool.h>
#include <syscfg/syscfg.h>
#include <tusb.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Volume range exposed to host, 1 dB steps. */
#define USB_HEADSET_VOLUME_MIN_DB   (-60)
#define USB_HEADSET_VOLUME_MAX_DB   0

/* Feature units in descriptors support up to 8 channels plus master. */
#define USB_HEADSET_FU_MAX_CHANNELS 8

/* Mute and volume state of one feature unit, index 0 is master channel. */
struct usb_headset_fu {
    uint8_t mute[USB_HEADSET_FU_MAX_CHANNELS + 1];
    /* Volume in 1/256 dB units as used by USB Audio Class. */
    int16_t volume[USB_HEADSET_FU_MAX_CHANNELS + 1];
};

struct usb_headset {
    struct usb_headset_fu spk;
    struct usb_headset_fu mic;
    /* Audio streaming interface numbers, found in configuration descriptor. */
    uint8_t spk_itf;
    uint8_t mic_itf;
    /* Current alternate settings, 0 means stream is not active. */
    volatile uint8_t spk_alt;
    volatile uint8_t mic_alt;
    /* Gain applied to mixed signal, Q15, 0 when muted. */
    volatile uint16_t gain_q15;
};

extern struct usb_headset usb_headset;

/* Initialize state and find interface numbers (usb_audio.c). */
void usb_audio_init(void);

/* Called when new speaker data is available in TinyUSB FIFO (main.c). */
void usb_headset_data_ready(void);

#ifdef __cplusplus
}
#endif

#endif /* H_USB_HEADSET_ */
