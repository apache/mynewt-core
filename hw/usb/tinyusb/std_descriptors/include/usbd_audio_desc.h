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

#ifndef H_USBD_AUDIO_DESC_
#define H_USBD_AUDIO_DESC_

#include <syscfg/syscfg.h>
#include <tusb.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * USB Audio Class 2.0 function descriptors built from syscfg values:
 *
 *   USBD_AUDIO                  IN (mic), OUT (speaker), IN_OUT (headset)
 *   USBD_AUDIO_SAMPLE_RATE      sampling frequency in Hz
 *   USBD_AUDIO_SAMPLE_SIZE      bits per sample
 *   USBD_AUDIO_BYTES_PER_SAMPLE bytes per sample (subslot size)
 *   USBD_AUDIO_IN_CHANNELS      number of microphone channels
 *   USBD_AUDIO_OUT_CHANNELS     number of speaker channels
 *   USBD_AUDIO_IN_TERMINAL_TYPE  microphone input terminal type
 *                                (AUDIO_TERM_TYPE_IN_*)
 *   USBD_AUDIO_OUT_TERMINAL_TYPE speaker output terminal type
 *                                (AUDIO_TERM_TYPE_OUT_*)
 *   USBD_AUDIO_VOLUME_CONTROL   feature unit with mute and volume controls
 *   USBD_AUDIO_FEEDBACK         feedback endpoint for speaker path
 *   USBD_AUDIO_INT_EP           interrupt endpoint for control changes
 *
 * Function has one clock source shared by all terminals, one alternate
 * setting with audio data per streaming interface and PCM type I format.
 * Layout of the function:
 *
 *   Interface N     Audio Control:
 *                     Clock Source (4)
 *                     Speaker path:
 *                       Input Terminal (1) -> [Feature Unit (2)] ->
 *                       Output Terminal (3)
 *                     Microphone path:
 *                       Input Terminal (0x11) -> [Feature Unit (0x12)] ->
 *                       Output Terminal (0x13)
 *                     [Interrupt endpoint]
 *   Interface N+1   Audio Streaming OUT (speaker),
 *                     alternate 0 (idle) and 1 (streaming)
 *   Interface N+2   Audio Streaming IN (microphone),
 *                     alternate 0 (idle) and 1 (streaming)
 *
 * Only descriptors are provided here, application has to implement TinyUSB
 * audio class callbacks (tud_audio_*_cb).  Entity IDs below are needed to
 * handle requests passed to tud_audio_get_req_entity_cb() and
 * tud_audio_set_req_entity_cb().
 */
#define USBD_AUDIO_ENTITY_CLOCK                 0x04
/* Speaker path (host -> device) */
#define USBD_AUDIO_ENTITY_SPK_INPUT_TERMINAL    0x01
#define USBD_AUDIO_ENTITY_SPK_FEATURE_UNIT      0x02
#define USBD_AUDIO_ENTITY_SPK_OUTPUT_TERMINAL   0x03
/* Microphone path (device -> host) */
#define USBD_AUDIO_ENTITY_MIC_INPUT_TERMINAL    0x11
#define USBD_AUDIO_ENTITY_MIC_FEATURE_UNIT      0x12
#define USBD_AUDIO_ENTITY_MIC_OUTPUT_TERMINAL   0x13

#if CFG_TUD_AUDIO

#if MYNEWT_VAL(USBD_AUDIO_VOLUME_CONTROL) && \
    (CFG_TUD_AUDIO_ENABLE_EP_OUT && USBD_AUDIO_OUT_CHANNELS > 8 || \
     CFG_TUD_AUDIO_ENABLE_EP_IN && USBD_AUDIO_IN_CHANNELS > 8)
#error USBD_AUDIO_VOLUME_CONTROL supports up to 8 channels
#endif

/* Number of USB interfaces used by audio function (AC + AS interfaces). */
#define USBD_AUDIO_ITF_COUNT \
    (1 + CFG_TUD_AUDIO_ENABLE_EP_OUT + CFG_TUD_AUDIO_ENABLE_EP_IN)

/* Endpoint interval resulting in 1 ms packets for full and high speed. */
#define USBD_AUDIO_EP_INTERVAL \
    ((USBD_RHPORT_MODE & OPT_MODE_HIGH_SPEED) ? 4 : 1)

/* Function category and default terminal types depend on direction(s). */
#if CFG_TUD_AUDIO_ENABLE_EP_OUT && CFG_TUD_AUDIO_ENABLE_EP_IN
#define USBD_AUDIO_FUNCTION_CATEGORY    AUDIO20_FUNC_HEADSET
#define USBD_AUDIO_SPK_TERMINAL_DEFAULT AUDIO_TERM_TYPE_OUT_HEADPHONES
/* Terminals of both paths are associated with each other. */
#define USBD_AUDIO_SPK_IT_ASSOC         USBD_AUDIO_ENTITY_MIC_OUTPUT_TERMINAL
#define USBD_AUDIO_MIC_OT_ASSOC         USBD_AUDIO_ENTITY_SPK_INPUT_TERMINAL
#elif CFG_TUD_AUDIO_ENABLE_EP_OUT
#define USBD_AUDIO_FUNCTION_CATEGORY    AUDIO20_FUNC_DESKTOP_SPEAKER
#define USBD_AUDIO_SPK_TERMINAL_DEFAULT AUDIO_TERM_TYPE_OUT_DESKTOP_SPEAKER
#define USBD_AUDIO_SPK_IT_ASSOC         0x00
#else
#define USBD_AUDIO_FUNCTION_CATEGORY    AUDIO20_FUNC_MICROPHONE
#define USBD_AUDIO_MIC_OT_ASSOC         0x00
#endif
#define USBD_AUDIO_MIC_TERMINAL_DEFAULT AUDIO_TERM_TYPE_IN_GENERIC_MIC

/* Terminal types, from syscfg when set (AUDIO_TERM_TYPE_* name or number). */
#if defined(MYNEWT_VAL_USBD_AUDIO_OUT_TERMINAL_TYPE)
#define USBD_AUDIO_SPK_TERMINAL_TYPE   MYNEWT_VAL(USBD_AUDIO_OUT_TERMINAL_TYPE)
#else
#define USBD_AUDIO_SPK_TERMINAL_TYPE   USBD_AUDIO_SPK_TERMINAL_DEFAULT
#endif
#if defined(MYNEWT_VAL_USBD_AUDIO_IN_TERMINAL_TYPE)
#define USBD_AUDIO_MIC_TERMINAL_TYPE   MYNEWT_VAL(USBD_AUDIO_IN_TERMINAL_TYPE)
#else
#define USBD_AUDIO_MIC_TERMINAL_TYPE   USBD_AUDIO_MIC_TERMINAL_DEFAULT
#endif

/*
 * Spatial location of channels: mono - front center, stereo - left and
 * right, other - not specified.
 */
#define USBD_AUDIO_CHANNEL_CONFIG(_nch) \
    ((_nch) == 1 ? AUDIO20_CHANNEL_CONFIG_FRONT_CENTER : \
     (_nch) == 2 ? (AUDIO20_CHANNEL_CONFIG_FRONT_LEFT | AUDIO20_CHANNEL_CONFIG_FRONT_RIGHT) : \
     AUDIO20_CHANNEL_CONFIG_NON_PREDEFINED)

/*
 * Feature unit controls: mute and volume, read/write, for master channel
 * (first) and every logical channel.  USBD_AUDIO_FU_CTRLS_n expands to
 * n + 1 values.
 * Channel counts come from syscfg as parenthesized values, so they can not be
 * pasted into macro names and #if ladders are used to select the list.
 */
#define USBD_AUDIO_FU_CTRL \
    ((AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_MUTE_POS) | \
     (AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_VOLUME_POS))
#define USBD_AUDIO_FU_CTRLS_1       USBD_AUDIO_FU_CTRL, USBD_AUDIO_FU_CTRL
#define USBD_AUDIO_FU_CTRLS_2       USBD_AUDIO_FU_CTRLS_1, USBD_AUDIO_FU_CTRL
#define USBD_AUDIO_FU_CTRLS_3       USBD_AUDIO_FU_CTRLS_2, USBD_AUDIO_FU_CTRL
#define USBD_AUDIO_FU_CTRLS_4       USBD_AUDIO_FU_CTRLS_3, USBD_AUDIO_FU_CTRL
#define USBD_AUDIO_FU_CTRLS_5       USBD_AUDIO_FU_CTRLS_4, USBD_AUDIO_FU_CTRL
#define USBD_AUDIO_FU_CTRLS_6       USBD_AUDIO_FU_CTRLS_5, USBD_AUDIO_FU_CTRL
#define USBD_AUDIO_FU_CTRLS_7       USBD_AUDIO_FU_CTRLS_6, USBD_AUDIO_FU_CTRL
#define USBD_AUDIO_FU_CTRLS_8       USBD_AUDIO_FU_CTRLS_7, USBD_AUDIO_FU_CTRL

#if USBD_AUDIO_OUT_CHANNELS == 1
#define USBD_AUDIO_SPK_FU_CTRLS     USBD_AUDIO_FU_CTRLS_1
#elif USBD_AUDIO_OUT_CHANNELS == 2
#define USBD_AUDIO_SPK_FU_CTRLS     USBD_AUDIO_FU_CTRLS_2
#elif USBD_AUDIO_OUT_CHANNELS == 3
#define USBD_AUDIO_SPK_FU_CTRLS     USBD_AUDIO_FU_CTRLS_3
#elif USBD_AUDIO_OUT_CHANNELS == 4
#define USBD_AUDIO_SPK_FU_CTRLS     USBD_AUDIO_FU_CTRLS_4
#elif USBD_AUDIO_OUT_CHANNELS == 5
#define USBD_AUDIO_SPK_FU_CTRLS     USBD_AUDIO_FU_CTRLS_5
#elif USBD_AUDIO_OUT_CHANNELS == 6
#define USBD_AUDIO_SPK_FU_CTRLS     USBD_AUDIO_FU_CTRLS_6
#elif USBD_AUDIO_OUT_CHANNELS == 7
#define USBD_AUDIO_SPK_FU_CTRLS     USBD_AUDIO_FU_CTRLS_7
#else
#define USBD_AUDIO_SPK_FU_CTRLS     USBD_AUDIO_FU_CTRLS_8
#endif

#if USBD_AUDIO_IN_CHANNELS == 1
#define USBD_AUDIO_MIC_FU_CTRLS     USBD_AUDIO_FU_CTRLS_1
#elif USBD_AUDIO_IN_CHANNELS == 2
#define USBD_AUDIO_MIC_FU_CTRLS     USBD_AUDIO_FU_CTRLS_2
#elif USBD_AUDIO_IN_CHANNELS == 3
#define USBD_AUDIO_MIC_FU_CTRLS     USBD_AUDIO_FU_CTRLS_3
#elif USBD_AUDIO_IN_CHANNELS == 4
#define USBD_AUDIO_MIC_FU_CTRLS     USBD_AUDIO_FU_CTRLS_4
#elif USBD_AUDIO_IN_CHANNELS == 5
#define USBD_AUDIO_MIC_FU_CTRLS     USBD_AUDIO_FU_CTRLS_5
#elif USBD_AUDIO_IN_CHANNELS == 6
#define USBD_AUDIO_MIC_FU_CTRLS     USBD_AUDIO_FU_CTRLS_6
#elif USBD_AUDIO_IN_CHANNELS == 7
#define USBD_AUDIO_MIC_FU_CTRLS     USBD_AUDIO_FU_CTRLS_7
#else
#define USBD_AUDIO_MIC_FU_CTRLS     USBD_AUDIO_FU_CTRLS_8
#endif

#if MYNEWT_VAL(USBD_AUDIO_VOLUME_CONTROL)
#define USBD_AUDIO_FU_LEN(_nch)     TUD_AUDIO20_DESC_FEATURE_UNIT_LEN(_nch)
#define USBD_AUDIO_FU_DESC(_unitid, _srcid, _ctrls) \
    /* Feature Unit Descriptor(4.7.2.8) */ \
    , TUD_AUDIO20_DESC_FEATURE_UNIT(_unitid, _srcid, 0x00, _ctrls)
#define USBD_AUDIO_SPK_OT_SOURCE    USBD_AUDIO_ENTITY_SPK_FEATURE_UNIT
#define USBD_AUDIO_MIC_OT_SOURCE    USBD_AUDIO_ENTITY_MIC_FEATURE_UNIT
#else
#define USBD_AUDIO_FU_LEN(_nch)     0
#define USBD_AUDIO_FU_DESC(_unitid, _srcid, _ctrls)
#define USBD_AUDIO_SPK_OT_SOURCE    USBD_AUDIO_ENTITY_SPK_INPUT_TERMINAL
#define USBD_AUDIO_MIC_OT_SOURCE    USBD_AUDIO_ENTITY_MIC_INPUT_TERMINAL
#endif

/*
 * Optional parts of the descriptor are defined as macros that expand to
 * nothing when the part is not needed.  Each part starts with a comma so
 * it can follow the previous one.
 */

/* Audio Control entities of speaker path */
#if CFG_TUD_AUDIO_ENABLE_EP_OUT
#define USBD_AUDIO_AC_SPK_ENTITIES_LEN \
    (TUD_AUDIO20_DESC_INPUT_TERM_LEN + USBD_AUDIO_FU_LEN(USBD_AUDIO_OUT_CHANNELS) + \
     TUD_AUDIO20_DESC_OUTPUT_TERM_LEN)
#define USBD_AUDIO_AC_SPK_ENTITIES \
    /* Input Terminal Descriptor(4.7.2.4) - USB stream from host */ \
    , TUD_AUDIO20_DESC_INPUT_TERM(USBD_AUDIO_ENTITY_SPK_INPUT_TERMINAL, AUDIO_TERM_TYPE_USB_STREAMING, \
                                  USBD_AUDIO_SPK_IT_ASSOC, USBD_AUDIO_ENTITY_CLOCK, USBD_AUDIO_OUT_CHANNELS, \
                                  USBD_AUDIO_CHANNEL_CONFIG(USBD_AUDIO_OUT_CHANNELS), 0x00, 0x0000, 0x00) \
    USBD_AUDIO_FU_DESC(USBD_AUDIO_ENTITY_SPK_FEATURE_UNIT, USBD_AUDIO_ENTITY_SPK_INPUT_TERMINAL, \
                       USBD_AUDIO_SPK_FU_CTRLS) \
    /* Output Terminal Descriptor(4.7.2.5) - speaker */ \
    , TUD_AUDIO20_DESC_OUTPUT_TERM(USBD_AUDIO_ENTITY_SPK_OUTPUT_TERMINAL, USBD_AUDIO_SPK_TERMINAL_TYPE, 0x00, \
                                   USBD_AUDIO_SPK_OT_SOURCE, USBD_AUDIO_ENTITY_CLOCK, 0x0000, 0x00)
#else
#define USBD_AUDIO_AC_SPK_ENTITIES_LEN  0
#define USBD_AUDIO_AC_SPK_ENTITIES
#endif

/* Audio Control entities of microphone path */
#if CFG_TUD_AUDIO_ENABLE_EP_IN
#define USBD_AUDIO_AC_MIC_ENTITIES_LEN \
    (TUD_AUDIO20_DESC_INPUT_TERM_LEN + USBD_AUDIO_FU_LEN(USBD_AUDIO_IN_CHANNELS) + \
     TUD_AUDIO20_DESC_OUTPUT_TERM_LEN)
#define USBD_AUDIO_AC_MIC_ENTITIES \
    /* Input Terminal Descriptor(4.7.2.4) - microphone */ \
    , TUD_AUDIO20_DESC_INPUT_TERM(USBD_AUDIO_ENTITY_MIC_INPUT_TERMINAL, USBD_AUDIO_MIC_TERMINAL_TYPE, \
                                  0x00, USBD_AUDIO_ENTITY_CLOCK, USBD_AUDIO_IN_CHANNELS, \
                                  USBD_AUDIO_CHANNEL_CONFIG(USBD_AUDIO_IN_CHANNELS), 0x00, 0x0000, 0x00) \
    USBD_AUDIO_FU_DESC(USBD_AUDIO_ENTITY_MIC_FEATURE_UNIT, USBD_AUDIO_ENTITY_MIC_INPUT_TERMINAL, \
                       USBD_AUDIO_MIC_FU_CTRLS) \
    /* Output Terminal Descriptor(4.7.2.5) - USB stream to host */ \
    , TUD_AUDIO20_DESC_OUTPUT_TERM(USBD_AUDIO_ENTITY_MIC_OUTPUT_TERMINAL, AUDIO_TERM_TYPE_USB_STREAMING, \
                                   USBD_AUDIO_MIC_OT_ASSOC, USBD_AUDIO_MIC_OT_SOURCE, USBD_AUDIO_ENTITY_CLOCK, \
                                   0x0000, 0x00)
#else
#define USBD_AUDIO_AC_MIC_ENTITIES_LEN  0
#define USBD_AUDIO_AC_MIC_ENTITIES
#endif

#define USBD_AUDIO_AC_ENTITIES_LEN \
    (TUD_AUDIO20_DESC_CLK_SRC_LEN + USBD_AUDIO_AC_SPK_ENTITIES_LEN + USBD_AUDIO_AC_MIC_ENTITIES_LEN)

/* Audio Control interrupt endpoint */
#if CFG_TUD_AUDIO_ENABLE_INTERRUPT_EP
#define USBD_AUDIO_AC_INT_EP_LEN        TUD_AUDIO20_DESC_STD_AC_INT_EP_LEN
#define USBD_AUDIO_AC_INT_EP \
    /* Standard AC Interrupt Endpoint Descriptor(4.8.2.1) */ \
    , TUD_AUDIO20_DESC_STD_AC_INT_EP(USBD_AUDIO_INT_EP, USBD_AUDIO_EP_INTERVAL)
#else
#define USBD_AUDIO_AC_INT_EP_LEN        0
#define USBD_AUDIO_AC_INT_EP
#endif

/* Audio Streaming interface of speaker path */
#if CFG_TUD_AUDIO_ENABLE_EP_OUT
#if CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP
/* Rate is driven by device clock, host adjusts to feedback. */
#define USBD_AUDIO_EP_OUT_SYNC          TUSB_ISO_EP_ATT_ASYNCHRONOUS
#define USBD_AUDIO_AS_OUT_FB_EP_LEN     TUD_AUDIO20_DESC_STD_AS_ISO_FB_EP_LEN
#define USBD_AUDIO_AS_OUT_FB_EP \
    /* Standard AS Isochronous Feedback Endpoint Descriptor(4.10.2.1) */ \
    , TUD_AUDIO20_DESC_STD_AS_ISO_FB_EP(USBD_AUDIO_FEEDBACK_EP, 4, USBD_AUDIO_EP_INTERVAL)
#else
/* Device adapts to the rate host sends data at. */
#define USBD_AUDIO_EP_OUT_SYNC          TUSB_ISO_EP_ATT_ADAPTIVE
#define USBD_AUDIO_AS_OUT_FB_EP_LEN     0
#define USBD_AUDIO_AS_OUT_FB_EP
#endif
#define USBD_AUDIO_AS_OUT_DESC_LEN \
    (2 * TUD_AUDIO20_DESC_STD_AS_LEN + TUD_AUDIO20_DESC_CS_AS_INT_LEN + \
     TUD_AUDIO20_DESC_TYPE_I_FORMAT_LEN + TUD_AUDIO20_DESC_STD_AS_ISO_EP_LEN + \
     TUD_AUDIO20_DESC_CS_AS_ISO_EP_LEN + USBD_AUDIO_AS_OUT_FB_EP_LEN)
#define USBD_AUDIO_AS_OUT_DESC(_itfnum) \
    /* Standard AS Interface Descriptor(4.9.1), alternate 0 - no bandwidth */ \
    , TUD_AUDIO20_DESC_STD_AS_INT(_itfnum, 0x00, 0x00, 0x00) \
    /* Standard AS Interface Descriptor(4.9.1), alternate 1 - streaming */ \
    , TUD_AUDIO20_DESC_STD_AS_INT(_itfnum, 0x01, 1 + CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP, 0x00) \
    /* Class-Specific AS Interface Descriptor(4.9.2) */ \
    , TUD_AUDIO20_DESC_CS_AS_INT(USBD_AUDIO_ENTITY_SPK_INPUT_TERMINAL, AUDIO20_CTRL_NONE, \
                                 AUDIO20_FORMAT_TYPE_I, AUDIO20_DATA_FORMAT_TYPE_I_PCM, USBD_AUDIO_OUT_CHANNELS, \
                                 USBD_AUDIO_CHANNEL_CONFIG(USBD_AUDIO_OUT_CHANNELS), 0x00) \
    /* Type I Format Type Descriptor(2.3.1.6 - Audio Formats) */ \
    , TUD_AUDIO20_DESC_TYPE_I_FORMAT(USBD_AUDIO_BYTES_PER_SAMPLE, USBD_AUDIO_SAMPLE_SIZE) \
    /* Standard AS Isochronous Audio Data Endpoint Descriptor(4.10.1.1) */ \
    , TUD_AUDIO20_DESC_STD_AS_ISO_EP(USBD_AUDIO_OUT_EP, \
                                     (uint8_t)(TUSB_XFER_ISOCHRONOUS | USBD_AUDIO_EP_OUT_SYNC | TUSB_ISO_EP_ATT_DATA), \
                                     USBD_AUDIO_EP_OUT_SZ, USBD_AUDIO_EP_INTERVAL) \
    /* Class-Specific AS Isochronous Data Endpoint Descriptor(4.10.1.2) */ \
    , TUD_AUDIO20_DESC_CS_AS_ISO_EP(AUDIO20_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK, AUDIO20_CTRL_NONE, \
                                    AUDIO20_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_MILLISEC, 0x0001) \
    USBD_AUDIO_AS_OUT_FB_EP
#else
#define USBD_AUDIO_AS_OUT_DESC_LEN      0
#define USBD_AUDIO_AS_OUT_DESC(_itfnum)
#endif

/* Audio Streaming interface of microphone path */
#if CFG_TUD_AUDIO_ENABLE_EP_IN
#define USBD_AUDIO_AS_IN_DESC_LEN \
    (2 * TUD_AUDIO20_DESC_STD_AS_LEN + TUD_AUDIO20_DESC_CS_AS_INT_LEN + \
     TUD_AUDIO20_DESC_TYPE_I_FORMAT_LEN + TUD_AUDIO20_DESC_STD_AS_ISO_EP_LEN + \
     TUD_AUDIO20_DESC_CS_AS_ISO_EP_LEN)
#define USBD_AUDIO_AS_IN_DESC(_itfnum) \
    /* Standard AS Interface Descriptor(4.9.1), alternate 0 - no bandwidth */ \
    , TUD_AUDIO20_DESC_STD_AS_INT(_itfnum, 0x00, 0x00, 0x00) \
    /* Standard AS Interface Descriptor(4.9.1), alternate 1 - streaming */ \
    , TUD_AUDIO20_DESC_STD_AS_INT(_itfnum, 0x01, 0x01, 0x00) \
    /* Class-Specific AS Interface Descriptor(4.9.2) */ \
    , TUD_AUDIO20_DESC_CS_AS_INT(USBD_AUDIO_ENTITY_MIC_OUTPUT_TERMINAL, AUDIO20_CTRL_NONE, \
                                 AUDIO20_FORMAT_TYPE_I, AUDIO20_DATA_FORMAT_TYPE_I_PCM, USBD_AUDIO_IN_CHANNELS, \
                                 USBD_AUDIO_CHANNEL_CONFIG(USBD_AUDIO_IN_CHANNELS), 0x00) \
    /* Type I Format Type Descriptor(2.3.1.6 - Audio Formats) */ \
    , TUD_AUDIO20_DESC_TYPE_I_FORMAT(USBD_AUDIO_BYTES_PER_SAMPLE, USBD_AUDIO_SAMPLE_SIZE) \
    /* Standard AS Isochronous Audio Data Endpoint Descriptor(4.10.1.1) */ \
    , TUD_AUDIO20_DESC_STD_AS_ISO_EP(USBD_AUDIO_IN_EP, \
                                     (uint8_t)(TUSB_XFER_ISOCHRONOUS | TUSB_ISO_EP_ATT_ASYNCHRONOUS | TUSB_ISO_EP_ATT_DATA), \
                                     USBD_AUDIO_EP_IN_SZ, USBD_AUDIO_EP_INTERVAL) \
    /* Class-Specific AS Isochronous Data Endpoint Descriptor(4.10.1.2) */ \
    , TUD_AUDIO20_DESC_CS_AS_ISO_EP(AUDIO20_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK, AUDIO20_CTRL_NONE, \
                                    AUDIO20_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_UNDEFINED, 0x0000)
#else
#define USBD_AUDIO_AS_IN_DESC_LEN       0
#define USBD_AUDIO_AS_IN_DESC(_itfnum)
#endif

/* Total length of audio function descriptors. */
#define USBD_AUDIO_DESC_LEN \
    (TUD_AUDIO20_DESC_IAD_LEN + TUD_AUDIO20_DESC_STD_AC_LEN + TUD_AUDIO20_DESC_CS_AC_LEN + \
     USBD_AUDIO_AC_ENTITIES_LEN + USBD_AUDIO_AC_INT_EP_LEN + \
     USBD_AUDIO_AS_OUT_DESC_LEN + USBD_AUDIO_AS_IN_DESC_LEN)

/*
 * Audio function descriptors.
 * _itfnum - number of Audio Control interface, streaming interfaces follow it
 *           (OUT first when both are present).
 * _stridx - string descriptor index of Audio Control interface.
 */
#define USBD_AUDIO20_DESCRIPTOR(_itfnum, _stridx) \
    /* Standard Interface Association Descriptor (IAD) */ \
    TUD_AUDIO20_DESC_IAD(_itfnum, USBD_AUDIO_ITF_COUNT, 0x00), \
    /* Standard AC Interface Descriptor(4.7.1) */ \
    TUD_AUDIO20_DESC_STD_AC(_itfnum, CFG_TUD_AUDIO_ENABLE_INTERRUPT_EP, _stridx), \
    /* Class-Specific AC Interface Header Descriptor(4.7.2) */ \
    TUD_AUDIO20_DESC_CS_AC(0x0200, USBD_AUDIO_FUNCTION_CATEGORY, USBD_AUDIO_AC_ENTITIES_LEN, AUDIO20_CTRL_NONE), \
    /* Clock Source Descriptor(4.7.2.1), fixed frequency readable by host */ \
    TUD_AUDIO20_DESC_CLK_SRC(USBD_AUDIO_ENTITY_CLOCK, AUDIO20_CLOCK_SOURCE_ATT_INT_FIX_CLK, \
                             (AUDIO20_CTRL_R << AUDIO20_CLOCK_SOURCE_CTRL_CLK_FRQ_POS), 0x00, 0x00) \
    USBD_AUDIO_AC_SPK_ENTITIES \
    USBD_AUDIO_AC_MIC_ENTITIES \
    USBD_AUDIO_AC_INT_EP \
    USBD_AUDIO_AS_OUT_DESC((_itfnum) + 1) \
    USBD_AUDIO_AS_IN_DESC((_itfnum) + 1 + CFG_TUD_AUDIO_ENABLE_EP_OUT)

#else /* CFG_TUD_AUDIO */

#define USBD_AUDIO_ITF_COUNT    0
#define USBD_AUDIO_DESC_LEN     0

#endif /* CFG_TUD_AUDIO */

// 5.2.2 Control Request Layout - was removed from TinyUSB
typedef struct TU_ATTR_PACKED {
    union {
        struct TU_ATTR_PACKED {
            uint8_t recipient : 5; ///< Recipient type tusb_request_recipient_t.
            uint8_t type : 2;      ///< Request type tusb_request_type_t.
            uint8_t direction : 1; ///< Direction type. tusb_dir_t
        } bmRequestType_bit;

        uint8_t bmRequestType;
    };

    uint8_t bRequest; ///< Request type audio_cs_req_t
    uint8_t bChannelNumber;
    uint8_t bControlSelector;
    union {
        uint8_t bInterface;
        uint8_t bEndpoint;
    };
    uint8_t bEntityID;
    uint16_t wLength;
} audio_control_request_t;

#ifdef __cplusplus
}
#endif

#endif /* H_USBD_AUDIO_DESC_ */
