/*
 * Copyright (c) 2020 Siddharth Chandrasekaran <siddharth@embedjournal.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "modlog/modlog.h"
#include "osdp/osdp_common.h"

#define LOG_TAG "PHY: "
#define OSDP_PKT_MARK                  0xFF
#define OSDP_PKT_SOM                   0x53
#define PKT_CONTROL_SQN                0x03
#define PKT_CONTROL_CRC                0x04
#define PKT_CONTROL_SCB                0x08

struct osdp_packet_header {
    uint8_t som;
    uint8_t pd_address;
    uint8_t len_lsb;
    uint8_t len_msb;
    uint8_t control;
    uint8_t data[];
} __packed;

uint8_t
osdp_compute_checksum(uint8_t *msg, int length)
{
    uint8_t checksum = 0;
    int i, whole_checksum;

    whole_checksum = 0;
    for (i = 0; i < length; i++) {
        whole_checksum += msg[i];
        checksum = ~(0xff & whole_checksum) + 1;
    }
    return checksum;
}

static int
osdp_phy_get_seq_number(struct osdp_pd *pd, int do_inc)
{
    /* pd->seq_num is set to -1 to reset phy cmd state */
    if (do_inc) {
        pd->seq_number += 1;
        if (pd->seq_number > 3) {
            pd->seq_number = 1;
        }
    }
    return pd->seq_number & PKT_CONTROL_SQN;
}

int
osdp_phy_packet_get_data_offset(struct osdp_pd *pd, const uint8_t *buf)
{
    int sb_len = 0, mark_byte_len = 0;
    struct osdp_packet_header *pkt;

    ARG_UNUSED(pd);
    if (ISSET_FLAG(pd, PD_FLAG_PKT_HAS_MARK)) {
        mark_byte_len = 1;
        buf += 1;
    }
    pkt = (struct osdp_packet_header *)buf;
    if (pkt->control & PKT_CONTROL_SCB) {
        sb_len = pkt->data[0];
    }
    return mark_byte_len + sizeof(struct osdp_packet_header) + sb_len;
}

uint8_t *
osdp_phy_packet_get_smb(struct osdp_pd *pd, const uint8_t *buf)
{
    struct osdp_packet_header *pkt;

    ARG_UNUSED(pd);
    if (ISSET_FLAG(pd, PD_FLAG_PKT_HAS_MARK)) {
        buf += 1;
    }
    pkt = (struct osdp_packet_header *)buf;
    if (pkt->control & PKT_CONTROL_SCB) {
        return pkt->data;
    }
    return NULL;
}

int
osdp_phy_in_sc_handshake(int is_reply, int id)
{
    if (is_reply) {
        return (id == REPLY_CCRYPT || id == REPLY_RMAC_I);
    } else {
        return (id == CMD_CHLNG || id == CMD_SCRYPT);
    }
}

int
osdp_phy_packet_init(struct osdp_pd *pd, uint8_t *buf, int max_len)
{
    int exp_len, pd_mode, id, scb_len = 0, mark_byte_len = 0;
    struct osdp_packet_header *pkt;

    pd_mode = ISSET_FLAG(pd, PD_FLAG_PD_MODE);
    exp_len = sizeof(struct osdp_packet_header) + 64; /* 64 is estimated */
    if (max_len < exp_len) {
        OSDP_LOG_ERROR("packet_init: out of space! CMD: %02x\n", pd->cmd_id);
        return OSDP_ERR_PKT_FMT;
    }

    /**
     * In PD mode just follow what we received from CP. In CP mode, as we
     * initiate the transaction, choose based on CONFIG_OSDP_SKIP_MARK_BYTE.
     */
    if ((pd_mode && ISSET_FLAG(pd, PD_FLAG_PKT_HAS_MARK)) ||
        (!pd_mode && !ISSET_FLAG(pd, PD_FLAG_PKT_SKIP_MARK))) {
        buf[0] = OSDP_PKT_MARK;
        buf++;
        mark_byte_len = 1;
        SET_FLAG(pd, PD_FLAG_PKT_HAS_MARK);
    }

    /* Fill packet header */
    pkt = (struct osdp_packet_header *)buf;
    pkt->som = OSDP_PKT_SOM;
    pkt->pd_address = pd->address & 0x7F; /* Use only the lower 7 bits */
    if (pd_mode) {
        /* PD must reply with MSB of it's address set */
        pkt->pd_address |= 0x80;
        id = pd->reply_id;
    } else {
        id = pd->cmd_id;
    }
    pkt->control = osdp_phy_get_seq_number(pd, !pd_mode);
    pkt->control |= PKT_CONTROL_CRC;

    if (ISSET_FLAG(pd, PD_FLAG_SC_ACTIVE)) {
        pkt->control |= PKT_CONTROL_SCB;
        pkt->data[0] = scb_len = 2;
        pkt->data[1] = SCS_15;
    } else if (osdp_phy_in_sc_handshake(pd_mode, id)) {
        pkt->control |= PKT_CONTROL_SCB;
        pkt->data[0] = scb_len = 3;
        pkt->data[1] = SCS_11;
    }

    return mark_byte_len + sizeof(struct osdp_packet_header) + scb_len;
}

int
osdp_phy_packet_finalize(struct osdp_pd *pd, uint8_t *buf,
                         int len, int max_len)
{
    uint8_t *data;
    uint16_t crc16;
    struct osdp_packet_header *pkt;
    int i, is_cmd, data_len;

    is_cmd = !ISSET_FLAG(pd, PD_FLAG_PD_MODE);

    /* Do a sanity check only; we expect expect header to be prefilled */
    if ((unsigned long)len <= sizeof(struct osdp_packet_header)) {
        OSDP_LOG_ERROR("PKT_F: Invalid header\n");
        return OSDP_ERR_PKT_FMT;
    }

    if (ISSET_FLAG(pd, PD_FLAG_PKT_HAS_MARK)) {
        if (buf[0] != OSDP_PKT_MARK) {
            OSDP_LOG_ERROR("PKT_F: MARK validation failed! ID: 0x%02x\n",
          is_cmd ? pd->cmd_id : pd->reply_id);
            return OSDP_ERR_PKT_FMT;
        }
        /* temporarily get rid of mark byte */
        buf += 1;
        len -= 1;
        max_len -= 1;
    }
    pkt = (struct osdp_packet_header *)buf;
    if (pkt->som != OSDP_PKT_SOM) {
        OSDP_LOG_ERROR("PKT_F: header SOM validation failed! ID: 0x%02x\n",
        is_cmd ? pd->cmd_id : pd->reply_id);
        return OSDP_ERR_PKT_FMT;
    }

    /* len: with 2 byte CRC */
    pkt->len_lsb = BYTE_0(len + 2);
    pkt->len_msb = BYTE_1(len + 2);

    if (ISSET_FLAG(pd, PD_FLAG_SC_ACTIVE) &&
        pkt->control & PKT_CONTROL_SCB &&
        pkt->data[1] >= SCS_15) {
        if (pkt->data[1] == SCS_17 || pkt->data[1] == SCS_18) {
            /**
             * Only the data portion of message (after id byte)
             * is encrypted. While (en/de)crypting, we must skip
             * header, security block, and cmd/reply ID byte.
             *
             * Note: if cmd/reply has no data, we must set type to
             * SCS_15/SCS_16 and send them.
             */
            data = pkt->data + pkt->data[0] + 1;
            data_len = len - (sizeof(struct osdp_packet_header)
                              + pkt->data[0] + 1);
            len -= data_len;
            /**
             * check if the passed buffer can hold the encrypted
             * data where length may be rounded up to the nearest
             * 16 byte block bondary.
             */
            if (AES_PAD_LEN(data_len + 1) > max_len) {
                /* data_len + 1 for OSDP_SC_EOM_MARKER */
                goto out_of_space_error;
            }
            len += osdp_encrypt_data(pd, is_cmd, data, data_len);
        }
        /* len: with 4bytes MAC; with 2 byte CRC; without 1 byte mark */
        if (len + 4 > max_len) {
            goto out_of_space_error;
        }

        /* len: with 2 byte CRC; with 4 byte MAC */
        pkt->len_lsb = BYTE_0(len + 2 + 4);
        pkt->len_msb = BYTE_1(len + 2 + 4);

        /* compute and extend the buf with 4 MAC bytes */
        osdp_compute_mac(pd, is_cmd, buf, len);
        data = is_cmd ? pd->sc.c_mac : pd->sc.r_mac;
        for (i = 0; i < 4; i++) {
            buf[len + i] = data[i];
        }
        len += 4;
    }

    /* fill crc16 */
    if (len + 2 > max_len) {
        goto out_of_space_error;
    }
    crc16 = osdp_compute_crc16(buf, len);
    buf[len + 0] = BYTE_0(crc16);
    buf[len + 1] = BYTE_1(crc16);
    len += 2;

    if (ISSET_FLAG(pd, PD_FLAG_PKT_HAS_MARK)) {
        len += 1; /* put back mark byte */
    }

    return len;

out_of_space_error:
    OSDP_LOG_ERROR("PKT_F: Out of buffer space! CMD(%02x)\n", pd->cmd_id);
    return OSDP_ERR_PKT_FMT;
}

int
osdp_phy_check_packet(struct osdp_pd *pd, uint8_t *buf, int len,
                      int *one_pkt_len)
{
    uint16_t comp, cur;
    int pd_addr, pkt_len;
    struct osdp_packet_header *pkt;

    /* wait till we have the header */
    if ((unsigned long)len < sizeof(struct osdp_packet_header)) {
        /* incomplete data */
        return OSDP_ERR_PKT_WAIT;
    }

    CLEAR_FLAG(pd, PD_FLAG_PKT_HAS_MARK);
    if (buf[0] == OSDP_PKT_MARK) {
        buf += 1;
        len -= 1;
        SET_FLAG(pd, PD_FLAG_PKT_HAS_MARK);
    }

    pkt = (struct osdp_packet_header *)buf;

    /* validate packet header */
    if (pkt->som != OSDP_PKT_SOM) {
        OSDP_LOG_ERROR("Invalid SOM 0x%02x\n", pkt->som);
        return OSDP_ERR_PKT_FMT;
    }

    if (!ISSET_FLAG(pd, PD_FLAG_PD_MODE) &&
        !(pkt->pd_address & 0x80)) {
        OSDP_LOG_ERROR("Reply without address MSB set 0x%02x!\n", pkt->pd_address);
        return OSDP_ERR_PKT_FMT;
    }

    /* validate packet length */
    pkt_len = (pkt->len_msb << 8) | pkt->len_lsb;
    if (len < pkt_len) {
        /* wait for more data? */
        return OSDP_ERR_PKT_WAIT;
    }

    *one_pkt_len = pkt_len + (ISSET_FLAG(pd, PD_FLAG_PKT_HAS_MARK) ? 1 : 0);

    /* validate CRC/checksum */
    if (pkt->control & PKT_CONTROL_CRC) {
        pkt_len -= 2; /* consume CRC */
        cur = (buf[pkt_len + 1] << 8) | buf[pkt_len];
        comp = osdp_compute_crc16(buf, pkt_len);
        if (comp != cur) {
            OSDP_LOG_ERROR("Invalid crc 0x%04x/0x%04x\n", comp, cur);
            pd->reply_id = REPLY_NAK;
            pd->ephemeral_data[0] = OSDP_PD_NAK_MSG_CHK;
            return OSDP_ERR_PKT_CHECK;
        }
    } else {
        pkt_len -= 1; /* consume checksum */
        cur = buf[pkt_len];
        comp = osdp_compute_checksum(buf, pkt_len);
        if (comp != cur) {
            OSDP_LOG_ERROR("Invalid checksum %02x/%02x\n", comp, cur);
            pd->reply_id = REPLY_NAK;
            pd->ephemeral_data[0] = OSDP_PD_NAK_MSG_CHK;
            return OSDP_ERR_PKT_CHECK;
        }
    }

    /* validate PD address */
    pd_addr = pkt->pd_address & 0x7F;
    if (pd_addr != pd->address && pd_addr != 0x7F) {
        /* not addressed to us and was not broadcasted */
        if (!ISSET_FLAG(pd, PD_FLAG_PD_MODE)) {
            OSDP_LOG_ERROR("Invalid pd address %d\n", pd_addr);
            return OSDP_ERR_PKT_FMT;
        }
        return OSDP_ERR_PKT_SKIP;
    }

    /* validate sequence number */
    comp = pkt->control & PKT_CONTROL_SQN;
    if (ISSET_FLAG(pd, PD_FLAG_PD_MODE)) {
        if (comp == 0) {
            /**
             * CP is trying to restart communication by sending a 0.
             * The current PD implementation does not hold any state
             * between commands so we can just set seq_number to -1
             * (so it gets incremented to 0 with a call to
             * phy_get_seq_number()) and invalidate any established
             * secure channels.
             */
            pd->seq_number = -1;
            CLEAR_FLAG(pd, PD_FLAG_SC_ACTIVE);
        }
        if (comp == pd->seq_number) {
            /**
             * TODO: PD must resend the last response if CP send the
             * same sequence number again.
             */
            OSDP_LOG_ERROR("seq-repeat/reply-resend not supported!\n");
            pd->reply_id = REPLY_NAK;
            pd->ephemeral_data[0] = OSDP_PD_NAK_SEQ_NUM;
            return OSDP_ERR_PKT_FMT;
        }
    } else {
        if (comp == 0) {
            /**
             * Check for receiving a busy reply from the PD which would
             * have a sequence number of 0, come in an unsecured packet
             * of minimum length, and have the reply ID REPLY_BUSY.
             */
            if ((pkt_len == 6) && (pkt->data[0] == REPLY_BUSY)) {
                pd->seq_number -= 1;
                return OSDP_ERR_PKT_BUSY;
            }
        }
    }
    cur = osdp_phy_get_seq_number(pd, ISSET_FLAG(pd, PD_FLAG_PD_MODE));
    if (cur != comp && !ISSET_FLAG(pd, PD_FLAG_SKIP_SEQ_CHECK)) {
        OSDP_LOG_ERROR("packet seq mismatch %d/%d\n", cur, comp);
        pd->reply_id = REPLY_NAK;
        pd->ephemeral_data[0] = OSDP_PD_NAK_SEQ_NUM;
        return OSDP_ERR_PKT_FMT;
    }

    return OSDP_ERR_PKT_NONE;
}

int
osdp_phy_decode_packet(struct osdp_pd *pd, uint8_t *buf, int len,
                       uint8_t **pkt_start)
{
    uint8_t *data, *mac;
    int mac_offset, is_cmd;
    struct osdp_packet_header *pkt;

    if (ISSET_FLAG(pd, PD_FLAG_PKT_HAS_MARK)) {
        /* Consume mark byte */
        buf += 1;
        len -= 1;
    }

    pkt = (struct osdp_packet_header *)buf;
    len -= pkt->control & PKT_CONTROL_CRC ? 2 : 1;
    mac_offset = len - 4;
    data = pkt->data;
    len -= sizeof(struct osdp_packet_header);

    if (pkt->control & PKT_CONTROL_SCB) {
        if (ISSET_FLAG(pd, PD_FLAG_PD_MODE) &&
            !ISSET_FLAG(pd, PD_FLAG_SC_CAPABLE)) {
            OSDP_LOG_ERROR("PD is not SC capable\n");
            pd->reply_id = REPLY_NAK;
            pd->ephemeral_data[0] = OSDP_PD_NAK_SC_UNSUP;
            return OSDP_ERR_PKT_FMT;
        }
        if (pkt->data[1] < SCS_11 || pkt->data[1] > SCS_18) {
            OSDP_LOG_ERROR("Invalid SB Type\n");
            pd->reply_id = REPLY_NAK;
            pd->ephemeral_data[0] = OSDP_PD_NAK_SC_COND;
            return OSDP_ERR_PKT_FMT;
        }
        if (pkt->data[1] == SCS_11 || pkt->data[1] == SCS_13) {
            /**
             * CP signals PD to use SCBKD by setting SB data byte
             * to 0. In CP, PD_FLAG_SC_USE_SCBKD comes from FSM; on
             * PD we extract it from the command itself. But this
             * usage of SCBKD is allowed only when the PD is in
             * install mode (indicated by OSDP_FLAG_INSTALL_MODE).
             */
            if (ISSET_FLAG(pd, OSDP_FLAG_INSTALL_MODE) &&
                pkt->data[2] == 0) {
                SET_FLAG(pd, PD_FLAG_SC_USE_SCBKD);
            }
        }
        data = pkt->data + pkt->data[0];
        len -= pkt->data[0]; /* consume security block */
    } else {
        if (ISSET_FLAG(pd, PD_FLAG_SC_ACTIVE)) {
            OSDP_LOG_ERROR("Received plain-text message in SC\n");
            pd->reply_id = REPLY_NAK;
            pd->ephemeral_data[0] = OSDP_PD_NAK_SC_COND;
            return OSDP_ERR_PKT_FMT;
        }
    }

    if (ISSET_FLAG(pd, PD_FLAG_SC_ACTIVE) &&
        pkt->control & PKT_CONTROL_SCB &&
        pkt->data[1] >= SCS_15) {
        /* validate MAC */
        is_cmd = ISSET_FLAG(pd, PD_FLAG_PD_MODE);
        osdp_compute_mac(pd, is_cmd, buf, mac_offset);
        mac = is_cmd ? pd->sc.c_mac : pd->sc.r_mac;
        if (memcmp(buf + mac_offset, mac, 4) != 0) {
            OSDP_LOG_ERROR("Invalid MAC; discarding SC\n");
            CLEAR_FLAG(pd, PD_FLAG_SC_ACTIVE);
            pd->reply_id = REPLY_NAK;
            pd->ephemeral_data[0] = OSDP_PD_NAK_SC_COND;
            return OSDP_ERR_PKT_FMT;
        }
        len -= 4; /* consume MAC */

        /* decrypt data block */
        if (pkt->data[1] == SCS_17 || pkt->data[1] == SCS_18) {
            /**
             * Only the data portion of message (after id byte)
             * is encrypted. While (en/de)crypting, we must skip
             * header (6), security block (2) and cmd/reply id (1)
             * bytes.
             *
             * At this point, the header and security block is
             * already consumed. So we can just skip the cmd/reply
             * ID (data[0])  when calling osdp_decrypt_data().
             */
            len = osdp_decrypt_data(pd, is_cmd, data + 1, len - 1);
            if (len < 0) {
                OSDP_LOG_ERROR("Failed at decrypt; discarding SC\n");
                CLEAR_FLAG(pd, PD_FLAG_SC_ACTIVE);
                pd->reply_id = REPLY_NAK;
                pd->ephemeral_data[0] = OSDP_PD_NAK_SC_COND;
                return OSDP_ERR_PKT_FMT;
            }
            if (len == 0) {
                /**
                 * If cmd/reply has no data, PD "should" have
                 * used SCS_15/SCS_16 but we will be tolerant
                 * towards those faulty implementations.
                 */
                OSDP_LOG_INFO("Received encrypted data block with 0 "
                    "length; tolerating non-conformance!\n");
            }
            len += 1; /* put back cmd/reply ID */
        }
    }

    *pkt_start = data;
    return len;
}

void
osdp_phy_state_reset(struct osdp_pd *pd)
{
    pd->phy_state = 0;
    pd->seq_number = -1;
    pd->rx_buf_len = 0;
}
