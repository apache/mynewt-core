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
 * USB headset example: stereo samples received from host on speaker
 * endpoint are mixed to mono and sent back to host on microphone endpoint.
 */

#include <string.h>
#include <os/mynewt.h>
#include <console/console.h>
#include <tusb.h>
#include <class/audio/audio_device.h>
#include <tinyusb/tinyusb.h>

#include "usb_headset.h"

#if USBD_AUDIO_BYTES_PER_SAMPLE != 2
#error This application mixes 16 bit samples only
#endif
#if USBD_AUDIO_OUT_CHANNELS != 2 || USBD_AUDIO_IN_CHANNELS != 1
#error This application mixes stereo speaker to mono microphone only
#endif

/* Speaker frame: one sample for each channel. */
#define FRAME_BYTES     (USBD_AUDIO_BYTES_PER_SAMPLE * USBD_AUDIO_OUT_CHANNELS)
/* Process up to 2 ms of audio at a time. */
#define CHUNK_FRAMES    (2 * (USBD_AUDIO_SAMPLE_RATE / 1000))

static int16_t spk_buf[CHUNK_FRAMES * USBD_AUDIO_OUT_CHANNELS];
static int16_t mic_buf[CHUNK_FRAMES];

static void pump_ev_cb(struct os_event *ev);

static struct os_event pump_ev = {
    .ev_cb = pump_ev_cb,
};

void
usb_headset_data_ready(void)
{
    if (!OS_EVENT_QUEUED(&pump_ev)) {
        os_eventq_put(os_eventq_dflt_get(), &pump_ev);
    }
}

/*
 * Mix left and right channel to mono applying master gain.
 * (left + right) * gain_q15 >> 16 averages two channels and scales
 * them in one step; values fit in 32 bits for full scale input.
 */
static void
mix_to_mono(const int16_t *stereo, int16_t *mono, uint16_t frames,
            int32_t gain)
{
    uint16_t i;
    int32_t sum;

    for (i = 0; i < frames; ++i) {
        sum = (int32_t)stereo[2 * i] + stereo[2 * i + 1];
        mono[i] = (int16_t)((sum * gain) >> 16);
    }
}

static void
pump_ev_cb(struct os_event *ev)
{
    uint16_t available;
    uint16_t bytes;
    uint16_t frames;

    (void)ev;

    while ((available = tud_audio_available()) >= FRAME_BYTES) {
        bytes = min(available, sizeof(spk_buf));
        bytes -= bytes % FRAME_BYTES;
        bytes = tud_audio_read(spk_buf, bytes);
        frames = bytes / FRAME_BYTES;

        mix_to_mono(spk_buf, mic_buf, frames, usb_headset.gain_q15);

        /* Microphone FIFO is only consumed when host streams from it. */
        if (usb_headset.mic_alt != 0) {
            tud_audio_write(mic_buf, frames * sizeof(mic_buf[0]));
        }
    }
}

int
mynewt_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    sysinit();

    usb_audio_init();
    tinyusb_start();

    console_printf("USB headset started\n");

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }

    return 0;
}
