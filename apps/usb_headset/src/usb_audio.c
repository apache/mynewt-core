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

/*
 * TinyUSB audio class callbacks: clock source and feature unit requests,
 * alternate setting tracking and speaker data notification.
 * Descriptors come from hw/usb/tinyusb/std_descriptors.
 */

#include <assert.h>
#include <string.h>
#include <syscfg/syscfg.h>
#include <os/mynewt.h>
#include <tusb.h>
#include <class/audio/audio_device.h>
#include <usbd_audio_desc.h>

#include "usb_headset.h"

struct usb_headset usb_headset;

/* Q15 gain for 0, -1, ... -5 dB; every further 6 dB halves the value. */
static const uint16_t gain_q15_table[6] = {
    32768, 29205, 26029, 23198, 20675, 18427
};

static uint16_t
db_to_gain_q15(int db)
{
    int attenuation = -db;

    if (attenuation < 0) {
        attenuation = 0;
    }

    return gain_q15_table[attenuation % 6] >> (attenuation / 6);
}

/*
 * Only master channel controls of both feature units are applied to the
 * mixed signal.  Per channel values are stored and reported back to host.
 */
static void
update_gain(void)
{
    uint32_t gain;

    if (usb_headset.spk.mute[0] || usb_headset.mic.mute[0]) {
        gain = 0;
    } else {
        gain = db_to_gain_q15(usb_headset.spk.volume[0] / 256);
        gain = (gain * db_to_gain_q15(usb_headset.mic.volume[0] / 256)) >> 15;
    }

    usb_headset.gain_q15 = (uint16_t)gain;
}

static bool
clock_get(uint8_t rhport, const tusb_control_request_t *req)
{
    uint8_t selector = TU_U16_HIGH(req->wValue);
    audio20_control_cur_4_t cur_freq;
    audio20_control_range_4_n_t(1) range_freq;
    audio20_control_cur_1_t cur_valid;

    if (selector == AUDIO20_CS_CTRL_SAM_FREQ) {
        if (req->bRequest == AUDIO20_CS_REQ_CUR) {
            cur_freq.bCur = (int32_t)tu_htole32(USBD_AUDIO_SAMPLE_RATE);
            return tud_audio_buffer_and_schedule_control_xfer(
                rhport, req, &cur_freq, sizeof(cur_freq));
        } else if (req->bRequest == AUDIO20_CS_REQ_RANGE) {
            /*
             * Host may first ask for 2 bytes to learn number of subranges,
             * TinyUSB truncates reply to wLength.
             */
            range_freq.wNumSubRanges = tu_htole16(1);
            range_freq.subrange[0].bMin = (int32_t)tu_htole32(USBD_AUDIO_SAMPLE_RATE);
            range_freq.subrange[0].bMax = (int32_t)tu_htole32(USBD_AUDIO_SAMPLE_RATE);
            range_freq.subrange[0].bRes = 0;
            return tud_audio_buffer_and_schedule_control_xfer(
                rhport, req, &range_freq, sizeof(range_freq));
        }
    } else if (selector == AUDIO20_CS_CTRL_CLK_VALID &&
               req->bRequest == AUDIO20_CS_REQ_CUR) {
        cur_valid.bCur = 1;
        return tud_audio_buffer_and_schedule_control_xfer(rhport, req, &cur_valid,
                                                          sizeof(cur_valid));
    }

    return false;
}

/*
 * Clock frequency is read-only in descriptors, accept only request
 * that does not change it.
 */
static bool
clock_set(const tusb_control_request_t *req, const uint8_t *buf)
{
    uint32_t freq;

    if (TU_U16_HIGH(req->wValue) == AUDIO20_CS_CTRL_SAM_FREQ &&
        req->bRequest == AUDIO20_CS_REQ_CUR &&
        req->wLength == sizeof(audio20_control_cur_4_t)) {
        memcpy(&freq, buf, sizeof(freq));
        return tu_le32toh(freq) == USBD_AUDIO_SAMPLE_RATE;
    }

    return false;
}

static bool
fu_get(uint8_t rhport, const tusb_control_request_t *req,
       const struct usb_headset_fu *fu, uint8_t channels)
{
    uint8_t selector = TU_U16_HIGH(req->wValue);
    uint8_t channel = TU_U16_LOW(req->wValue);
    audio20_control_cur_1_t cur_mute;
    audio20_control_cur_2_t cur_vol;
    audio20_control_range_2_n_t(1) range_vol;

    if (channel > channels) {
        return false;
    }

    if (selector == AUDIO20_FU_CTRL_MUTE && req->bRequest == AUDIO20_CS_REQ_CUR) {
        cur_mute.bCur = fu->mute[channel];
        return tud_audio_buffer_and_schedule_control_xfer(rhport, req,
                                                          &cur_mute,
                                                          sizeof(cur_mute));
    } else if (selector == AUDIO20_FU_CTRL_VOLUME) {
        if (req->bRequest == AUDIO20_CS_REQ_CUR) {
            cur_vol.bCur = (int16_t)tu_htole16(fu->volume[channel]);
            return tud_audio_buffer_and_schedule_control_xfer(rhport, req, &cur_vol,
                                                              sizeof(cur_vol));
        } else if (req->bRequest == AUDIO20_CS_REQ_RANGE) {
            range_vol.wNumSubRanges = tu_htole16(1);
            range_vol.subrange[0].bMin =
                (int16_t)tu_htole16(USB_HEADSET_VOLUME_MIN_DB * 256);
            range_vol.subrange[0].bMax =
                (int16_t)tu_htole16(USB_HEADSET_VOLUME_MAX_DB * 256);
            range_vol.subrange[0].bRes = tu_htole16(256);
            return tud_audio_buffer_and_schedule_control_xfer(
                rhport, req, &range_vol, sizeof(range_vol));
        }
    }

    return false;
}

static bool
fu_set(const tusb_control_request_t *req, const uint8_t *buf,
       struct usb_headset_fu *fu, uint8_t channels)
{
    uint8_t selector = TU_U16_HIGH(req->wValue);
    uint8_t channel = TU_U16_LOW(req->wValue);
    int16_t volume;

    if (channel > channels || req->bRequest != AUDIO20_CS_REQ_CUR) {
        return false;
    }

    if (selector == AUDIO20_FU_CTRL_MUTE) {
        if (req->wLength != sizeof(audio20_control_cur_1_t)) {
            return false;
        }
        fu->mute[channel] = buf[0] != 0;
    } else if (selector == AUDIO20_FU_CTRL_VOLUME) {
        if (req->wLength != sizeof(audio20_control_cur_2_t)) {
            return false;
        }
        memcpy(&volume, buf, sizeof(volume));
        volume = (int16_t)tu_le16toh((uint16_t)volume);
        if (volume < USB_HEADSET_VOLUME_MIN_DB * 256) {
            volume = USB_HEADSET_VOLUME_MIN_DB * 256;
        } else if (volume > USB_HEADSET_VOLUME_MAX_DB * 256) {
            volume = USB_HEADSET_VOLUME_MAX_DB * 256;
        }
        fu->volume[channel] = volume;
    } else {
        return false;
    }

    update_gain();

    return true;
}

bool
tud_audio_get_req_entity_cb(uint8_t rhport, const tusb_control_request_t *req)
{
    switch (TU_U16_HIGH(req->wIndex)) {
    case USBD_AUDIO_ENTITY_CLOCK:
        return clock_get(rhport, req);
    case USBD_AUDIO_ENTITY_SPK_FEATURE_UNIT:
        return fu_get(rhport, req, &usb_headset.spk, USBD_AUDIO_OUT_CHANNELS);
    case USBD_AUDIO_ENTITY_MIC_FEATURE_UNIT:
        return fu_get(rhport, req, &usb_headset.mic, USBD_AUDIO_IN_CHANNELS);
    default:
        /* Not supported request, TinyUSB stalls it. */
        return false;
    }
}

bool
tud_audio_set_req_entity_cb(uint8_t rhport, const tusb_control_request_t *req,
                            uint8_t *buf)
{
    (void)rhport;

    switch (TU_U16_HIGH(req->wIndex)) {
    case USBD_AUDIO_ENTITY_CLOCK:
        return clock_set(req, buf);
    case USBD_AUDIO_ENTITY_SPK_FEATURE_UNIT:
        return fu_set(req, buf, &usb_headset.spk, USBD_AUDIO_OUT_CHANNELS);
    case USBD_AUDIO_ENTITY_MIC_FEATURE_UNIT:
        return fu_set(req, buf, &usb_headset.mic, USBD_AUDIO_IN_CHANNELS);
    default:
        return false;
    }
}

static void
set_alt(const tusb_control_request_t *req)
{
    uint8_t itf = TU_U16_LOW(req->wIndex);
    uint8_t alt = TU_U16_LOW(req->wValue);

    if (itf == usb_headset.spk_itf) {
        usb_headset.spk_alt = alt;
    } else if (itf == usb_headset.mic_itf) {
        usb_headset.mic_alt = alt;
    }
}

/* Called when streaming alternate setting is selected. */
bool
tud_audio_set_itf_cb(uint8_t rhport, const tusb_control_request_t *req)
{
    (void)rhport;

    set_alt(req);

    return true;
}

/* Called when alternate setting 0 (no streaming) is selected. */
bool
tud_audio_set_itf_close_ep_cb(uint8_t rhport, const tusb_control_request_t *req)
{
    (void)rhport;

    set_alt(req);

    return true;
}

/* Speaker packet was received and stored in TinyUSB FIFO. */
bool
tud_audio_rx_done_isr(uint8_t rhport, uint16_t n_bytes_received,
                      uint8_t func_id, uint8_t ep_out, uint8_t cur_alt_setting)
{
    (void)rhport;
    (void)n_bytes_received;
    (void)func_id;
    (void)ep_out;
    (void)cur_alt_setting;

    usb_headset_data_ready();

    return true;
}

/*
 * Find audio streaming interface numbers in configuration descriptor.
 * Streaming alternate setting has endpoint whose direction tells whether
 * it is speaker (OUT) or microphone (IN) interface.  This way application
 * does not depend on other functions (like CDC) placed before audio.
 */
static void
find_streaming_interfaces(void)
{
    const uint8_t *desc = tud_descriptor_configuration_cb(0);
    const tusb_desc_configuration_t *cfg = (const void *)desc;
    const uint8_t *end = desc + tu_le16toh(cfg->wTotalLength);
    const tusb_desc_interface_t *itf;
    const tusb_desc_endpoint_t *ep;
    const uint8_t *p;
    const uint8_t *q;

    for (p = tu_desc_next(desc); p < end; p = tu_desc_next(p)) {
        if (tu_desc_type(p) != TUSB_DESC_INTERFACE) {
            continue;
        }
        itf = (const void *)p;
        if (itf->bInterfaceClass != TUSB_CLASS_AUDIO ||
            itf->bInterfaceSubClass != AUDIO_SUBCLASS_STREAMING ||
            itf->bNumEndpoints == 0) {
            continue;
        }
        /* First endpoint of this alternate setting */
        for (q = tu_desc_next(p); q < end && tu_desc_type(q) != TUSB_DESC_INTERFACE;
             q = tu_desc_next(q)) {
            if (tu_desc_type(q) == TUSB_DESC_ENDPOINT) {
                ep = (const void *)q;
                if (tu_edpt_dir(ep->bEndpointAddress) == TUSB_DIR_OUT) {
                    usb_headset.spk_itf = itf->bInterfaceNumber;
                } else {
                    usb_headset.mic_itf = itf->bInterfaceNumber;
                }
                break;
            }
        }
    }
}

void
usb_audio_init(void)
{
    /* Unmuted, 0 dB on all channels. */
    memset(&usb_headset, 0, sizeof(usb_headset));
    usb_headset.spk_itf = 0xFF;
    usb_headset.mic_itf = 0xFF;
    update_gain();

    find_streaming_interfaces();
    assert(usb_headset.spk_itf != 0xFF);
    assert(usb_headset.mic_itf != 0xFF);
}
