/*
 * Copyright (c) 2020 Siddharth Chandrasekaran <siddharth@embedjournal.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "modlog/modlog.h"

#include <stdlib.h>
#include <string.h>

#include "osdp/osdp_common.h"

#if MYNEWT_VAL(OSDP_MODE_PD) /* compile flag based on mode */

#define TAG "PD: "

#define CMD_POLL_DATA_LEN              0
#define CMD_LSTAT_DATA_LEN             0
#define CMD_ISTAT_DATA_LEN             0
#define CMD_OSTAT_DATA_LEN             0
#define CMD_RSTAT_DATA_LEN             0
#define CMD_ID_DATA_LEN                1
#define CMD_CAP_DATA_LEN               1
#define CMD_OUT_DATA_LEN               4
#define CMD_LED_DATA_LEN               14
#define CMD_BUZ_DATA_LEN               5
#define CMD_TEXT_DATA_LEN              6   /* variable length command */
#define CMD_COMSET_DATA_LEN            5
#define CMD_MFG_DATA_LEN               4   /* variable length command */
#define CMD_KEYSET_DATA_LEN            18
#define CMD_CHLNG_DATA_LEN             8
#define CMD_SCRYPT_DATA_LEN            16

#define REPLY_ACK_LEN                  1
#define REPLY_PDID_LEN                 13
#define REPLY_PDCAP_LEN                1   /* variable length command */
#define REPLY_PDCAP_ENTITY_LEN         3
#define REPLY_LSTATR_LEN               3
#define REPLY_RSTATR_LEN               2
#define REPLY_KEYPAD_LEN               2
#define REPLY_RAW_LEN                  4
#define REPLY_FMT_LEN                  3
#define REPLY_COM_LEN                  6
#define REPLY_NAK_LEN                  2
#define REPLY_MFGREP_LEN               4   /* variable length command */
#define REPLY_CCRYPT_LEN               33
#define REPLY_RMAC_I_LEN               17

#define OSDP_PD_ERR_NONE               0
#define OSDP_PD_ERR_NO_DATA            1
#define OSDP_PD_ERR_GENERIC           -1
#define OSDP_PD_ERR_REPLY             -2
#define OSDP_PD_ERR_EMPTY_Q           -3
#define OSDP_PD_ERR_IGNORE            -4

/* Implicit cababilities */
static struct osdp_pd_cap osdp_pd_cap[] = {
    /* Driver Implicit cababilities */
    {
        OSDP_PD_CAP_CHECK_CHARACTER_SUPPORT,
        1, /* The PD supports the 16-bit CRC-16 mode */
        0, /* N/A */
    },
    { -1, 0, 0 } /* Sentinel */
};

static int
pd_event_queue_init(struct osdp_pd *pd)
{
    int rc;
    rc = os_mempool_init(&pd->event.pool,
          MYNEWT_VAL(OSDP_PD_COMMAND_QUEUE_SIZE),
          sizeof(struct pd_event_node),
          pd->event.pool_buf, "pd_event_pool");

    if (rc != OS_OK) {
        OSDP_LOG_ERROR("osdp: pd: Failed to initialize command pool\n");
        return rc;
    }

    pd->event.queue.tqh_first = NULL;
    pd->event.queue.tqh_last = &pd->event.queue.tqh_first;

    os_mutex_init(&pd->lock);

    return rc;
}

static struct osdp_event *
pd_event_alloc(struct osdp_pd *pd)
{
    struct pd_event_node *event = NULL;

    event = os_memblock_get(&pd->event.pool);

    if (event == NULL) {
        OSDP_LOG_ERROR("osdp: pd: Event pool allocation failed\n");
        return NULL;
    }

    return &event->object;
}

static void
pd_event_free(struct osdp_pd *pd, struct osdp_event *event)
{
    struct pd_event_node *node;

    node = CONTAINER_OF(event, struct pd_event_node, object);
    os_memblock_put(&pd->event.pool, node);
}

static void
pd_event_enqueue(struct osdp_pd *pd, struct osdp_event *event)
{
    struct pd_event_node *node;

    node = CONTAINER_OF(event, struct pd_event_node, object);
    TAILQ_INSERT_HEAD(&pd->event.queue, node, pd_node);
}

static int
pd_event_dequeue(struct osdp_pd *pd, struct osdp_event **event)
{
    struct pd_event_node *node;

    node = TAILQ_LAST(&pd->event.queue, queue);
    if (node == NULL) {
        return OSDP_PD_ERR_EMPTY_Q;
    }
    TAILQ_REMOVE(&pd->event.queue, node, pd_node);

    *event = &node->object;
    return 0;
}

static void
pd_event_queue_del(struct osdp_pd *pd)
{
    /* Empty the queue and put back blocks */
    struct osdp_event *event;
    while (pd_event_dequeue(pd, &event) == 0) {
        pd_event_free(pd, event);
    }

    os_mempool_clear(&pd->event.pool);
    os_mempool_unregister(&pd->event.pool);
}

static int
pd_translate_event(struct osdp_event *event, uint8_t *data)
{
    int reply_code = 0;

    switch (event->type) {
    case OSDP_EVENT_CARDREAD:
        if (event->cardread.format == OSDP_CARD_FMT_RAW_UNSPECIFIED ||
            event->cardread.format == OSDP_CARD_FMT_RAW_WIEGAND) {
            reply_code = REPLY_RAW;
        } else if (event->cardread.format == OSDP_CARD_FMT_ASCII) {
            reply_code = REPLY_FMT;
        } else {
            OSDP_LOG_ERROR("osdp: pd: Event: cardread; Error: unknown format\n");
            break;
        }
        break;
    case OSDP_EVENT_KEYPRESS:
        reply_code = REPLY_KEYPPAD;
        break;
    default:
        OSDP_LOG_ERROR("osdp: pd: Unknown event type %d\n", event->type);
        break;
    }
    if (reply_code == 0) {
        /* POLL command cannot fail even when there are errors here */
        return REPLY_ACK;
    }
    memcpy(data, event, sizeof(struct osdp_event));
    return reply_code;
}

static int
pd_event_get(struct osdp_pd *pd, struct osdp_event **event, int *ret)
{
    int rc = 0;

    rc = osdp_device_lock(&pd->lock);
    if (rc) {
        return rc;
    }

    rc = pd_event_dequeue(pd, event);
    if (rc) {
        goto err;
    }

    *ret = pd_translate_event(*event, pd->ephemeral_data);
    pd_event_free(pd, *event);

err:
    osdp_device_unlock(&pd->lock);
    return rc;
}

static int
pd_event_put(struct osdp_pd *pd, struct osdp_event *event)
{
    int rc = 0;
    struct osdp_event *ev;

    rc = osdp_device_lock(&pd->lock);
    if (rc) {
        return rc;
    }

    ev = pd_event_alloc(pd);
    if (ev == NULL) {
        rc = OS_ENOMEM;
        goto err;
    }

    memcpy(ev, event, sizeof(struct osdp_event));
    pd_event_enqueue(pd, ev);

err:
    osdp_device_unlock(&pd->lock);
    return rc;
}

static int
pd_cmd_cap_ok(struct osdp_pd *pd, struct osdp_cmd *cmd)
{
    struct osdp_pd_cap *cap = NULL;

    /* Validate the cmd_id against a PD capabilities where applicable */
    switch (pd->cmd_id) {
    case CMD_ISTAT:
        cap = &pd->cap[OSDP_PD_CAP_CONTACT_STATUS_MONITORING];
        if (cap->num_items == 0 || cap->compliance_level == 0) {
            break;
        }
        return 0; /* Remove this when REPLY_ISTATR is supported */
    case CMD_OSTAT:
        cap = &pd->cap[OSDP_PD_CAP_OUTPUT_CONTROL];
        if (cap->num_items == 0 || cap->compliance_level == 0) {
            break;
        }
        return 0; /* Remove this when REPLY_OSTATR is supported */
    case CMD_OUT:
        cap = &pd->cap[OSDP_PD_CAP_OUTPUT_CONTROL];
        if (cmd->output.output_no + 1 > cap->num_items) {
            OSDP_LOG_DEBUG("osdp: pd: CAP check: output_no(%d) > cap->num_items(%d)\n",
            cmd->output.output_no + 1, cap->num_items);
            break;
        }
        if (cap->compliance_level == 0) {
            break;
        }
        return 1;
    case CMD_LED:
        cap = &pd->cap[OSDP_PD_CAP_READER_LED_CONTROL];
        if (cmd->led.led_number + 1 > cap->num_items) {
            OSDP_LOG_DEBUG("osdp: pd: CAP check: LED(%d) > cap->num_items(%d)\n",
            cmd->led.led_number + 1, cap->num_items);
            break;
        }
        if (cap->compliance_level == 0) {
            break;
        }
        return 1;
    case CMD_BUZ:
        cap = &pd->cap[OSDP_PD_CAP_READER_AUDIBLE_OUTPUT];
        if (cap->num_items == 0 || cap->compliance_level == 0) {
            break;
        }
        return 1;
    case CMD_TEXT:
        cap = &pd->cap[OSDP_PD_CAP_READER_TEXT_OUTPUT];
        if (cap->num_items == 0 || cap->compliance_level == 0) {
            break;
        }
        return 1;
    case CMD_CHLNG:
    case CMD_SCRYPT:
    case CMD_KEYSET:
        cap = &pd->cap[OSDP_PD_CAP_COMMUNICATION_SECURITY];
        if (cap->compliance_level == 0) {
            pd->reply_id = REPLY_NAK;
            pd->ephemeral_data[0] = OSDP_PD_NAK_SC_UNSUP;
            return 0;
        }
        return 1;
    }

    pd->reply_id = REPLY_NAK;
    pd->ephemeral_data[0] = OSDP_PD_NAK_CMD_UNKNOWN;
    return 0;
}

static int
pd_decode_command(struct osdp_pd *pd, uint8_t *buf, int len)
{
    int i, ret = OSDP_PD_ERR_GENERIC, pos = 0;
    struct osdp_cmd cmd;
    struct osdp_event *event;

    pd->reply_id = 0;
    pd->cmd_id = cmd.id = buf[pos++];
    len--;

    if (ISSET_FLAG(pd, OSDP_FLAG_ENFORCE_SECURE) &&
        !ISSET_FLAG(pd, PD_FLAG_SC_ACTIVE)) {
        /**
         * Only CMD_ID, CMD_CAP and SC handshake commands (CMD_CHLNG
         * and CMD_SCRYPT) are allowed when SC is inactive and
         * ENFORCE_SECURE was requested.
         */
        if (pd->cmd_id != CMD_ID && pd->cmd_id != CMD_CAP &&
            pd->cmd_id != CMD_CHLNG && pd->cmd_id != CMD_SCRYPT) {
            OSDP_LOG_ERROR("osdp: pd: CMD(%02x) not allowed due to ENFORCE_SECURE\n",
          pd->cmd_id);
            pd->reply_id = REPLY_NAK;
            pd->ephemeral_data[0] = OSDP_PD_NAK_RECORD;
            return OSDP_PD_ERR_REPLY;
        }
    }

    /* helper macro, can be called from switch cases below */
#define PD_CMD_CAP_CHECK(pd, cmd)                                    \
    if (!pd_cmd_cap_ok(pd, cmd)) {                               \
        OSDP_LOG_INFO("osdp: pd: PD is not capable of handling CMD(%02x); "  \
        "Reply with NAK_CMD_UNKNOWN\n", pd->cmd_id);   \
        ret = OSDP_PD_ERR_REPLY;                             \
        break;                                               \
    }

#define ASSERT_LENGTH(got, exp)                                      \
    if (got != exp) {                                            \
        OSDP_LOG_ERROR("osdp: pd: CMD(%02x) length error! Got:%d, Exp:%d\n",    \
        pd->cmd_id, got, exp);                       \
        return OSDP_PD_ERR_GENERIC;                          \
    }

    switch (pd->cmd_id) {
    case CMD_POLL:
        ASSERT_LENGTH(len, CMD_POLL_DATA_LEN);
        /* Check if we have external events in the queue */
        if (pd_event_get(pd, &event, &ret) == 0) {
            pd->reply_id = ret;
        } else {
            pd->reply_id = REPLY_ACK;
        }
        ret = OSDP_PD_ERR_NONE;
        SET_FLAG(pd, PD_FLAG_CP_POLL_ACTIVE);
        break;
    case CMD_LSTAT:
        ASSERT_LENGTH(len, CMD_LSTAT_DATA_LEN);
        pd->reply_id = REPLY_LSTATR;
        ret = OSDP_PD_ERR_NONE;
        break;
    case CMD_ISTAT:
        ASSERT_LENGTH(len, CMD_ISTAT_DATA_LEN);
        PD_CMD_CAP_CHECK(pd, NULL);
        pd->reply_id = REPLY_ISTATR;
        ret = OSDP_PD_ERR_NONE;
        break;
    case CMD_OSTAT:
        ASSERT_LENGTH(len, CMD_OSTAT_DATA_LEN);
        PD_CMD_CAP_CHECK(pd, NULL);
        pd->reply_id = REPLY_OSTATR;
        ret = OSDP_PD_ERR_NONE;
        break;
    case CMD_RSTAT:
        ASSERT_LENGTH(len, CMD_RSTAT_DATA_LEN);
        pd->reply_id = REPLY_RSTATR;
        ret = OSDP_PD_ERR_NONE;
        break;
    case CMD_ID:
        ASSERT_LENGTH(len, CMD_ID_DATA_LEN);
        pos++;  /* Skip reply type info. */
        pd->reply_id = REPLY_PDID;
        ret = OSDP_PD_ERR_NONE;
        break;
    case CMD_CAP:
        ASSERT_LENGTH(len, CMD_CAP_DATA_LEN);
        pos++;  /* Skip reply type info. */
        pd->reply_id = REPLY_PDCAP;
        ret = OSDP_PD_ERR_NONE;
        break;
    case CMD_OUT:
        ASSERT_LENGTH(len, CMD_OUT_DATA_LEN);
        if (!pd->command_callback) {
            break;
        }
        cmd.id = OSDP_CMD_OUTPUT;
        cmd.output.output_no = buf[pos++];
        cmd.output.control_code = buf[pos++];
        cmd.output.timer_count = buf[pos++];
        cmd.output.timer_count |= buf[pos++] << 8;
        PD_CMD_CAP_CHECK(pd, &cmd);
        ret = pd->command_callback(pd->command_callback_arg, &cmd);
        if (ret != 0) {
            pd->reply_id = REPLY_NAK;
            pd->ephemeral_data[0] = OSDP_PD_NAK_RECORD;
            ret = OSDP_PD_ERR_REPLY;
            break;
        }
        pd->reply_id = REPLY_ACK;
        ret = OSDP_PD_ERR_NONE;
        break;
    case CMD_LED:
        ASSERT_LENGTH(len, CMD_LED_DATA_LEN);
        if (!pd->command_callback) {
            break;
        }
        cmd.id = OSDP_CMD_LED;
        cmd.led.reader = buf[pos++];
        cmd.led.led_number = buf[pos++];

        cmd.led.temporary.control_code = buf[pos++];
        cmd.led.temporary.on_count = buf[pos++];
        cmd.led.temporary.off_count = buf[pos++];
        cmd.led.temporary.on_color = buf[pos++];
        cmd.led.temporary.off_color = buf[pos++];
        cmd.led.temporary.timer_count = buf[pos++];
        cmd.led.temporary.timer_count |= buf[pos++] << 8;

        cmd.led.permanent.control_code = buf[pos++];
        cmd.led.permanent.on_count = buf[pos++];
        cmd.led.permanent.off_count = buf[pos++];
        cmd.led.permanent.on_color = buf[pos++];
        cmd.led.permanent.off_color = buf[pos++];
        PD_CMD_CAP_CHECK(pd, &cmd);
        ret = pd->command_callback(pd->command_callback_arg, &cmd);
        if (ret != 0) {
            pd->reply_id = REPLY_NAK;
            pd->ephemeral_data[0] = OSDP_PD_NAK_RECORD;
            ret = OSDP_PD_ERR_REPLY;
            break;
        }
        pd->reply_id = REPLY_ACK;
        ret = OSDP_PD_ERR_NONE;
        break;
    case CMD_BUZ:
        ASSERT_LENGTH(len, CMD_BUZ_DATA_LEN);
        if (!pd->command_callback) {
            break;
        }
        cmd.id = OSDP_CMD_BUZZER;
        cmd.buzzer.reader = buf[pos++];
        cmd.buzzer.control_code = buf[pos++];
        cmd.buzzer.on_count = buf[pos++];
        cmd.buzzer.off_count = buf[pos++];
        cmd.buzzer.rep_count = buf[pos++];
        PD_CMD_CAP_CHECK(pd, &cmd);
        ret = pd->command_callback(pd->command_callback_arg, &cmd);
        if (ret != 0) {
            pd->reply_id = REPLY_NAK;
            pd->ephemeral_data[0] = OSDP_PD_NAK_RECORD;
            ret = OSDP_PD_ERR_REPLY;
            break;
        }
        pd->reply_id = REPLY_ACK;
        ret = OSDP_PD_ERR_NONE;
        break;
    case CMD_TEXT:
        if (len < CMD_TEXT_DATA_LEN || !pd->command_callback) {
            break;
        }
        cmd.id = OSDP_CMD_TEXT;
        cmd.text.reader = buf[pos++];
        cmd.text.control_code = buf[pos++];
        cmd.text.temp_time = buf[pos++];
        cmd.text.offset_row = buf[pos++];
        cmd.text.offset_col = buf[pos++];
        cmd.text.length = buf[pos++];
        if (cmd.text.length > OSDP_CMD_TEXT_MAX_LEN ||
            ((len - CMD_TEXT_DATA_LEN) < cmd.text.length) ||
            cmd.text.length > OSDP_CMD_TEXT_MAX_LEN) {
            break;
        }
        for (i = 0; i < cmd.text.length; i++) {
            cmd.text.data[i] = buf[pos++];
        }
        PD_CMD_CAP_CHECK(pd, &cmd);
        ret = pd->command_callback(pd->command_callback_arg, &cmd);
        if (ret != 0) {
            pd->reply_id = REPLY_NAK;
            pd->ephemeral_data[0] = OSDP_PD_NAK_RECORD;
            ret = OSDP_PD_ERR_REPLY;
            break;
        }
        pd->reply_id = REPLY_ACK;
        ret = OSDP_PD_ERR_NONE;
        break;
    case CMD_COMSET:
        ASSERT_LENGTH(len, CMD_COMSET_DATA_LEN);
        if (!pd->command_callback) {
            break;
        }
        cmd.id = OSDP_CMD_COMSET;
        cmd.comset.address = buf[pos++];
        cmd.comset.baud_rate = buf[pos++];
        cmd.comset.baud_rate |= buf[pos++] << 8;
        cmd.comset.baud_rate |= buf[pos++] << 16;
        cmd.comset.baud_rate |= buf[pos++] << 24;
        if (cmd.comset.address >= 0x7F ||
            (cmd.comset.baud_rate != 9600 &&
             cmd.comset.baud_rate != 14400 &&
             cmd.comset.baud_rate != 19200 &&
             cmd.comset.baud_rate != 38400 &&
             cmd.comset.baud_rate != 57600 &&
             cmd.comset.baud_rate != 115200)) {
            OSDP_LOG_ERROR("osdp: pd: COMSET Failed! command discarded\n");
            cmd.comset.address = pd->address;
            cmd.comset.baud_rate = pd->baud_rate;
        }
        ret = pd->command_callback(pd->command_callback_arg, &cmd);
        if (ret != 0) {
            pd->reply_id = REPLY_NAK;
            pd->ephemeral_data[0] = OSDP_PD_NAK_RECORD;
            ret = OSDP_PD_ERR_REPLY;
            break;
        }
        memcpy(pd->ephemeral_data, &cmd, sizeof(struct osdp_cmd));
        pd->reply_id = REPLY_COM;
        ret = OSDP_PD_ERR_NONE;
        break;
    case CMD_MFG:
        if (len < CMD_MFG_DATA_LEN || !pd->command_callback) {
            break;
        }
        cmd.id = OSDP_CMD_MFG;
        cmd.mfg.vendor_code = buf[pos++]; /* vendor_code */
        cmd.mfg.vendor_code |= buf[pos++] << 8;
        cmd.mfg.vendor_code |= buf[pos++] << 16;
        cmd.mfg.command = buf[pos++];
        cmd.mfg.length = len - CMD_MFG_DATA_LEN;
        if (cmd.mfg.length > OSDP_CMD_MFG_MAX_DATALEN) {
            OSDP_LOG_ERROR("osdp: pd: cmd length error\n");
            break;
        }
        for (i = 0; i < cmd.mfg.length; i++) {
            cmd.mfg.data[i] = buf[pos++];
        }
        ret = pd->command_callback(pd->command_callback_arg, &cmd);
        if (ret < 0) { /* Errors */
            pd->reply_id = REPLY_NAK;
            pd->ephemeral_data[0] = OSDP_PD_NAK_RECORD;
            ret = OSDP_PD_ERR_REPLY;
            break;
        }
        if (ret > 0) { /* App wants to send a REPLY_MFGREP to the CP */
            memcpy(pd->ephemeral_data, &cmd, sizeof(struct osdp_cmd));
            pd->reply_id = REPLY_MFGREP;
        } else {
            pd->reply_id = REPLY_ACK;
        }
        ret = OSDP_PD_ERR_NONE;
        break;
    case CMD_KEYSET:
        PD_CMD_CAP_CHECK(pd, &cmd);
        ASSERT_LENGTH(len, CMD_KEYSET_DATA_LEN);
        /**
         * For CMD_KEYSET to be accepted, PD must be
         * ONLINE and SC_ACTIVE.
         */
        if (ISSET_FLAG(pd, PD_FLAG_SC_ACTIVE) == 0) {
            pd->reply_id = REPLY_NAK;
            pd->ephemeral_data[0] = OSDP_PD_NAK_SC_COND;
            OSDP_LOG_ERROR("osdp: pd: Keyset with SC inactive\n");
            break;
        }
        /* only key_type == 1 (SCBK) and key_len == 16 is supported */
        if (buf[pos] != 1 || buf[pos + 1] != 16) {
            OSDP_LOG_ERROR("osdp: pd: Keyset invalid len/type: %d/%d\n",
            buf[pos], buf[pos + 1]);
            break;
        }
        cmd.id = OSDP_CMD_KEYSET;
        cmd.keyset.type = buf[pos++];
        cmd.keyset.length = buf[pos++];
        memcpy(cmd.keyset.data, buf + pos, 16);
        memcpy(pd->sc.scbk, buf + pos, 16);
        ret = OSDP_PD_ERR_NONE;
        if (pd->command_callback) {
            ret = pd->command_callback(pd->command_callback_arg,
                &cmd);
        } else {
            OSDP_LOG_WARN("osdp: pd: Keyset without command callback trigger\n");
        }
        if (ret != 0) {
            pd->reply_id = REPLY_NAK;
            pd->ephemeral_data[0] = OSDP_PD_NAK_RECORD;
            ret = OSDP_PD_ERR_REPLY;
            break;
        }
        CLEAR_FLAG(pd, PD_FLAG_SC_USE_SCBKD);
        CLEAR_FLAG(pd, OSDP_FLAG_INSTALL_MODE);
        pd->reply_id = REPLY_ACK;
        ret = OSDP_PD_ERR_NONE;
        break;
    case CMD_CHLNG:
        PD_CMD_CAP_CHECK(pd, &cmd);
        ASSERT_LENGTH(len, CMD_CHLNG_DATA_LEN);
        osdp_sc_init(pd);
        CLEAR_FLAG(pd, PD_FLAG_SC_ACTIVE);
        for (i = 0; i < CMD_CHLNG_DATA_LEN; i++) {
            pd->sc.cp_random[i] = buf[pos++];
        }
        pd->reply_id = REPLY_CCRYPT;
        ret = OSDP_PD_ERR_NONE;
        break;
    case CMD_SCRYPT:
        PD_CMD_CAP_CHECK(pd, &cmd);
        ASSERT_LENGTH(len, CMD_SCRYPT_DATA_LEN);
        for (i = 0; i < CMD_SCRYPT_DATA_LEN; i++) {
            pd->sc.cp_cryptogram[i] = buf[pos++];
        }
        pd->reply_id = REPLY_RMAC_I;
        ret = OSDP_PD_ERR_NONE;
        break;
    default:
        OSDP_LOG_ERROR("osdp: pd: Unknown command ID %02x\n", pd->cmd_id);
        pd->reply_id = REPLY_NAK;
        pd->ephemeral_data[0] = OSDP_PD_NAK_CMD_UNKNOWN;
        ret = OSDP_PD_ERR_NONE;
        break;
    }

    if (ret != 0 && ret != OSDP_PD_ERR_REPLY) {
        OSDP_LOG_ERROR("osdp: pd: Invalid command structure. CMD: %02x, Len: %d ret: %d\n",
        pd->cmd_id, len, ret);
        pd->reply_id = REPLY_NAK;
        pd->ephemeral_data[0] = OSDP_PD_NAK_CMD_LEN;
        return OSDP_PD_ERR_REPLY;
    }

    if (pd->cmd_id != CMD_POLL) {
        OSDP_LOG_DEBUG("osdp: pd: CMD: %02x REPLY: %02x\n", pd->cmd_id, pd->reply_id);
    }

    return ret;
}

/**
 * Returns:
 * +ve: length of command
 * -ve: error
 */
static int
pd_build_reply(struct osdp_pd *pd, uint8_t *buf, int max_len)
{
    int i, data_off, len = 0, ret = -1;
    uint8_t t1, *smb;
    struct osdp_event *event;
    struct osdp_cmd *cmd;

    data_off = osdp_phy_packet_get_data_offset(pd, buf);
    smb = osdp_phy_packet_get_smb(pd, buf);
    buf += data_off;
    max_len -= data_off;

#define ASSERT_BUF_LEN(need)                                           \
    if (max_len < need) {                                          \
        OSDP_LOG_ERROR("osdp: pd: OOM at build REPLY(%02x) - have:%d, need:%d\n", \
        pd->reply_id, max_len, need);                  \
        return OSDP_PD_ERR_GENERIC;                            \
    }

    switch (pd->reply_id) {
    case REPLY_ACK:
        ASSERT_BUF_LEN(REPLY_ACK_LEN);
        buf[len++] = pd->reply_id;
        ret = OSDP_PD_ERR_NONE;
        break;
    case REPLY_PDID:
        ASSERT_BUF_LEN(REPLY_PDID_LEN);
        buf[len++] = pd->reply_id;

        buf[len++] = BYTE_0(pd->id.vendor_code);
        buf[len++] = BYTE_1(pd->id.vendor_code);
        buf[len++] = BYTE_2(pd->id.vendor_code);

        buf[len++] = pd->id.model;
        buf[len++] = pd->id.version;

        buf[len++] = BYTE_0(pd->id.serial_number);
        buf[len++] = BYTE_1(pd->id.serial_number);
        buf[len++] = BYTE_2(pd->id.serial_number);
        buf[len++] = BYTE_3(pd->id.serial_number);

        buf[len++] = BYTE_3(pd->id.firmware_version);
        buf[len++] = BYTE_2(pd->id.firmware_version);
        buf[len++] = BYTE_1(pd->id.firmware_version);
        ret = OSDP_PD_ERR_NONE;
        break;
    case REPLY_PDCAP:
        ASSERT_BUF_LEN(REPLY_PDCAP_LEN);
        buf[len++] = pd->reply_id;
        for (i = 1; i < OSDP_PD_CAP_SENTINEL; i++) {
            if (pd->cap[i].function_code != i) {
                continue;
            }
            if (max_len < REPLY_PDCAP_ENTITY_LEN) {
                OSDP_LOG_ERROR("osdp: pd: Out of buffer space!\n");
                break;
            }
            buf[len++] = i;
            buf[len++] = pd->cap[i].compliance_level;
            buf[len++] = pd->cap[i].num_items;
            max_len -= REPLY_PDCAP_ENTITY_LEN;
        }
        ret = OSDP_PD_ERR_NONE;
        break;
    case REPLY_LSTATR:
        ASSERT_BUF_LEN(REPLY_LSTATR_LEN);
        buf[len++] = pd->reply_id;
        buf[len++] = ISSET_FLAG(pd, PD_FLAG_TAMPER);
        buf[len++] = ISSET_FLAG(pd, PD_FLAG_POWER);
        ret = OSDP_PD_ERR_NONE;
        break;
    case REPLY_RSTATR:
        ASSERT_BUF_LEN(REPLY_RSTATR_LEN);
        buf[len++] = pd->reply_id;
        buf[len++] = ISSET_FLAG(pd, PD_FLAG_R_TAMPER);
        ret = OSDP_PD_ERR_NONE;
        break;
    case REPLY_KEYPPAD:
        event = (struct osdp_event *)pd->ephemeral_data;
        ASSERT_BUF_LEN(REPLY_KEYPAD_LEN + event->keypress.length);
        buf[len++] = pd->reply_id;
        buf[len++] = (uint8_t)event->keypress.reader_no;
        buf[len++] = (uint8_t)event->keypress.length;
        for (i = 0; i < event->keypress.length; i++) {
            buf[len++] = event->keypress.data[i];
        }
        ret = OSDP_PD_ERR_NONE;
        break;
    case REPLY_RAW:
        event = (struct osdp_event *)pd->ephemeral_data;
        t1 = (event->cardread.length + 7) / 8;
        ASSERT_BUF_LEN(REPLY_RAW_LEN + t1);
        buf[len++] = pd->reply_id;
        buf[len++] = (uint8_t)event->cardread.reader_no;
        buf[len++] = (uint8_t)event->cardread.format;
        buf[len++] = BYTE_0(event->cardread.length);
        buf[len++] = BYTE_1(event->cardread.length);
        for (i = 0; i < t1; i++) {
            buf[len++] = event->cardread.data[i];
        }
        ret = OSDP_PD_ERR_NONE;
        break;
    case REPLY_FMT:
        event = (struct osdp_event *)pd->ephemeral_data;
        ASSERT_BUF_LEN(REPLY_FMT_LEN + event->cardread.length);
        buf[len++] = pd->reply_id;
        buf[len++] = (uint8_t)event->cardread.reader_no;
        buf[len++] = (uint8_t)event->cardread.direction;
        buf[len++] = (uint8_t)event->cardread.length;
        for (i = 0; i < event->cardread.length; i++) {
            buf[len++] = event->cardread.data[i];
        }
        ret = OSDP_PD_ERR_NONE;
        break;
    case REPLY_COM:
        ASSERT_BUF_LEN(REPLY_COM_LEN);
        /**
         * If COMSET succeeds, the PD must reply with the old params and
         * then switch to the new params from then then on. We have the
         * new params in the commands struct that we just enqueued so
         * we can peek at tail of command queue and set that to
         * pd->addr/pd->baud_rate.
         */
        cmd = (struct osdp_cmd *)pd->ephemeral_data;
        buf[len++] = pd->reply_id;
        buf[len++] = cmd->comset.address;
        buf[len++] = BYTE_0(cmd->comset.baud_rate);
        buf[len++] = BYTE_1(cmd->comset.baud_rate);
        buf[len++] = BYTE_2(cmd->comset.baud_rate);
        buf[len++] = BYTE_3(cmd->comset.baud_rate);

        pd->address = (int)cmd->comset.address;
        pd->baud_rate = (int)cmd->comset.baud_rate;
        OSDP_LOG_INFO("osdp: pd: COMSET Succeeded! New PD-Addr: %d; Baud: %d\n",
          pd->address, pd->baud_rate);
        ret = OSDP_PD_ERR_NONE;
        break;
    case REPLY_NAK:
        ASSERT_BUF_LEN(REPLY_NAK_LEN);
        buf[len++] = pd->reply_id;
        buf[len++] = pd->ephemeral_data[0];
        ret = OSDP_PD_ERR_NONE;
        break;
    case REPLY_MFGREP:
        cmd = (struct osdp_cmd *)pd->ephemeral_data;
        ASSERT_BUF_LEN(REPLY_MFGREP_LEN + cmd->mfg.length);
        buf[len++] = pd->reply_id;
        buf[len++] = BYTE_0(cmd->mfg.vendor_code);
        buf[len++] = BYTE_1(cmd->mfg.vendor_code);
        buf[len++] = BYTE_2(cmd->mfg.vendor_code);
        buf[len++] = cmd->mfg.command;
        for (i = 0; i < cmd->mfg.length; i++) {
            buf[len++] = cmd->mfg.data[i];
        }
        ret = OSDP_PD_ERR_NONE;
        break;
    case REPLY_CCRYPT:
        if (smb == NULL) {
            break;
        }
        ASSERT_BUF_LEN(REPLY_CCRYPT_LEN);
        osdp_get_rand(pd->sc.pd_random, 8);
        osdp_compute_session_keys(TO_CTX(pd));
        osdp_compute_pd_cryptogram(pd);
        buf[len++] = pd->reply_id;
        for (i = 0; i < 8; i++) {
            buf[len++] = pd->sc.pd_client_uid[i];
        }
        for (i = 0; i < 8; i++) {
            buf[len++] = pd->sc.pd_random[i];
        }
        for (i = 0; i < 16; i++) {
            buf[len++] = pd->sc.pd_cryptogram[i];
        }
        smb[0] = 3;    /* length */
        smb[1] = SCS_12; /* type */
        smb[2] = ISSET_FLAG(pd, PD_FLAG_SC_USE_SCBKD) ? 0 : 1;
        ret = OSDP_PD_ERR_NONE;
        break;
    case REPLY_RMAC_I:
        if (smb == NULL) {
            break;
        }
        ASSERT_BUF_LEN(REPLY_RMAC_I_LEN);
        osdp_compute_rmac_i(pd);
        buf[len++] = pd->reply_id;
        for (i = 0; i < 16; i++) {
            buf[len++] = pd->sc.r_mac[i];
        }
        smb[0] = 3;     /* length */
        smb[1] = SCS_14; /* type */
        if (osdp_verify_cp_cryptogram(pd) == 0) {
            smb[2] = 1; /* CP auth succeeded */
            SET_FLAG(pd, PD_FLAG_SC_ACTIVE);
            pd->sc_tstamp = osdp_millis_now();
            if (ISSET_FLAG(pd, PD_FLAG_SC_USE_SCBKD)) {
                OSDP_LOG_WARN("osdp: pd: SC Active with SCBK-D\n");
            } else {
                OSDP_LOG_INFO("osdp: pd: SC Active\n");
            }
        } else {
            smb[2] = 0; /* CP auth failed */
            OSDP_LOG_WARN("osdp: pd: failed to verify CP_crypt\n");
        }
        ret = OSDP_PD_ERR_NONE;
        break;
    }

    if (smb && (smb[1] > SCS_14) && ISSET_FLAG(pd, PD_FLAG_SC_ACTIVE)) {
        smb[0] = 2; /* length */
        smb[1] = (len > 1) ? SCS_18 : SCS_16;
    }

    if (ret != 0) {
        /* catch all errors and report it as a RECORD error to CP */
        OSDP_LOG_ERROR("osdp: pd: Failed to build REPLY(%02x); Sending NAK instead!\n",
        pd->reply_id);
        ASSERT_BUF_LEN(REPLY_NAK_LEN);
        buf[0] = REPLY_NAK;
        buf[1] = OSDP_PD_NAK_RECORD;
        len = 2;
    }

    return len;
}

/**
 * pd_send_reply - blocking send; doesn't handle partials
 * Returns:
 *   0 - success
 *  -1 - failure
 */
static int
pd_send_reply(struct osdp_pd *pd)
{
    int ret, len;

    /* init packet buf with header */
    len = osdp_phy_packet_init(pd, pd->rx_buf, sizeof(pd->rx_buf));
    if (len < 0) {
        return OSDP_PD_ERR_GENERIC;
    }

    /* fill reply data */
    ret = pd_build_reply(pd, pd->rx_buf, sizeof(pd->rx_buf));
    if (ret <= 0) {
        return OSDP_PD_ERR_GENERIC;
    }
    len += ret;

    /* finalize packet */
    len = osdp_phy_packet_finalize(pd, pd->rx_buf, len, sizeof(pd->rx_buf));
    if (len < 0) {
        return OSDP_PD_ERR_GENERIC;
    }

    /* flush rx to remove any invalid data. */
    if (pd->channel.flush) {
        pd->channel.flush(pd->channel.data);
    }

    ret = pd->channel.send(pd->channel.data, pd->rx_buf, len);
    if (ret != len) {
        OSDP_LOG_ERROR("osdp: pd: Channel send for %d bytes failed! ret: %d\n", len, ret);
        return OSDP_PD_ERR_GENERIC;
    }

    if (MYNEWT_VAL(OSDP_PACKET_TRACE)) {
        if (pd->cmd_id != CMD_POLL) {
            osdp_dump(pd->rx_buf, pd->rx_buf_len,
          "OSDP: PD[%d]: Sent\n", pd->address);
        }
    }

    return OSDP_PD_ERR_NONE;
}

static int
pd_receive_packet(struct osdp_pd *pd)
{
    uint8_t *buf;
    int len, err, remaining;

    len = pd->channel.recv(pd->channel.data, pd->rx_buf + pd->rx_buf_len,
          sizeof(pd->rx_buf) - pd->rx_buf_len);
    if (len > 0) {
        pd->rx_buf_len += len;
    }

    if (MYNEWT_VAL(OSDP_PACKET_TRACE)) {
        /**
         * A crude way of identifying and not printing poll messages
         * when OSDP_PACKET_TRACE is enabled. This is an early
         * print to catch errors so keeping it simple.
         * OSDP_CMD_ID_OFFSET + 2 is also checked as the CMD_ID can be
         * pushed back by 2 bytes if secure channel block is present in
         * header.
         */
        if (pd->rx_buf_len > MYNEWT_VAL(OSDP_CMD_ID_OFFSET) + 2 &&
            pd->rx_buf[MYNEWT_VAL(OSDP_CMD_ID_OFFSET)] != CMD_POLL &&
            pd->rx_buf[MYNEWT_VAL(OSDP_CMD_ID_OFFSET) + 2] != CMD_POLL) {
            osdp_dump(pd->rx_buf, pd->rx_buf_len,
          "OSDP: PD[%d]: Received\n", pd->address);
        }
    }

    err = osdp_phy_check_packet(pd, pd->rx_buf, pd->rx_buf_len, &len);
    if (err == OSDP_ERR_PKT_WAIT) {
        /* rx_buf_len < pkt->len; wait for more data */
        return OSDP_PD_ERR_NO_DATA;
    }
    if (err == OSDP_ERR_PKT_FMT) {
        return OSDP_PD_ERR_GENERIC;
    }
    if (err == OSDP_ERR_PKT_SKIP) {
        err = OSDP_PD_ERR_IGNORE;
    }
    if (err == OSDP_ERR_PKT_NONE) {
        pd->reply_id = 0; /* reset past reply ID so phy can send NAK */
        pd->ephemeral_data[0] = 0; /* reset past NAK reason */
        len = osdp_phy_decode_packet(pd, pd->rx_buf, len, &buf);
        if (len <= 0) {
            if (pd->reply_id != 0) {
                return OSDP_PD_ERR_REPLY; /* Send a NAK */
            }
            return OSDP_PD_ERR_GENERIC; /* fatal errors */
        }
        err = pd_decode_command(pd, buf, len);
    }

    /* We are done with the packet (error or not). Remove processed bytes */
    remaining = pd->rx_buf_len - len;
    if (remaining) {
        memmove(pd->rx_buf, pd->rx_buf + len, remaining);
    }
    /**
     * Store remaining length that needs to be processed.
     * State machine will be updated accordingly.
     */
    pd->rx_buf_len = remaining;

    return err;
}

static void
osdp_update(struct osdp_pd *pd)
{
    int ret;

    switch (pd->state) {
    case OSDP_PD_STATE_IDLE:
        if (ISSET_FLAG(pd, PD_FLAG_SC_ACTIVE) &&
            osdp_millis_since(pd->sc_tstamp) > MYNEWT_VAL(OSDP_PD_SC_TIMEOUT_MS)) {
            OSDP_LOG_INFO("osdp: pd: PD SC session timeout!\n");
            CLEAR_FLAG(pd, PD_FLAG_SC_ACTIVE);
        }
        /* When secure mode is inactive check if CP is polling */
        if (ISSET_FLAG(pd, PD_FLAG_CP_POLL_ACTIVE) &&
            osdp_millis_since(pd->tstamp) > MYNEWT_VAL(OSDP_PD_IDLE_TIMEOUT_MS)) {
            OSDP_LOG_INFO("osdp: pd: PD CP-poll timeout!\n");
            CLEAR_FLAG(pd, PD_FLAG_CP_POLL_ACTIVE);
        }
        ret = pd->channel.recv(pd->channel.data, pd->rx_buf,
              sizeof(pd->rx_buf));
        if (ret <= 0) {
            break;
        }
        pd->rx_buf_len = ret;
        pd->tstamp = osdp_millis_now();
        pd->state = OSDP_PD_STATE_PROCESS_CMD;
        __fallthrough;
    case OSDP_PD_STATE_PROCESS_CMD:
        ret = pd_receive_packet(pd);
        if (ret == OSDP_PD_ERR_NO_DATA &&
            osdp_millis_since(pd->tstamp) < MYNEWT_VAL(OSDP_RESP_TOUT_MS)) {
            break;
        }
        if (ret == OSDP_PD_ERR_IGNORE) {
            /* Process command if non-empty */
            if (pd->rx_buf_len > 0) {
                pd->state = OSDP_PD_STATE_PROCESS_CMD;
            } else {
                pd->state = OSDP_PD_STATE_IDLE;
            }
            break;
        }
        if (ret != OSDP_PD_ERR_NONE && ret != OSDP_PD_ERR_REPLY) {
            OSDP_LOG_ERROR("osdp: pd: CMD receive error/timeout - err:%d\n", ret);
            pd->state = OSDP_PD_STATE_ERR;
            break;
        }
        if (ISSET_FLAG(pd, PD_FLAG_SC_ACTIVE) &&
            ret == OSDP_PD_ERR_NONE) {
            pd->sc_tstamp = osdp_millis_now();
        }
        pd->state = OSDP_PD_STATE_SEND_REPLY;
        __fallthrough;
    case OSDP_PD_STATE_SEND_REPLY:
        if (pd_send_reply(pd) == -1) {
            pd->state = OSDP_PD_STATE_ERR;
            break;
        }
        pd->rx_buf_len = 0;
        pd->state = OSDP_PD_STATE_IDLE;
        break;
    case OSDP_PD_STATE_ERR:
        /**
         * PD error state is momentary as it doesn't maintain any state
         * between commands. We just clean up secure channel status and
         * go back to idle state.
         */
        CLEAR_FLAG(pd, PD_FLAG_SC_ACTIVE);
        if (pd->channel.flush) {
            pd->channel.flush(pd->channel.data);
        }
        pd->state = OSDP_PD_STATE_IDLE;
        break;
    }
}

static void
osdp_pd_set_attributes(struct osdp_pd *pd, struct osdp_pd_cap *cap,
                       struct osdp_pd_id *id)
{
    int fc;

    while (cap && ((fc = cap->function_code) > 0)) {
        if (fc >= OSDP_PD_CAP_SENTINEL) {
            break;
        }
        pd->cap[fc].function_code = cap->function_code;
        pd->cap[fc].compliance_level = cap->compliance_level;
        pd->cap[fc].num_items = cap->num_items;
        cap++;
    }
    if (id != NULL) {
        memcpy(&pd->id, id, sizeof(struct osdp_pd_id));
    }
}

osdp_t *
osdp_pd_setup(struct osdp_ctx *osdp_ctx, osdp_pd_info_t *info, uint8_t *scbk)
{
    struct osdp_pd *pd;
    struct osdp_cp *cp;
    struct osdp *ctx;

    assert(info);

    /*
       osdp_log_ctx_set(info->address);
     */

    ctx = &osdp_ctx->ctx;
    ctx->magic = 0xDEADBEAF;

    ctx->cp = &osdp_ctx->cp_ctx;
    cp = TO_CP(ctx);
    cp->__parent = ctx;
    cp->num_pd = 1;

    ctx->pd = &osdp_ctx->pd_ctx[0];
    SET_CURRENT_PD(ctx, 0);
    pd = TO_PD(ctx, 0);

    pd->__parent = ctx;
    pd->offset = 0;
    pd->baud_rate = info->baud_rate;
    pd->address = info->address;
    pd->flags = info->flags;
    pd->seq_number = -1;
    memcpy(&pd->channel, &info->channel, sizeof(struct osdp_channel));

    if (pd_event_queue_init(pd)) {
        goto error;
    }

    if (scbk == NULL) {
        if (ISSET_FLAG(pd, OSDP_FLAG_ENFORCE_SECURE)) {
            OSDP_LOG_ERROR("osdp: pd: SCBK must be provided in ENFORCE_SECURE\n");
            goto error;
        }
        if (!ISSET_FLAG(pd, OSDP_FLAG_NON_SECURE_MODE)) {
            OSDP_LOG_WARN("osdp: pd: SCBK not provided. PD is in INSTALL_MODE\n");
            SET_FLAG(pd, OSDP_FLAG_INSTALL_MODE);
        } else {
            OSDP_LOG_WARN("osdp: pd: Setting up in non-secure mode\n");
            /* Non secure mode */
        }
    } else {
        memcpy(pd->sc.scbk, scbk, 16);
    }

    /* Set secure capability based on non-secure flag */
    if (!ISSET_FLAG(pd, OSDP_FLAG_NON_SECURE_MODE)) {
        OSDP_LOG_INFO("osdp: pd: PD is SC capable!\n");
        SET_FLAG(pd, PD_FLAG_SC_CAPABLE);
    }

    if (IS_ENABLED(CONFIG_OSDP_SKIP_MARK_BYTE)) {
        SET_FLAG(pd, PD_FLAG_PKT_SKIP_MARK);
    }
    /* Set capabilities based on application */
    osdp_pd_set_attributes(pd, info->cap, &info->id);
    /* Set implicit capabilities */
    osdp_pd_set_attributes(pd, osdp_pd_cap, NULL);

    SET_FLAG(pd, PD_FLAG_PD_MODE); /* used in checks in phy */

    osdp_pd_set_command_callback(ctx, info->pd_cb, NULL);

    OSDP_LOG_INFO("osdp: pd: PD setup complete\n");
    return (osdp_t *) ctx;

error:
    osdp_pd_teardown((osdp_t *) ctx);
    return NULL;
}

/* --- Exported Methods --- */

void
osdp_pd_teardown(osdp_t *ctx)
{
    assert(ctx);

    pd_event_queue_del(TO_PD(ctx, 0));
}

void
osdp_refresh(osdp_t *ctx)
{
    assert(ctx);
    struct osdp_pd *pd = GET_CURRENT_PD(ctx);

    osdp_update(pd);
}

void
osdp_pd_set_capabilities(osdp_t *ctx, struct osdp_pd_cap *cap)
{
    assert(ctx);
    struct osdp_pd *pd = GET_CURRENT_PD(ctx);

    osdp_pd_set_attributes(pd, cap, NULL);
}

void
osdp_pd_set_command_callback(osdp_t *ctx, pd_command_callback_t cb,
                             void *arg)
{
    assert(ctx);
    struct osdp_pd *pd = GET_CURRENT_PD(ctx);

    pd->command_callback_arg = arg;
    pd->command_callback = cb;
}

int
osdp_pd_notify_event(osdp_t *ctx, struct osdp_event *event)
{
    assert(ctx);
    struct osdp_pd *pd = GET_CURRENT_PD(ctx);

    pd_event_put(pd, event);
    return 0;
}

#endif /* OSDP_MODE_PD */
