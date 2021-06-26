/*
 * Copyright (c) 2020 Siddharth Chandrasekaran <siddharth@embedjournal.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>

#include "modlog/modlog.h"

#include "osdp/osdp_common.h"

#if MYNEWT_VAL(OSDP_MODE_CP) /* compile flag based on mode */

#define TAG "CP: "

#define OSDP_PD_POLL_TIMEOUT_MS        (1000 / MYNEWT_VAL(OSDP_PD_POLL_RATE))
#define OSDP_CMD_RETRY_WAIT_MS         (MYNEWT_VAL(OSDP_CMD_RETRY_WAIT_SEC) * 1000)
#define OSDP_PD_SC_RETRY_MS            (MYNEWT_VAL(OSDP_SC_RETRY_WAIT_SEC) * 1000)
#define OSDP_ONLINE_RETRY_WAIT_MAX_MS  (MYNEWT_VAL(OSDP_ONLINE_RETRY_WAIT_MAX_SEC) * 1000)

#define CMD_POLL_LEN                   1
#define CMD_LSTAT_LEN                  1
#define CMD_ISTAT_LEN                  1
#define CMD_OSTAT_LEN                  1
#define CMD_RSTAT_LEN                  1
#define CMD_ID_LEN                     2
#define CMD_CAP_LEN                    2
#define CMD_DIAG_LEN                   2
#define CMD_OUT_LEN                    5
#define CMD_LED_LEN                    15
#define CMD_BUZ_LEN                    6
#define CMD_TEXT_LEN                   7   /* variable length command */
#define CMD_COMSET_LEN                 6
#define CMD_MFG_LEN                    4   /* variable length command */
#define CMD_KEYSET_LEN                 19
#define CMD_CHLNG_LEN                  9
#define CMD_SCRYPT_LEN                 17

#define REPLY_ACK_DATA_LEN             0
#define REPLY_PDID_DATA_LEN            12
#define REPLY_PDCAP_ENTITY_LEN         3
#define REPLY_LSTATR_DATA_LEN          2
#define REPLY_RSTATR_DATA_LEN          1
#define REPLY_COM_DATA_LEN             5
#define REPLY_NAK_DATA_LEN             1
#define REPLY_MFGREP_LEN               4   /* variable length command */
#define REPLY_CCRYPT_DATA_LEN          32
#define REPLY_RMAC_I_DATA_LEN          16
#define REPLY_KEYPPAD_DATA_LEN         2   /* variable length command */
#define REPLY_RAW_DATA_LEN             4   /* variable length command */
#define REPLY_FMT_DATA_LEN             3   /* variable length command */
#define REPLY_BUSY_DATA_LEN            0

#define OSDP_CP_ERR_NONE               0
#define OSDP_CP_ERR_GENERIC           -1
#define OSDP_CP_ERR_EMPTY_Q           -2
#define OSDP_CP_ERR_NO_DATA            1
#define OSDP_CP_ERR_RETRY_CMD          2
#define OSDP_CP_ERR_CAN_YIELD          3
#define OSDP_CP_ERR_INPROG             4

#define POOL_NAME_COMMON "cp_cmd_pool"

/* Enough to hold POOL_NAME_COMMON + digits in 16bit number */
static char pool_names[MYNEWT_VAL(OSDP_PD_COMMAND_QUEUE_SIZE)][sizeof(POOL_NAME_COMMON) + U16_STR_SZ - 1];

static int
cp_cmd_queue_init(struct osdp_pd *pd, uint16_t num)
{
    int rc;
    char buf[U16_STR_SZ] = {0};

    char *num_ptr = u16_to_str(num, buf);
    memcpy(pool_names[num], POOL_NAME_COMMON, sizeof(POOL_NAME_COMMON));
    strcat(pool_names[num], num_ptr);

    rc = os_mempool_init(&pd->cmd.pool,
          MYNEWT_VAL(OSDP_PD_COMMAND_QUEUE_SIZE),
          sizeof(struct cp_cmd_node),
          pd->cmd.pool_buf, pool_names[num]);

    if (rc != OS_OK) {
        OSDP_LOG_ERROR("osdp: cp: Failed to initialize command pool\n");
        return rc;
    }

    pd->cmd.queue.tqh_first = NULL;
    pd->cmd.queue.tqh_last = &pd->cmd.queue.tqh_first;

    os_mutex_init(&pd->lock);

    return rc;
}

static struct osdp_cmd *
cp_cmd_alloc(struct osdp_pd *pd)
{
    struct cp_cmd_node *cmd = NULL;

    cmd = os_memblock_get(&pd->cmd.pool);
    if (cmd == NULL) {
        OSDP_LOG_ERROR("osdp: cp: Command pool allocation failed\n");
        return NULL;
    }

    return &cmd->object;
}

static void
cp_cmd_free(struct osdp_pd *pd, struct osdp_cmd *cmd)
{
    struct cp_cmd_node *node;

    node = CONTAINER_OF(cmd, struct cp_cmd_node, object);
    os_memblock_put(&pd->cmd.pool, node);
}

static void
cp_cmd_enqueue(struct osdp_pd *pd, struct osdp_cmd *cmd)
{
    struct cp_cmd_node *node;

    node = CONTAINER_OF(cmd, struct cp_cmd_node, object);
    TAILQ_INSERT_HEAD(&pd->cmd.queue, node, cp_node);
}

static int
cp_cmd_dequeue(struct osdp_pd *pd, struct osdp_cmd **cmd)
{
    struct cp_cmd_node *node;

    node = TAILQ_LAST(&pd->cmd.queue, queue);
    if (node == NULL) {
        return OSDP_CP_ERR_EMPTY_Q;
    }
    TAILQ_REMOVE(&pd->cmd.queue, node, cp_node);

    *cmd = &node->object;
    return 0;
}

static void
cp_flush_command_queue(struct osdp_pd *pd)
{
    struct osdp_cmd *cmd;

    while (cp_cmd_dequeue(pd, &cmd) == 0) {
        cp_cmd_free(pd, cmd);
    }
}

static void
cp_cmd_queue_del(struct osdp_pd *pd)
{
    /* Empty the queue and put back blocks */
    cp_flush_command_queue(pd);

    os_mempool_clear(&pd->cmd.pool);
    os_mempool_unregister(&pd->cmd.pool);
}

static int
cp_cmd_get(struct osdp_pd *pd, struct osdp_cmd **cmd, int *ret)
{
    int rc = 0;

    rc = osdp_device_lock(&pd->lock);
    if (rc) {
        *ret = OSDP_CP_ERR_NONE; /* no error for now, retry later */
        return rc;
    }

    rc = cp_cmd_dequeue(pd, cmd);
    if (rc) {
        *ret = OSDP_CP_ERR_NONE; /* command queue is empty */
        goto err;
    }

    pd->cmd_id = (*cmd)->id;
    memcpy(pd->ephemeral_data, *cmd, sizeof(struct osdp_cmd));
    cp_cmd_free(pd, *cmd);

err:
    osdp_device_unlock(&pd->lock);
    return rc;
}

static int
cp_cmd_put(struct osdp_pd *pd, struct osdp_cmd *in, int cmd_id)
{
    int rc = 0;
    struct osdp_cmd *cmd;

    rc = osdp_device_lock(&pd->lock);
    if (rc) {
        return rc;
    }

    cmd = cp_cmd_alloc(pd);
    if (cmd == NULL) {
        rc = OS_ENOMEM;
        goto err;
    }

    memcpy(cmd, in, sizeof(struct osdp_cmd));
    cmd->id = cmd_id; /* translate to internal */
    cp_cmd_enqueue(pd, cmd);

err:
    osdp_device_unlock(&pd->lock);
    return rc;
}


static int
cp_channel_acquire(struct osdp_pd *pd, int *owner)
{
    int i;
    struct osdp *ctx = TO_CTX(pd);

    if (ctx->cp->channel_lock[pd->offset] == pd->channel.id) {
        return 0; /* already acquired! by current PD */
    }
    assert(ctx->cp->channel_lock[pd->offset] == 0);
    for (i = 0; i < NUM_PD(ctx); i++) {
        if (ctx->cp->channel_lock[i] == pd->channel.id) {
            /* some other PD has locked this channel */
            if (owner != NULL) {
                *owner = i;
            }
            return -1;
        }
    }
    ctx->cp->channel_lock[pd->offset] = pd->channel.id;

    return 0;
}

static int
cp_channel_release(struct osdp_pd *pd)
{
    struct osdp *ctx = TO_CTX(pd);

    if (ctx->cp->channel_lock[pd->offset] != pd->channel.id) {
        OSDP_LOG_ERROR("osdp: cp: Attempt to release another PD's channel lock\n");
        return -1;
    }
    ctx->cp->channel_lock[pd->offset] = 0;

    return 0;
}

/**
 * Returns:
 * +ve: length of command
 * -ve: error
 */
static int
cp_build_command(struct osdp_pd *pd, uint8_t *buf, int max_len)
{
    uint8_t *smb;
    struct osdp_cmd *cmd = NULL;
    int data_off, i, ret = -1, len = 0;

    data_off = osdp_phy_packet_get_data_offset(pd, buf);
    smb = osdp_phy_packet_get_smb(pd, buf);

    buf += data_off;
    max_len -= data_off;

#define ASSERT_BUF_LEN(need)                                         \
    if (max_len < need) {                                        \
        OSDP_LOG_ERROR("osdp: cp: OOM at build CMD(%02x) - have:%d, need:%d\n", \
        pd->cmd_id, max_len, need);                  \
        return OSDP_CP_ERR_GENERIC;                          \
    }

    switch (pd->cmd_id) {
    case CMD_POLL:
        ASSERT_BUF_LEN(CMD_POLL_LEN);
        buf[len++] = pd->cmd_id;
        ret = 0;
        break;
    case CMD_LSTAT:
        ASSERT_BUF_LEN(CMD_LSTAT_LEN);
        buf[len++] = pd->cmd_id;
        ret = 0;
        break;
    case CMD_ISTAT:
        ASSERT_BUF_LEN(CMD_ISTAT_LEN);
        buf[len++] = pd->cmd_id;
        ret = 0;
        break;
    case CMD_OSTAT:
        ASSERT_BUF_LEN(CMD_OSTAT_LEN);
        buf[len++] = pd->cmd_id;
        ret = 0;
        break;
    case CMD_RSTAT:
        ASSERT_BUF_LEN(CMD_RSTAT_LEN);
        buf[len++] = pd->cmd_id;
        ret = 0;
        break;
    case CMD_ID:
        ASSERT_BUF_LEN(CMD_ID_LEN);
        buf[len++] = pd->cmd_id;
        buf[len++] = 0x00;
        ret = 0;
        break;
    case CMD_CAP:
        ASSERT_BUF_LEN(CMD_CAP_LEN);
        buf[len++] = pd->cmd_id;
        buf[len++] = 0x00;
        ret = 0;
        break;
    case CMD_DIAG:
        ASSERT_BUF_LEN(CMD_DIAG_LEN);
        buf[len++] = pd->cmd_id;
        buf[len++] = 0x00;
        ret = 0;
        break;
    case CMD_OUT:
        ASSERT_BUF_LEN(CMD_OUT_LEN);
        cmd = (struct osdp_cmd *)pd->ephemeral_data;
        buf[len++] = pd->cmd_id;
        buf[len++] = cmd->output.output_no;
        buf[len++] = cmd->output.control_code;
        buf[len++] = BYTE_0(cmd->output.timer_count);
        buf[len++] = BYTE_1(cmd->output.timer_count);
        ret = 0;
        break;
    case CMD_LED:
        ASSERT_BUF_LEN(CMD_LED_LEN);
        cmd = (struct osdp_cmd *)pd->ephemeral_data;
        buf[len++] = pd->cmd_id;
        buf[len++] = cmd->led.reader;
        buf[len++] = cmd->led.led_number;

        buf[len++] = cmd->led.temporary.control_code;
        buf[len++] = cmd->led.temporary.on_count;
        buf[len++] = cmd->led.temporary.off_count;
        buf[len++] = cmd->led.temporary.on_color;
        buf[len++] = cmd->led.temporary.off_color;
        buf[len++] = BYTE_0(cmd->led.temporary.timer_count);
        buf[len++] = BYTE_1(cmd->led.temporary.timer_count);

        buf[len++] = cmd->led.permanent.control_code;
        buf[len++] = cmd->led.permanent.on_count;
        buf[len++] = cmd->led.permanent.off_count;
        buf[len++] = cmd->led.permanent.on_color;
        buf[len++] = cmd->led.permanent.off_color;
        ret = 0;
        break;
    case CMD_BUZ:
        ASSERT_BUF_LEN(CMD_BUZ_LEN);
        cmd = (struct osdp_cmd *)pd->ephemeral_data;
        buf[len++] = pd->cmd_id;
        buf[len++] = cmd->buzzer.reader;
        buf[len++] = cmd->buzzer.control_code;
        buf[len++] = cmd->buzzer.on_count;
        buf[len++] = cmd->buzzer.off_count;
        buf[len++] = cmd->buzzer.rep_count;
        ret = 0;
        break;
    case CMD_TEXT:
        cmd = (struct osdp_cmd *)pd->ephemeral_data;
        ASSERT_BUF_LEN(CMD_TEXT_LEN + cmd->text.length);
        buf[len++] = pd->cmd_id;
        buf[len++] = cmd->text.reader;
        buf[len++] = cmd->text.control_code;
        buf[len++] = cmd->text.temp_time;
        buf[len++] = cmd->text.offset_row;
        buf[len++] = cmd->text.offset_col;
        buf[len++] = cmd->text.length;
        for (i = 0; i < cmd->text.length; i++) {
            buf[len++] = cmd->text.data[i];
        }
        ret = 0;
        break;
    case CMD_COMSET:
        ASSERT_BUF_LEN(CMD_COMSET_LEN);
        cmd = (struct osdp_cmd *)pd->ephemeral_data;
        buf[len++] = pd->cmd_id;
        buf[len++] = cmd->comset.address;
        buf[len++] = BYTE_0(cmd->comset.baud_rate);
        buf[len++] = BYTE_1(cmd->comset.baud_rate);
        buf[len++] = BYTE_2(cmd->comset.baud_rate);
        buf[len++] = BYTE_3(cmd->comset.baud_rate);
        ret = 0;
        break;
    case CMD_MFG:
        cmd = (struct osdp_cmd *)pd->ephemeral_data;
        ASSERT_BUF_LEN(CMD_MFG_LEN + cmd->mfg.length);
        buf[len++] = pd->cmd_id;
        buf[len++] = BYTE_0(cmd->mfg.vendor_code);
        buf[len++] = BYTE_1(cmd->mfg.vendor_code);
        buf[len++] = BYTE_2(cmd->mfg.vendor_code);
        buf[len++] = cmd->mfg.command;
        for (i = 0; i < cmd->mfg.length; i++) {
            buf[len++] = cmd->mfg.data[i];
        }
        ret = 0;
        break;
    case CMD_KEYSET:
        if (!ISSET_FLAG(pd, PD_FLAG_SC_ACTIVE)) {
            OSDP_LOG_ERROR("osdp: cp: Can not perform a KEYSET without SC!\n");
            return -1;
        }
        cmd = (struct osdp_cmd *)pd->ephemeral_data;
        ASSERT_BUF_LEN(CMD_KEYSET_LEN);
        buf[len++] = pd->cmd_id;
        buf[len++] = 1; /* key type (1: SCBK) */
        buf[len++] = 16; /* key length in bytes */
        osdp_compute_scbk(pd, cmd->keyset.data, buf + len);
        len += 16;
        ret = 0;
        break;
    case CMD_CHLNG:
        ASSERT_BUF_LEN(CMD_CHLNG_LEN);
        if (smb == NULL) {
            break;
        }
        smb[0] = 3;     /* length */
        smb[1] = SCS_11; /* type */
        smb[2] = ISSET_FLAG(pd, PD_FLAG_SC_USE_SCBKD) ? 0 : 1;
        buf[len++] = pd->cmd_id;
        for (i = 0; i < 8; i++) {
            buf[len++] = pd->sc.cp_random[i];
        }
        ret = 0;
        break;
    case CMD_SCRYPT:
        ASSERT_BUF_LEN(CMD_SCRYPT_LEN);
        if (smb == NULL) {
            break;
        }
        osdp_compute_cp_cryptogram(pd);
        smb[0] = 3;     /* length */
        smb[1] = SCS_13; /* type */
        smb[2] = ISSET_FLAG(pd, PD_FLAG_SC_USE_SCBKD) ? 0 : 1;
        buf[len++] = pd->cmd_id;
        for (i = 0; i < 16; i++) {
            buf[len++] = pd->sc.cp_cryptogram[i];
        }
        ret = 0;
        break;
    default:
        OSDP_LOG_ERROR("osdp: cp: Unknown/Unsupported CMD(%02x)\n", pd->cmd_id);
        return OSDP_CP_ERR_GENERIC;
    }

    if (smb && (smb[1] > SCS_14) && ISSET_FLAG(pd, PD_FLAG_SC_ACTIVE)) {
        /**
         * When SC active and current cmd is not a handshake (<= SCS_14)
         * then we must set SCS type to 17 if this message has data
         * bytes and 15 otherwise.
         */
        smb[0] = 2;
        smb[1] = (len > 1) ? SCS_17 : SCS_15;
    }

    if (ret < 0) {
        OSDP_LOG_ERROR("osdp: cp: Unable to build CMD(%02x)\n", pd->cmd_id);
        return OSDP_CP_ERR_GENERIC;
    }

    return len;
}

static int
cp_decode_response(struct osdp_pd *pd, uint8_t *buf, int len)
{
    uint32_t temp32;
    struct osdp_cp *cp = TO_CTX(pd)->cp;
    int i, ret = OSDP_CP_ERR_GENERIC, pos = 0, t1, t2;
    struct osdp_event event;

    pd->reply_id = buf[pos++];
    len--;  /* consume reply id from the head */

#define ASSERT_LENGTH(got, exp)                                      \
    if (got != exp) {                                            \
        OSDP_LOG_ERROR("osdp: cp: REPLY(%02x) length error! Got:%d, Exp:%d\n",  \
        pd->reply_id, got, exp);                     \
        return OSDP_CP_ERR_GENERIC;                          \
    }

    switch (pd->reply_id) {
    case REPLY_ACK:
        ASSERT_LENGTH(len, REPLY_ACK_DATA_LEN);
        ret = OSDP_CP_ERR_NONE;
        break;
    case REPLY_NAK:
        ASSERT_LENGTH(len, REPLY_NAK_DATA_LEN);
        OSDP_LOG_WARN("osdp: cp: PD replied with NAK(%d) for CMD(%02x)",
          buf[pos], pd->cmd_id);
        ret = OSDP_CP_ERR_NONE;
        break;
    case REPLY_PDID:
        ASSERT_LENGTH(len, REPLY_PDID_DATA_LEN);
        pd->id.vendor_code = buf[pos++];
        pd->id.vendor_code |= buf[pos++] << 8;
        pd->id.vendor_code |= buf[pos++] << 16;

        pd->id.model = buf[pos++];
        pd->id.version = buf[pos++];

        pd->id.serial_number = buf[pos++];
        pd->id.serial_number |= buf[pos++] << 8;
        pd->id.serial_number |= buf[pos++] << 16;
        pd->id.serial_number |= buf[pos++] << 24;

        pd->id.firmware_version = buf[pos++] << 16;
        pd->id.firmware_version |= buf[pos++] << 8;
        pd->id.firmware_version |= buf[pos++];
        ret = OSDP_CP_ERR_NONE;
        break;
    case REPLY_PDCAP:
        if ((len % REPLY_PDCAP_ENTITY_LEN) != 0) {
            OSDP_LOG_ERROR("osdp: cp: PDCAP response length is not a multiple of 3",
            buf[pos], pd->cmd_id);
            return OSDP_CP_ERR_GENERIC;
        }
        while (pos < len) {
            t1 = buf[pos++]; /* func_code */
            if (t1 > OSDP_PD_CAP_SENTINEL) {
                break;
            }
            pd->cap[t1].function_code = t1;
            pd->cap[t1].compliance_level = buf[pos++];
            pd->cap[t1].num_items = buf[pos++];
        }
        /* post-capabilities hooks */
        t2 = OSDP_PD_CAP_COMMUNICATION_SECURITY;
        if (pd->cap[t2].compliance_level & 0x01) {
            SET_FLAG(pd, PD_FLAG_SC_CAPABLE);
        } else {
            CLEAR_FLAG(pd, PD_FLAG_SC_CAPABLE);
        }
        ret = OSDP_CP_ERR_NONE;
        break;
    case REPLY_LSTATR:
        ASSERT_LENGTH(len, REPLY_LSTATR_DATA_LEN);
        if (buf[pos++]) {
            SET_FLAG(pd, PD_FLAG_TAMPER);
        } else {
            CLEAR_FLAG(pd, PD_FLAG_TAMPER);
        }
        if (buf[pos++]) {
            SET_FLAG(pd, PD_FLAG_POWER);
        } else {
            CLEAR_FLAG(pd, PD_FLAG_POWER);
        }
        ret = OSDP_CP_ERR_NONE;
        break;
    case REPLY_RSTATR:
        ASSERT_LENGTH(len, REPLY_RSTATR_DATA_LEN);
        if (buf[pos++]) {
            SET_FLAG(pd, PD_FLAG_R_TAMPER);
        } else {
            CLEAR_FLAG(pd, PD_FLAG_R_TAMPER);
        }
        ret = OSDP_CP_ERR_NONE;
        break;
    case REPLY_COM:
        ASSERT_LENGTH(len, REPLY_COM_DATA_LEN);
        t1 = buf[pos++];
        temp32 = buf[pos++];
        temp32 |= buf[pos++] << 8;
        temp32 |= buf[pos++] << 16;
        temp32 |= buf[pos++] << 24;
        OSDP_LOG_WARN("osdp: cp: COMSET responded with ID:%d Baud:%d\n", t1, temp32);
        pd->address = t1;
        pd->baud_rate = temp32;
        ret = OSDP_CP_ERR_NONE;
        break;
    case REPLY_KEYPPAD:
        if (len < REPLY_KEYPPAD_DATA_LEN || !cp->event_callback) {
            break;
        }
        event.type = OSDP_EVENT_KEYPRESS;
        event.keypress.reader_no = buf[pos++];
        event.keypress.length = buf[pos++];  /* key length */
        if ((len - REPLY_KEYPPAD_DATA_LEN) != event.keypress.length) {
            break;
        }
        for (i = 0; i < event.keypress.length; i++) {
            event.keypress.data[i] = buf[pos + i];
        }
        cp->event_callback(cp->event_callback_arg, pd->offset, &event);
        ret = OSDP_CP_ERR_NONE;
        break;
    case REPLY_RAW:
        if (len < REPLY_RAW_DATA_LEN || !cp->event_callback) {
            break;
        }
        event.type = OSDP_EVENT_CARDREAD;
        event.cardread.reader_no = buf[pos++];
        event.cardread.format = buf[pos++];
        event.cardread.length = buf[pos++];        /* bits LSB */
        event.cardread.length |= buf[pos++] << 8;  /* bits MSB */
        event.cardread.direction = 0;              /* un-specified */
        t1 = (event.cardread.length + 7) / 8;      /* len: bytes */
        if (t1 != (len - REPLY_RAW_DATA_LEN)) {
            break;
        }
        for (i = 0; i < t1; i++) {
            event.cardread.data[i] = buf[pos + i];
        }
        cp->event_callback(cp->event_callback_arg, pd->offset, &event);
        ret = OSDP_CP_ERR_NONE;
        break;
    case REPLY_FMT:
        if (len < REPLY_FMT_DATA_LEN || !cp->event_callback) {
            break;
        }
        event.type = OSDP_EVENT_CARDREAD;
        event.cardread.reader_no = buf[pos++];
        event.cardread.direction = buf[pos++];
        event.cardread.length = buf[pos++];
        event.cardread.format = OSDP_CARD_FMT_ASCII;
        if (event.cardread.length != (len - REPLY_FMT_DATA_LEN) ||
            event.cardread.length > OSDP_EVENT_MAX_DATALEN) {
            break;
        }
        for (i = 0; i < event.cardread.length; i++) {
            event.cardread.data[i] = buf[pos + i];
        }
        cp->event_callback(cp->event_callback_arg, pd->offset, &event);
        ret = OSDP_CP_ERR_NONE;
        break;
    case REPLY_BUSY:
        /* PD busy; signal upper layer to retry command */
        ASSERT_LENGTH(len, REPLY_BUSY_DATA_LEN);
        ret = OSDP_CP_ERR_RETRY_CMD;
        break;
    case REPLY_MFGREP:
        if (len < REPLY_MFGREP_LEN || !cp->event_callback) {
            break;
        }
        event.type = OSDP_EVENT_MFGREP;
        event.mfgrep.vendor_code = buf[pos++];
        event.mfgrep.vendor_code |= buf[pos++] << 8;
        event.mfgrep.vendor_code |= buf[pos++] << 16;
        event.mfgrep.command = buf[pos++];
        event.mfgrep.length = len - REPLY_MFGREP_LEN;
        if (event.mfgrep.length > OSDP_EVENT_MAX_DATALEN) {
            break;
        }
        for (i = 0; i < event.mfgrep.length; i++) {
            event.mfgrep.data[i] = buf[pos + i];
        }
        cp->event_callback(cp->event_callback_arg, pd->offset, &event);
        ret = OSDP_CP_ERR_NONE;
        break;
    case REPLY_CCRYPT:
        ASSERT_LENGTH(len, REPLY_CCRYPT_DATA_LEN);
        for (i = 0; i < 8; i++) {
            pd->sc.pd_client_uid[i] = buf[pos++];
        }
        for (i = 0; i < 8; i++) {
            pd->sc.pd_random[i] = buf[pos++];
        }
        for (i = 0; i < 16; i++) {
            pd->sc.pd_cryptogram[i] = buf[pos++];
        }
        osdp_compute_session_keys(TO_CTX(pd));
        if (osdp_verify_pd_cryptogram(pd) != 0) {
            OSDP_LOG_ERROR("osdp: cp: Failed to verify PD cryptogram\n");
            return OSDP_CP_ERR_GENERIC;
        }
        ret = OSDP_CP_ERR_NONE;
        break;
    case REPLY_RMAC_I:
        ASSERT_LENGTH(len, REPLY_RMAC_I_DATA_LEN);
        for (i = 0; i < 16; i++) {
            pd->sc.r_mac[i] = buf[pos++];
        }
        SET_FLAG(pd, PD_FLAG_SC_ACTIVE);
        ret = OSDP_CP_ERR_NONE;
        break;
    default:
        OSDP_LOG_DEBUG("osdp: cp: Unexpected REPLY(%02x)\n", pd->reply_id);
        return OSDP_CP_ERR_GENERIC;
    }

    if (ret == OSDP_CP_ERR_GENERIC) {
        OSDP_LOG_ERROR("osdp: cp: Format error in REPLY(%02x) for CMD(%02x)",
        pd->reply_id, pd->cmd_id);
        return OSDP_CP_ERR_GENERIC;
    }

    if (pd->cmd_id != CMD_POLL) {
        OSDP_LOG_DEBUG("osdp: cp: CMD(%02x) REPLY(%02x)\n", pd->cmd_id, pd->reply_id);
    }

    return ret;
}

static int
cp_send_command(struct osdp_pd *pd)
{
    int ret, len;

    /* init packet buf with header */
    len = osdp_phy_packet_init(pd, pd->rx_buf, sizeof(pd->rx_buf));
    if (len < 0) {
        return OSDP_CP_ERR_GENERIC;
    }

    /* fill command data */
    ret = cp_build_command(pd, pd->rx_buf, sizeof(pd->rx_buf));
    if (ret < 0) {
        return OSDP_CP_ERR_GENERIC;
    }
    len += ret;

    /* finalize packet */
    len = osdp_phy_packet_finalize(pd, pd->rx_buf, len, sizeof(pd->rx_buf));
    if (len < 0) {
        return OSDP_CP_ERR_GENERIC;
    }

    /* flush rx to remove any invalid data. */
    if (pd->channel.flush) {
        pd->channel.flush(pd->channel.data);
    }

    ret = pd->channel.send(pd->channel.data, pd->rx_buf, len);
    if (ret != len) {
        OSDP_LOG_ERROR("osdp: cp: Channel send for %d bytes failed! ret: %d\n", len, ret);
        return OSDP_CP_ERR_GENERIC;
    }

    if (MYNEWT_VAL(OSDP_PACKET_TRACE)) {
        if (pd->cmd_id != CMD_POLL) {
            osdp_dump(pd->rx_buf, len,
          "OSDP: PD[%d]: Sent\n", pd->offset);
        }
    }

    return OSDP_CP_ERR_NONE;
}

static int
cp_process_reply(struct osdp_pd *pd)
{
    uint8_t *buf;
    int err, len, remaining;

    buf = pd->rx_buf + pd->rx_buf_len;
    remaining = sizeof(pd->rx_buf) - pd->rx_buf_len;

    len = pd->channel.recv(pd->channel.data, buf, remaining);
    if (len <= 0) { /* No data received */
        return OSDP_CP_ERR_NO_DATA;
    }
    pd->rx_buf_len += len;

    if (MYNEWT_VAL(OSDP_PACKET_TRACE)) {
        if (pd->cmd_id != CMD_POLL) {
            osdp_dump(pd->rx_buf, pd->rx_buf_len,
          "OSDP: PD[%d]: Received\n", pd->offset);
        }
    }

    err = osdp_phy_check_packet(pd, pd->rx_buf, pd->rx_buf_len, &len);
    if (err == OSDP_ERR_PKT_WAIT) {
        /* rx_buf_len < pkt->len; wait for more data */
        return OSDP_CP_ERR_NO_DATA;
    }
    if (err == OSDP_ERR_PKT_NONE) {
        /* Valid OSDP packet in buffer */
        len = osdp_phy_decode_packet(pd, pd->rx_buf, len, &buf);
        if (len <= 0) {
            return OSDP_CP_ERR_GENERIC;
        }
        err = cp_decode_response(pd, buf, len);
    }

    /* We are done with the packet (error or not). Remove processed bytes */
    remaining = pd->rx_buf_len - len;
    if (remaining) {
        memmove(pd->rx_buf, pd->rx_buf + len, remaining);
        pd->rx_buf_len = remaining;
    }

    return err;
}

static inline void
cp_set_state(struct osdp_pd *pd, enum osdp_state_e state)
{
    pd->state = state;
    CLEAR_FLAG(pd, PD_FLAG_AWAIT_RESP);
}

static inline void
cp_set_online(struct osdp_pd *pd)
{
    cp_set_state(pd, OSDP_CP_STATE_ONLINE);
    pd->wait_ms = 0;
}

static inline void
cp_set_offline(struct osdp_pd *pd)
{
    CLEAR_FLAG(pd, PD_FLAG_SC_ACTIVE);
    pd->state = OSDP_CP_STATE_OFFLINE;
    pd->tstamp = osdp_millis_now();
    if (pd->wait_ms == 0) {
        pd->wait_ms = 1000; /* retry after 1 second initially */
    } else {
        pd->wait_ms <<= 1;
        if (pd->wait_ms > OSDP_ONLINE_RETRY_WAIT_MAX_MS) {
            pd->wait_ms = OSDP_ONLINE_RETRY_WAIT_MAX_MS;
        }
    }
}

static int
cp_phy_state_update(struct osdp_pd *pd)
{
    int64_t elapsed;
    int rc, ret = OSDP_CP_ERR_CAN_YIELD;
    struct osdp_cmd *cmd = NULL;

    switch (pd->phy_state) {
    case OSDP_CP_PHY_STATE_WAIT:
        elapsed = osdp_millis_since(pd->phy_tstamp);
        if (elapsed < OSDP_CMD_RETRY_WAIT_MS) {
            break;
        }
        pd->phy_state = OSDP_CP_PHY_STATE_SEND_CMD;
        break;
    case OSDP_CP_PHY_STATE_ERR:
        ret = OSDP_CP_ERR_GENERIC;
        break;
    case OSDP_CP_PHY_STATE_IDLE:
        if (cp_cmd_get(pd, &cmd, &ret)) {
            break;
        }
    /* fall-thru */
    case OSDP_CP_PHY_STATE_SEND_CMD:
        if ((cp_send_command(pd)) < 0) {
            OSDP_LOG_ERROR("osdp: cp: Failed to send CMD(%d)\n", pd->cmd_id);
            pd->phy_state = OSDP_CP_PHY_STATE_ERR;
            ret = OSDP_CP_ERR_GENERIC;
            break;
        }
        ret = OSDP_CP_ERR_INPROG;
        pd->phy_state = OSDP_CP_PHY_STATE_REPLY_WAIT;
        pd->rx_buf_len = 0; /* reset buf_len for next use */
        pd->phy_tstamp = osdp_millis_now();
        break;
    case OSDP_CP_PHY_STATE_REPLY_WAIT:
        rc = cp_process_reply(pd);
        if (rc == OSDP_CP_ERR_NONE) {
            pd->phy_state = OSDP_CP_PHY_STATE_IDLE;
            break;
        }
        if (rc == OSDP_CP_ERR_RETRY_CMD) {
            OSDP_LOG_INFO("osdp: cp: PD busy; retry last command\n");
            pd->phy_tstamp = osdp_millis_now();
            pd->phy_state = OSDP_CP_PHY_STATE_WAIT;
            break;
        }
        if (rc == OSDP_CP_ERR_GENERIC ||
            osdp_millis_since(pd->phy_tstamp) > MYNEWT_VAL(OSDP_RESP_TOUT_MS)) {
            if (rc != OSDP_CP_ERR_GENERIC) {
                OSDP_LOG_ERROR("osdp: cp: Response timeout for CMD(%02x)",
              pd->cmd_id);
            }
            pd->rx_buf_len = 0;
            if (pd->channel.flush) {
                pd->channel.flush(pd->channel.data);
            }
            cp_flush_command_queue(pd);
            pd->phy_state = OSDP_CP_PHY_STATE_ERR;
            ret = OSDP_CP_ERR_GENERIC;
            break;
        }
        ret = OSDP_CP_ERR_INPROG;
        break;
    }

    return ret;
}

static int
cp_cmd_dispatcher(struct osdp_pd *pd, int cmd)
{
    int rc;
    struct osdp *ctx = TO_CTX(pd);
    struct osdp_cmd *c;

    if (ISSET_FLAG(pd, PD_FLAG_AWAIT_RESP)) {
        CLEAR_FLAG(pd, PD_FLAG_AWAIT_RESP);
        return OSDP_CP_ERR_NONE; /* nothing to be done here */
    }
    rc = osdp_device_lock(&pd->lock);
    if (rc) {
        return rc;
    }

    c = cp_cmd_alloc(pd);
    if (c == NULL) {
        rc = OS_ENOMEM;
        goto err;
    }

    c->id = cmd;
    switch (cmd) {
    case CMD_KEYSET:
        c->keyset.length = 16;
        memcpy(c->keyset.data, ctx->sc_master_key, 16);
        break;
    }
    cp_cmd_enqueue(pd, c);
    SET_FLAG(pd, PD_FLAG_AWAIT_RESP);
    rc = OSDP_CP_ERR_INPROG;

err:
    osdp_device_unlock(&pd->lock);
    return rc;
}

static int
state_update(struct osdp_pd *pd)
{
    int phy_state, soft_fail;
    struct osdp *ctx = TO_CTX(pd);

    phy_state = cp_phy_state_update(pd);
    if (phy_state == OSDP_CP_ERR_INPROG ||
        phy_state == OSDP_CP_ERR_CAN_YIELD) {
        return phy_state;
    }

    /* Certain states can fail without causing PD offline */
    soft_fail = (pd->state == OSDP_CP_STATE_SC_CHLNG);

    /* phy state error -- cleanup */
    if (pd->state != OSDP_CP_STATE_OFFLINE &&
        phy_state == OSDP_CP_ERR_GENERIC && soft_fail == 0) {
        cp_set_offline(pd);
        return OSDP_CP_ERR_CAN_YIELD;
    }

    /* command queue is empty and last command was successful */

    switch (pd->state) {
    case OSDP_CP_STATE_ONLINE:
        if (ISSET_FLAG(pd, PD_FLAG_SC_ACTIVE) == false &&
            ISSET_FLAG(pd, PD_FLAG_SC_CAPABLE) == true &&
            ISSET_FLAG(ctx, FLAG_SC_DISABLED) == false &&
            osdp_millis_since(pd->sc_tstamp) > OSDP_PD_SC_RETRY_MS) {
            OSDP_LOG_INFO("osdp: cp: Retry SC after retry timeout\n");
            cp_set_state(pd, OSDP_CP_STATE_SC_INIT);
            break;
        }
        if (osdp_millis_since(pd->tstamp) < OSDP_PD_POLL_TIMEOUT_MS) {
            break;
        }
        if (cp_cmd_dispatcher(pd, CMD_POLL) == 0) {
            pd->tstamp = osdp_millis_now();
        }
        break;
    case OSDP_CP_STATE_OFFLINE:
        if (osdp_millis_since(pd->tstamp) > pd->wait_ms) {
            cp_set_state(pd, OSDP_CP_STATE_INIT);
            osdp_phy_state_reset(pd);
        }
        break;
    case OSDP_CP_STATE_INIT:
        cp_set_state(pd, OSDP_CP_STATE_IDREQ);
        __fallthrough;
    case OSDP_CP_STATE_IDREQ:
        if (cp_cmd_dispatcher(pd, CMD_ID) != 0) {
            break;
        }
        if (pd->reply_id != REPLY_PDID) {
            OSDP_LOG_ERROR("osdp: cp: Unexpected REPLY(%02x) for cmd "
            STR(CMD_CAP), pd->reply_id);
            cp_set_offline(pd);
            break;
        }
        cp_set_state(pd, OSDP_CP_STATE_CAPDET);
        __fallthrough;
    case OSDP_CP_STATE_CAPDET:
        if (cp_cmd_dispatcher(pd, CMD_CAP) != 0) {
            break;
        }
        if (pd->reply_id != REPLY_PDCAP) {
            OSDP_LOG_ERROR("osdp: cp: Unexpected REPLY(%02x) for cmd "
            STR(CMD_CAP), pd->reply_id);
            cp_set_offline(pd);
            break;
        }
        if (ISSET_FLAG(pd, PD_FLAG_SC_CAPABLE) &&
            !ISSET_FLAG(ctx, FLAG_SC_DISABLED)) {
            CLEAR_FLAG(pd, PD_FLAG_SC_SCBKD_DONE);
            CLEAR_FLAG(pd, PD_FLAG_SC_USE_SCBKD);
            cp_set_state(pd, OSDP_CP_STATE_SC_INIT);
            break;
        }
        if (ISSET_FLAG(pd, OSDP_FLAG_ENFORCE_SECURE)) {
            OSDP_LOG_INFO("osdp: cp: SC disabled or not capable. Set PD offline due "
            "to ENFORCE_SECURE\n");
            cp_set_offline(pd);
        } else {
            cp_set_online(pd);
        }
        break;
    case OSDP_CP_STATE_SC_INIT:
        osdp_sc_init(pd);
        cp_set_state(pd, OSDP_CP_STATE_SC_CHLNG);
        __fallthrough;
    case OSDP_CP_STATE_SC_CHLNG:
        if (cp_cmd_dispatcher(pd, CMD_CHLNG) != 0) {
            break;
        }
        if (phy_state < 0) {
            if (ISSET_FLAG(pd, OSDP_FLAG_ENFORCE_SECURE)) {
                OSDP_LOG_INFO("osdp: cp: SC Failed. Set PD offline due to "
              "ENFORCE_SECURE\n");
                cp_set_offline(pd);
                break;
            }
            if (ISSET_FLAG(pd, PD_FLAG_SC_SCBKD_DONE)) {
                OSDP_LOG_INFO("osdp: cp: SC Failed. Online without SC\n");
                pd->sc_tstamp = osdp_millis_now();
                cp_set_online(pd);
                break;
            }
            SET_FLAG(pd, PD_FLAG_SC_USE_SCBKD);
            SET_FLAG(pd, PD_FLAG_SC_SCBKD_DONE);
            cp_set_state(pd, OSDP_CP_STATE_SC_INIT);
            pd->phy_state = 0; /* soft reset phy state */
            OSDP_LOG_WARN("osdp: cp: SC Failed. Retry with SCBK-D\n");
            break;
        }
        if (pd->reply_id != REPLY_CCRYPT) {
            if (ISSET_FLAG(pd, OSDP_FLAG_ENFORCE_SECURE)) {
                OSDP_LOG_ERROR("osdp: cp: CHLNG failed. Set PD offline due to "
              "ENFORCE_SECURE\n");
                cp_set_offline(pd);
            } else {
                OSDP_LOG_ERROR("osdp: cp: CHLNG failed. Online without SC\n");
                pd->sc_tstamp = osdp_millis_now();
                osdp_phy_state_reset(pd);
                cp_set_online(pd);
            }
            break;
        }
        cp_set_state(pd, OSDP_CP_STATE_SC_SCRYPT);
        __fallthrough;
    case OSDP_CP_STATE_SC_SCRYPT:
        if (cp_cmd_dispatcher(pd, CMD_SCRYPT) != 0) {
            break;
        }
        if (pd->reply_id != REPLY_RMAC_I) {
            if (ISSET_FLAG(pd, OSDP_FLAG_ENFORCE_SECURE)) {
                OSDP_LOG_ERROR("osdp: cp: SCRYPT failed. Set PD offline due to "
              "ENFORCE_SECURE\n");
                cp_set_offline(pd);
            } else {
                OSDP_LOG_ERROR("osdp: cp: SCRYPT failed. Online without SC\n");
                osdp_phy_state_reset(pd);
                pd->sc_tstamp = osdp_millis_now();
                cp_set_online(pd);
            }
            break;
        }
        if (ISSET_FLAG(pd, PD_FLAG_SC_USE_SCBKD)) {
            OSDP_LOG_WARN("osdp: cp: SC ACtive with SCBK-D. Set SCBK\n");
            cp_set_state(pd, OSDP_CP_STATE_SET_SCBK);
            break;
        }
        OSDP_LOG_INFO("osdp: cp: SC Active\n");
        pd->sc_tstamp = osdp_millis_now();
        cp_set_online(pd);
        break;
    case OSDP_CP_STATE_SET_SCBK:
        if (cp_cmd_dispatcher(pd, CMD_KEYSET) != 0) {
            break;
        }
        if (pd->reply_id == REPLY_NAK) {
            if (ISSET_FLAG(pd, OSDP_FLAG_ENFORCE_SECURE)) {
                OSDP_LOG_ERROR("osdp: cp: Failed to set SCBK; "
              "Set PD offline due to ENFORCE_SECURE\n");
                cp_set_offline(pd);
            } else {
                OSDP_LOG_WARN("osdp: cp: Failed to set SCBK; "
              "Continue with SCBK-D\n");
                cp_set_state(pd, OSDP_CP_STATE_ONLINE);
            }
            break;
        }
        OSDP_LOG_INFO("osdp: cp: SCBK set; restarting SC to verify new SCBK\n");
        CLEAR_FLAG(pd, PD_FLAG_SC_USE_SCBKD);
        CLEAR_FLAG(pd, PD_FLAG_SC_ACTIVE);
        cp_set_state(pd, OSDP_CP_STATE_SC_INIT);
        pd->seq_number = -1;
        break;
    default:
        break;
    }

    return OSDP_CP_ERR_CAN_YIELD;
}

static int
osdp_cp_send_command_keyset(osdp_t *ctx, struct osdp_cmd_keyset *p)
{
    int i, rc;
    struct osdp_pd *pd;

    if (osdp_get_sc_status_mask(ctx) != PD_MASK(ctx)) {
        OSDP_LOG_WARN("osdp: cp: CMD_KEYSET can be sent only when all PDs are "
        "ONLINE and SC_ACTIVE.\n");
        return 1;
    }

    for (i = 0; i < NUM_PD(ctx); i++) {
        pd = TO_PD(ctx, i);
        struct osdp_cmd *cmd;
        cmd = CONTAINER_OF(p, struct osdp_cmd, keyset);
        rc = cp_cmd_put(pd, cmd, CMD_KEYSET);
        if (rc) {
            return rc;
        }
    }

    return 0;
}

osdp_t *
osdp_cp_setup(struct osdp_ctx *osdp_ctx, int num_pd, osdp_pd_info_t *info,
              uint8_t *master_key)
{
    uint16_t i;
    int owner;
    struct osdp_pd *pd;
    struct osdp_cp *cp;
    struct osdp *ctx;

    assert(info);
    assert(num_pd > 0);

    ctx = &osdp_ctx->ctx;
    ctx->magic = 0xDEADBEAF;
    SET_FLAG(ctx, FLAG_CP_MODE);

    if (master_key != NULL) {
        memcpy(ctx->sc_master_key, master_key, 16);
    } else {
        OSDP_LOG_WARN("osdp: cp: Master key not available! SC Disabled.\n");
        SET_FLAG(ctx, FLAG_SC_DISABLED);
    }

    ctx->cp = &osdp_ctx->cp_ctx;
    cp = TO_CP(ctx);
    cp->__parent = ctx;
    cp->channel_lock = &osdp_ctx->ch_locks_ctx[0];

    ctx->pd = &osdp_ctx->pd_ctx[0];
    cp->num_pd = num_pd;

    for (i = 0; i < num_pd; i++) {
        osdp_pd_info_t *p = info + i;
        pd = TO_PD(ctx, i);
        pd->offset = i;
        pd->__parent = ctx;
        pd->baud_rate = p->baud_rate;
        pd->address = p->address;
        pd->flags = p->flags;
        pd->seq_number = -1;
        if (cp_cmd_queue_init(pd, i)) {
            goto error;
        }
        memcpy(&pd->channel, &p->channel, sizeof(struct osdp_channel));
        if (cp_channel_acquire(pd, &owner) == -1) {
            SET_FLAG(TO_PD(ctx, owner), PD_FLAG_CHN_SHARED);
            SET_FLAG(pd, PD_FLAG_CHN_SHARED);
        }
        if (IS_ENABLED(CONFIG_OSDP_SKIP_MARK_BYTE)) {
            SET_FLAG(pd, PD_FLAG_PKT_SKIP_MARK);
        }
        osdp_cp_set_event_callback(ctx, p->cp_cb, NULL);
    }
    memset(cp->channel_lock, 0, sizeof(int) * num_pd);
    SET_CURRENT_PD(ctx, 0);
    OSDP_LOG_INFO("osdp: cp: CP setup complete\n");
    return (osdp_t *) ctx;

error:
    osdp_cp_teardown((osdp_t *)ctx);
    return NULL;
}

void
osdp_cp_teardown(osdp_t *ctx)
{
    int i;

    if (ctx == NULL || TO_CP(ctx) == NULL) {
        return;
    }

    for (i = 0; i < NUM_PD(ctx); i++) {
        cp_cmd_queue_del(TO_PD(ctx, i));
    }
}

void
osdp_refresh(osdp_t *ctx)
{
    int i, rc;
    struct osdp_pd *pd;

    assert(ctx);

    for (i = 0; i < NUM_PD(ctx); i++) {
        SET_CURRENT_PD(ctx, i);
        /*
           osdp_log_ctx_set(i);
         */
        pd = TO_PD(ctx, i);

        if (ISSET_FLAG(pd, PD_FLAG_CHN_SHARED) &&
            cp_channel_acquire(pd, NULL)) {
            /* failed to lock shared channel */
            continue;
        }

        rc = state_update(pd);

        if (ISSET_FLAG(pd, PD_FLAG_CHN_SHARED) &&
            rc == OSDP_CP_ERR_CAN_YIELD) {
            cp_channel_release(pd);
        }
    }
}

/* --- Exported Methods --- */

void
osdp_cp_set_event_callback(osdp_t *ctx, cp_event_callback_t cb, void *arg)
{
    assert(ctx);

    TO_CP(ctx)->event_callback = cb;
    TO_CP(ctx)->event_callback_arg = arg;
}

int
osdp_cp_send_command(osdp_t *ctx, int pd, struct osdp_cmd *p)
{
    assert(ctx);
    int cmd_id;

    if (pd < 0 || pd >= NUM_PD(ctx)) {
        OSDP_LOG_ERROR("osdp: cp: Invalid PD number\n");
        return -1;
    }
    if (TO_PD(ctx, pd)->state != OSDP_CP_STATE_ONLINE) {
        OSDP_LOG_WARN("osdp: cp: PD not online\n");
        return -1;
    }

    switch (p->id) {
    case OSDP_CMD_OUTPUT:
        cmd_id = CMD_OUT;
        break;
    case OSDP_CMD_LED:
        cmd_id = CMD_LED;
        break;
    case OSDP_CMD_BUZZER:
        cmd_id = CMD_BUZ;
        break;
    case OSDP_CMD_TEXT:
        cmd_id = CMD_TEXT;
        break;
    case OSDP_CMD_COMSET:
        cmd_id = CMD_COMSET;
        break;
    case OSDP_CMD_MFG:
        cmd_id = CMD_MFG;
        break;
    case OSDP_CMD_KEYSET:
        OSDP_LOG_INFO("osdp: cp: Master KEYSET is a global command; "
          "all connected PDs will be affected.\n");
        return osdp_cp_send_command_keyset(ctx, &p->keyset);
    default:
        OSDP_LOG_ERROR("osdp: cp: Invalid CMD_ID:%d\n", p->id);
        return -1;
    }

    return cp_cmd_put(TO_PD(ctx, pd), p, cmd_id);
}

#endif /* OSDP_MODE_CP */
