/**
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

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "sysinit/sysinit.h"
#include "os/os.h"
#include "bsp/bsp.h"
#include "console/console.h"
#include "hal/hal_gpio.h"
#include "tinycrypt/aes.h"
#include "osdp/osdp.h"
#include "osdp/osdp_common.h"
#ifdef ARCH_sim
#include "mcu/mcu_sim.h"
#endif

#define COMMAND_HANDLER_INTERVAL (OS_TICKS_PER_SEC)

#if MYNEWT_VAL(OSDP_MODE_PD)
#define OSDP_KEY_STRING MYNEWT_VAL(OSDP_PD_SCBK)
#else
#define OSDP_KEY_STRING MYNEWT_VAL(OSDP_MASTER_KEY)
#endif

/* Use the first "per user" log module slot for test module. */
#define LOG_MODULE (LOG_MODULE_PERUSER + 0)
/* Convenience macro for logging to the app module. */
#define LOG(lvl, ...) LOG_ ## lvl(&g_logger, LOG_MODULE, __VA_ARGS__)

extern struct log g_logger;
struct log g_logger;

static struct os_callout cmd_timer;

static int create_keypress_event(struct osdp_event *event);
static int create_cardreader_event(struct osdp_event *event);
static int create_mfgreply_event(struct osdp_cmd *oc);
static int pd_command_handler(void *arg, struct osdp_cmd *cmd);

/* From osdp.h */
static const char *const osdp_cmd_str[] = {
    "OSDP_CMD_OUTPUT",
    "OSDP_CMD_LED",
    "OSDP_CMD_BUZZER",
    "OSDP_CMD_TEXT",
    "OSDP_CMD_KEYSET",
    "OSDP_CMD_COMSET",
    "OSDP_CMD_MFG",
    "OSDP_CMD_SENTINEL"
};

/*
 * Callback function registered with the library
 * Commands received from CP will be reflected here
 */
static int
pd_command_handler(void *arg, struct osdp_cmd *cmd)
{
    (void)(arg);
    int ret = 0;

    LOG(INFO, "CMD [%d] = %s\n", cmd->id, osdp_cmd_str[cmd->id - 1]);

    switch (cmd->id) {
    case OSDP_CMD_COMSET:
        break;
    case OSDP_CMD_BUZZER:
        LOG(INFO, "\n\trdr: %d,\n\tctrl_code: %d,\n\ton_ct: %d,\n\toff_ct: %d,\n\trep_count: %d\n",
          cmd->buzzer.reader,
          cmd->buzzer.control_code,
          cmd->buzzer.on_count,
          cmd->buzzer.off_count,
          cmd->buzzer.rep_count
            );
        break;
    case OSDP_CMD_LED:
        LOG(INFO,
            "\n\trdr: %d,\n\tctrl_code: %d,\n\tled_num: %d,\n\ton_ct: %d,\n\toff_ct: %d,\n\ton_clr: %d,\n\toff_clr: %d,\n\ttmr_ct: %d\n",
          cmd->led.reader,
          cmd->led.temporary.control_code,
          cmd->led.led_number,
          cmd->led.temporary.on_count,
          cmd->led.temporary.off_count,
          cmd->led.temporary.on_color,
          cmd->led.temporary.off_color,
          cmd->led.temporary.timer_count
            );
        break;
    case OSDP_CMD_TEXT:
        LOG(INFO,
            "\n\trdr: %d,\n\tctrl_code: %d,\n\ttemp_time: %d,\n\toffset_row: %d,\n\toffset_col: %d,\n\tdata: %s\n",
          cmd->text.reader,
          cmd->text.control_code,
          cmd->text.temp_time,
          cmd->text.offset_row,
          cmd->text.offset_col,
          cmd->text.data
            );
        break;
    case OSDP_CMD_MFG:
        LOG(INFO, "\n\tv_code: 0x%04x,\n\tcmd: %d,\n\tlen: %d,\n\tdata: ",
          cmd->mfg.vendor_code,
          cmd->mfg.command,
          cmd->mfg.length
            );
        for (int i = 0; i < cmd->mfg.length; ++i) {
            printf("%02x ", cmd->mfg.data[i]);
        }
        LOG(INFO, "\n");
        /* Send manufacturer specific reply. Indicate ret > 0 to send reply */
        LOG(INFO, "Sending manufacturer specific reply\n");
        create_mfgreply_event(cmd);
        ret = 1;
        break;
    default:
        break;
    }

    return ret;
}

/*
 *
 */
static int
create_mfgreply_event(struct osdp_cmd *oc)
{
    int i, data_length, vendor_code, mfg_command;
    struct osdp_cmd_mfg *cmd = &oc->mfg;

    /* Sample reply */
    uint8_t data_bytes[] = {"ManufacturerReply"};
    data_length = sizeof(data_bytes);
    vendor_code = 0x00030201;
    mfg_command = 14;

    cmd->vendor_code = (uint32_t)vendor_code;
    cmd->command = mfg_command;
    cmd->length = data_length;
    for (i = 0; i < cmd->length; i++) {
        cmd->data[i] = data_bytes[i];
    }

    return 0;
}

static int
create_cardreader_event(struct osdp_event *event)
{
    int i, data_length, reader_no, format, direction, len_bytes;
    struct osdp_event_cardread *ev = &event->cardread;

    /* Sample parameters */
    uint8_t data_bytes[] = {"CardCredentials"};
    data_length = sizeof(data_bytes);
    reader_no = 1;
    format = OSDP_CARD_FMT_ASCII;
    direction = 1;
    event->type = OSDP_EVENT_CARDREAD;

    if (format == OSDP_CARD_FMT_RAW_WIEGAND ||
        format == OSDP_CARD_FMT_RAW_UNSPECIFIED) {
        len_bytes = (data_length + 7) / 8; /* bits to bytes */
    } else {
        len_bytes = data_length;
    }

    if (len_bytes > OSDP_EVENT_MAX_DATALEN) {
        return -1;
    }

    ev->reader_no = (uint8_t)reader_no;
    ev->format = (uint8_t)format;
    ev->direction = (uint8_t)direction;
    ev->length = len_bytes;
    for (i = 0; i < len_bytes; i++) {
        ev->data[i] = data_bytes[i];
    }

    return 0;
}

static int
create_keypress_event(struct osdp_event *event)
{
    int i, data_length, reader_no;
    struct osdp_event_keypress *ev = &event->keypress;

    /* Sample parameters */
    event->type = OSDP_EVENT_KEYPRESS;
    uint8_t data_bytes[] = {"KeyPress"};
    data_length = sizeof(data_bytes);
    reader_no = 1;

    ev->reader_no = (uint8_t)reader_no;
    ev->length = data_length;
    for (i = 0; i < ev->length; i++) {
        ev->data[i] = data_bytes[i];
    }

    return 0;
}

/*
 * Handler called after cmd_handler timer timeout
 */
static void
cmd_handler(struct os_event *ev)
{
    assert(ev != NULL);
    struct osdp *ctx = osdp_get_ctx();
    static int cmd = 0;

    uint32_t sc_active = osdp_get_sc_status_mask(ctx);

    if (sc_active) {
        struct osdp_event event;
        if (cmd == 0) {
            LOG(INFO, "Sending Card Read\n");
            create_cardreader_event(&event);
        } else if (cmd == 1) {
            LOG(INFO, "Sending Key Press\n");
            create_keypress_event(&event);
        }
        osdp_pd_notify_event(ctx, &event);
        cmd++;
        if (cmd > 1) {
            cmd = 0;
        }
    }

    /* Heartbeat blink */
    hal_gpio_toggle(LED_BLINK_PIN);

    /* Restart */
    os_callout_reset(&cmd_timer, COMMAND_HANDLER_INTERVAL);
}

/*
 * Initialize all timers
 */
static int
timers_init(void)
{
    /* Configure and reset timer */
    os_callout_init(&cmd_timer, os_eventq_dflt_get(),
      cmd_handler, NULL);

    os_callout_reset(&cmd_timer, COMMAND_HANDLER_INTERVAL);

    return 0;
}

/*
 * Simple test for OSDP encryption routine
 */
static int
test_encryption_wrappers(uint8_t *key, uint8_t len)
{
    int rc;
    uint8_t plaintext_ref[TC_AES_BLOCK_SIZE] =
    {0xff, 0x53, 0x65, 0x13, 0x00, 0x0e, 0x03, 0x11, 0x00, 0x01, 0x76, 0x3b, 0x24, 0xdf, 0x92, 0x5b};
    uint8_t data_buffer[TC_AES_BLOCK_SIZE];
    uint8_t iv_buffer[TC_AES_BLOCK_SIZE];

    /* Test AES-CBC */
    memcpy(data_buffer, plaintext_ref, TC_AES_BLOCK_SIZE);
    osdp_get_rand(iv_buffer, TC_AES_BLOCK_SIZE); /* Create initial vector */
    osdp_encrypt(key, iv_buffer, data_buffer,
      TC_AES_BLOCK_SIZE); /* Encrypt using initial vector in CBC mode */
    osdp_decrypt(key, iv_buffer, data_buffer, TC_AES_BLOCK_SIZE); /* Decrypt */

    rc = memcmp(data_buffer, plaintext_ref, TC_AES_BLOCK_SIZE);
    if (rc) {
        return rc;
    }

    /* Test AES-ECB */
    memcpy(data_buffer, plaintext_ref, TC_AES_BLOCK_SIZE);
    osdp_encrypt(key, NULL, data_buffer,
      TC_AES_BLOCK_SIZE); /* Encrypt without initial vector, i.e ECB mode */
    osdp_decrypt(key, NULL, data_buffer, TC_AES_BLOCK_SIZE); /* Decrypt */

    rc = memcmp(data_buffer, plaintext_ref, TC_AES_BLOCK_SIZE);
    if (rc) {
        return rc;
    }

    return rc;
}

int
mynewt_main(int argc, char **argv)
{
    int rc, len;
    uint8_t key_buf[16];
    uint8_t *key = NULL;

    log_register("osdp_main_log", &g_logger, &log_console_handler, NULL, LOG_SYSLEVEL);

    sysinit();

    hal_gpio_init_out(LED_BLINK_PIN, 1);

    /* List capabilities of this PD */
    struct osdp_pd_cap cap[] = {
        {
            .function_code = OSDP_PD_CAP_READER_LED_CONTROL,
            .compliance_level = 1,
            .num_items = 1
        },
        {
            .function_code = OSDP_PD_CAP_READER_AUDIBLE_OUTPUT,
            .compliance_level = 1,
            .num_items = 1
        },
        {
            .function_code = OSDP_PD_CAP_OUTPUT_CONTROL,
            .compliance_level = 1,
            .num_items = 1
        },
        {
            .function_code = OSDP_PD_CAP_READER_TEXT_OUTPUT,
            .compliance_level = 1,
            .num_items = 1
        },
        { -1, 0, 0 }
    };

    osdp_pd_info_t info_pd = {
        .address = MYNEWT_VAL(OSDP_PD_ADDRESS),
        .baud_rate = MYNEWT_VAL(OSDP_UART_BAUD_RATE),
        .flags = 0,
        .channel.send = NULL, /* Managed by the library */
        .channel.recv = NULL, /* Managed by the library */
        .channel.flush = NULL, /* Managed by the library */
        .channel.data = NULL, /* Managed by the library */
        .id = {
            .version = MYNEWT_VAL(OSDP_PD_ID_VERSION),
            .model = MYNEWT_VAL(OSDP_PD_ID_MODEL),
            .vendor_code = MYNEWT_VAL(OSDP_PD_ID_VENDOR_CODE),
            .serial_number = MYNEWT_VAL(OSDP_PD_ID_SERIAL_NUMBER),
            .firmware_version = MYNEWT_VAL(OSDP_PD_ID_FIRMWARE_VERSION),
        },
        .cap = cap,
        .pd_cb = pd_command_handler
    };

    /* Validate and assign key */
    if (MYNEWT_VAL(OSDP_SC_ENABLED) && strcmp(OSDP_KEY_STRING, "NONE") != 0) {

        len = strlen(OSDP_KEY_STRING);
        assert(len == 32);

        len = hex2bin(OSDP_KEY_STRING, 32, key_buf, 16);
        assert(len == 16);

        rc = test_encryption_wrappers(key_buf, 16);
        assert(rc == 0);

        key = key_buf;
    }

    /* Initialize the library module */
    osdp_init(&info_pd, key);

    timers_init();

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }

    assert(0);

    return 0;
}
