/*
 * Copyright (c) 2016 Intel Corporation
 *
 * Copyright (c) 2013, Institute for Pervasive Computing, ETH Zurich
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <syscfg/syscfg.h>

#include "oic/port/mynewt/config.h"
#include "oic/messaging/coap/coap.h"
#include "oic/messaging/coap/transactions.h"

#include "api/oc_buffer.h"
#ifdef OC_SECURITY
#include "security/oc_dtls.h"
#endif

STATS_SECT_DECL(coap_stats) coap_stats;
STATS_NAME_START(coap_stats)
    STATS_NAME(coap_stats, iframe)
    STATS_NAME(coap_stats, ierr)
    STATS_NAME(coap_stats, itoobig)
    STATS_NAME(coap_stats, ilen)
    STATS_NAME(coap_stats, imem)
    STATS_NAME(coap_stats, oframe)
    STATS_NAME(coap_stats, oerr)
STATS_NAME_END(coap_stats)

/*---------------------------------------------------------------------------*/
/*- Variables ---------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
static uint16_t current_mid = 0;

coap_status_t erbium_status_code = NO_ERROR;
char *coap_error_message = "";
/*---------------------------------------------------------------------------*/
/*- Local helper functions --------------------------------------------------*/
/*---------------------------------------------------------------------------*/
static uint16_t
coap_log_2(uint16_t value)
{
    uint16_t result = 0;

    do {
        value = value >> 1;
        result++;
    } while (value);

    return (result - 1);
}
/*---------------------------------------------------------------------------*/
static uint32_t
coap_parse_int_option(struct os_mbuf *m, uint16_t off, size_t length)
{
    uint8_t bytes[4];
    uint32_t var = 0;
    int i = 0;

    if (length >= 4) {
        return -1;
    }
    if (os_mbuf_copydata(m, off, length, bytes)) {
        return -1;
    }
    while (i < length) {
        var <<= 8;
        var |= bytes[i++];
    }
    return var;
}
/*---------------------------------------------------------------------------*/
static uint8_t
coap_option_nibble(unsigned int value)
{
    if (value < 13) {
        return value;
    } else if (value <= 0xFF + 13) {
        return 13;
    } else {
        return 14;
    }
}
/*---------------------------------------------------------------------------*/

static int
coap_append_opt_hdr(struct os_mbuf *m, unsigned int delta, size_t length)
{
    uint8_t buffer[4];
    size_t written = 0;

    buffer[0] = coap_option_nibble(delta) << 4 | coap_option_nibble(length);

    if (delta > 268) {
        buffer[++written] = ((delta - 269) >> 8) & 0xff;
        buffer[++written] = (delta - 269) & 0xff;
    } else if (delta > 12) {
        buffer[++written] = (delta - 13);
    }

    if (length > 268) {
        buffer[++written] = ((length - 269) >> 8) & 0xff;
        buffer[++written] = (length - 269) & 0xff;
    } else if (length > 12) {
        buffer[++written] = (length - 13);
    }

    return os_mbuf_append(m, buffer, written + 1);
}

/*---------------------------------------------------------------------------*/
static int
coap_append_int_opt(struct os_mbuf *m, unsigned int number,
                    unsigned int current_number, uint32_t value)
{
    size_t i = 0;
    uint8_t buffer[4];
    int rc;

    if (0xFF000000 & value) {
        ++i;
    }
    if (0xFFFF0000 & value) {
        ++i;
    }
    if (0xFFFFFF00 & value) {
        ++i;
    }
    if (0xFFFFFFFF & value) {
        ++i;
    }
    OC_LOG_DEBUG("OPTION %u (delta %u, len %zu)\n",
                 number, number - current_number, i);

    rc = coap_append_opt_hdr(m, number - current_number, i);
    if (rc) {
        return rc;
    }

    i = 0;
    if (0xFF000000 & value) {
        buffer[i++] = (uint8_t)(value >> 24);
    }
    if (0xFFFF0000 & value) {
        buffer[i++] = (uint8_t)(value >> 16);
    }
    if (0xFFFFFF00 & value) {
        buffer[i++] = (uint8_t)(value >> 8);
    }
    if (0xFFFFFFFF & value) {
        buffer[i++] = (uint8_t)(value);
    }
    return os_mbuf_append(m, buffer, i);
}
/*---------------------------------------------------------------------------*/
static int
coap_append_array_opt(struct os_mbuf *m,
                      unsigned int number, unsigned int current_number,
                      uint8_t *array, size_t length, char split_char)
{
    int rc;
    int j;
    uint8_t *part_start = array;
    uint8_t *part_end = NULL;
    size_t blk;

    OC_LOG_DEBUG("ARRAY type %u, len %zu\n", number, length);

    if (split_char != '\0') {
        for (j = 0; j <= length + 1; ++j) {
            if (array[j] == split_char || j == length) {
                part_end = array + j;
                blk = part_end - part_start;

                rc = coap_append_opt_hdr(m, number - current_number, blk);
                if (rc) {
                    return rc;
                }
                rc = os_mbuf_append(m, part_start, blk);
                if (rc) {
                    return rc;
                }

                OC_LOG_DEBUG("OPTION type %u, delta %u, len %zu\n", number,
                    number - current_number, (int)blk);

                ++j; /* skip the splitter */
                current_number = number;
                part_start = array + j;
            }
        } /* for */
    } else {
        rc = coap_append_opt_hdr(m, number - current_number, length);
        if (rc) {
            return rc;
        }
        rc = os_mbuf_append(m, array, length);
        if (rc) {
            return rc;
        }

        OC_LOG_DEBUG("OPTION type %u, delta %u, len %zu\n", number,
            number - current_number, length);
    }

    return 0;
}
/*---------------------------------------------------------------------------*/

static void
coap_merge_multi_option(struct os_mbuf *m, uint16_t *off1, uint16_t *len1,
                        uint16_t off2, uint16_t len2, char separator)
{
    uint8_t tmp[4];
    int i;
    int blk;

    /* merge multiple options */
    if (*len1 > 0) {
        /* dst already contains an option: concatenate */
        os_mbuf_copyinto(m, *off1 + *len1, &separator, 1);
        *len1 += 1;

        for (i = 0; i < len2; i += blk) {
            blk = min(sizeof(tmp), len2 - i);
            os_mbuf_copydata(m, off2 + i, blk, tmp);
            os_mbuf_copyinto(m, *off1 + *len1 + i, tmp, blk);
        }

        *len1 += len2;
    } else {
        /* off1 is empty: set to off2 */
        *off1 = off2;
        *len1 = len2;
    }
}

/*---------------------------------------------------------------------------*/
#if 0
static int
coap_get_variable(const char *buffer, size_t length, const char *name,
		  const char **output)
{
    const char *start = NULL;
    const char *end = NULL;
    const char *value_end = NULL;
    size_t name_len = 0;

    /*initialize the output buffer first */
    *output = 0;

    name_len = strlen(name);
    end = buffer + length;

    for(start = buffer; start + name_len < end; ++start) {
        if ((start == buffer || start[-1] == '&') && start[name_len] == '=' &&
          strncmp(name, start, name_len) == 0) {

            /* Point start to variable value */
            start += name_len + 1;

            /* Point end to the end of the value */
            value_end = (const char *)memchr(start, '&', end - start);
            if(value_end == NULL) {
                value_end = end;
            }
            *output = start;

            return value_end - start;
        }
    }
    return 0;
}
#endif
/*---------------------------------------------------------------------------*/
/*- Internal API ------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
void
coap_init_connection(void)
{
    /* initialize transaction ID */
    current_mid = oc_random_rand();

    (void)stats_init_and_reg(STATS_HDR(coap_stats),
      STATS_SIZE_INIT_PARMS(coap_stats, STATS_SIZE_32),
      STATS_NAME_INIT_PARMS(coap_stats), "coap");
}
/*---------------------------------------------------------------------------*/
uint16_t
coap_get_mid(void)
{
    return ++current_mid;
}
/*---------------------------------------------------------------------------*/
void
coap_init_message(coap_packet_t *pkt, coap_message_type_t type,
                  uint8_t code, uint16_t mid)
{
    /* Important thing */
    memset(pkt, 0, sizeof(coap_packet_t));

    pkt->type = type;
    pkt->code = code;
    pkt->mid = mid;
}

/*---------------------------------------------------------------------------*/

int
coap_serialize_message(coap_packet_t *pkt, struct os_mbuf *m)
{
    struct coap_udp_hdr *cuh;
    struct coap_tcp_hdr0 *cth0;
    struct coap_tcp_hdr8 *cth8;
    struct coap_tcp_hdr16 *cth16;
    struct coap_tcp_hdr32 *cth32;
    unsigned int current_number = 0;
    int tcp_hdr;
    int len, data_len;

    /* Initialize */
    pkt->version = 1;

    OC_LOG_DEBUG("coap_tx: 0x%x\n", (unsigned)m);

    tcp_hdr = oc_endpoint_use_tcp(OC_MBUF_ENDPOINT(m));

    /*
     * Move data pointer, leave enough space to insert coap header and
     * token before options.
     */
    m->om_data += (sizeof(struct coap_tcp_hdr32) + pkt->token_len);

    /* Serialize options */
    current_number = 0;

#if 0
    /* The options must be serialized in the order of their number */
    COAP_SERIALIZE_BYTE_OPT(pkt, m, COAP_OPTION_IF_MATCH, if_match, "If-Match");
    COAP_SERIALIZE_STRING_OPT(pkt, m, COAP_OPTION_URI_HOST, uri_host, '\0',
                                 "Uri-Host");
    COAP_SERIALIZE_BYTE_OPT(pkt, m, COAP_OPTION_ETAG, etag, "ETag");
    COAP_SERIALIZE_INT_OPT(pkt, m, COAP_OPTION_IF_NONE_MATCH,
        content_format - pkt-> content_format /* hack to get a zero field */,
                           "If-None-Match");
#endif
    COAP_SERIALIZE_INT_OPT(pkt, m, COAP_OPTION_OBSERVE, observe, "Observe");
#if 0
    COAP_SERIALIZE_INT_OPT(pkt, m, COAP_OPTION_URI_PORT, uri_port,
                              "Uri-Port");
    COAP_SERIALIZE_STRING_OPT(pkt, m, COAP_OPTION_LOCATION_PATH,
                              location_path, '/', "Location-Path");
#endif
    COAP_SERIALIZE_STRING_OPT(pkt, m, COAP_OPTION_URI_PATH, uri_path, '/',
                              "Uri-Path");

    COAP_SERIALIZE_INT_OPT(pkt, m, COAP_OPTION_CONTENT_FORMAT, content_format,
                           "Content-Format");
#if 0
    COAP_SERIALIZE_INT_OPT(pkt, m, COAP_OPTION_MAX_AGE, max_age, "Max-Age");
#endif
    COAP_SERIALIZE_STRING_OPT(pkt, m, COAP_OPTION_URI_QUERY, uri_query, '&',
                              "Uri-Query");
    COAP_SERIALIZE_INT_OPT(pkt, m, COAP_OPTION_ACCEPT, accept, "Accept");
#if 0
    COAP_SERIALIZE_STRING_OPT(pkt, m, COAP_OPTION_LOCATION_QUERY,
                              location_query, '&', "Location-Query");
#endif
    COAP_SERIALIZE_BLOCK_OPT(pkt, m, COAP_OPTION_BLOCK2, block2, "Block2");
    COAP_SERIALIZE_BLOCK_OPT(pkt, m, COAP_OPTION_BLOCK1, block1, "Block1");
    COAP_SERIALIZE_INT_OPT(pkt, m, COAP_OPTION_SIZE2, size2, "Size2");
#if 0
    COAP_SERIALIZE_STRING_OPT(pkt, m, COAP_OPTION_PROXY_URI, proxy_uri, '\0',
                              "Proxy-Uri");
    COAP_SERIALIZE_STRING_OPT(pkt, m, COAP_OPTION_PROXY_SCHEME, proxy_scheme,
                              '\0', "Proxy-Scheme");
#endif
    COAP_SERIALIZE_INT_OPT(pkt, m, COAP_OPTION_SIZE1, size1, "Size1");

    /* Payload marker */
    if (pkt->payload_len) {
        if (os_mbuf_append(m, "\xff", 1)) {
            goto err_mem;
        }
    }
    data_len = OS_MBUF_PKTLEN(m) + pkt->payload_len;

    /* set header fields */
    if (!tcp_hdr) {
        len = sizeof(struct coap_udp_hdr) + pkt->token_len;
        os_mbuf_prepend(m, len);
        cuh = (struct coap_udp_hdr *)m->om_data;
        cuh->version = pkt->version;
        cuh->type = pkt->type;
        cuh->token_len = pkt->token_len;
        cuh->code = pkt->code;
        cuh->id = htons(pkt->mid);
        memcpy(cuh + 1, pkt->token, pkt->token_len);
    } else {
        if (data_len < 13) {
            len = sizeof(struct coap_tcp_hdr0) + pkt->token_len;
            os_mbuf_prepend(m, len);
            cth0 = (struct coap_tcp_hdr0 *)m->om_data;
            cth0->data_len = data_len;
            cth0->token_len = pkt->token_len;
            cth0->code = pkt->code;
            memcpy(cth0 + 1, pkt->token, pkt->token_len);
        } else if (data_len < 269) {
            len = sizeof(struct coap_tcp_hdr8) + pkt->token_len;
            os_mbuf_prepend(m, len);
            cth8 = (struct coap_tcp_hdr8 *)m->om_data;
            cth8->type = COAP_TCP_TYPE8;
            cth8->token_len = pkt->token_len;
            cth8->data_len = data_len - COAP_TCP_LENGTH8_OFF;
            cth8->code = pkt->code;
            memcpy(cth8 + 1, pkt->token, pkt->token_len);
        } else if (data_len < 65805) {
            len = sizeof(struct coap_tcp_hdr16) + pkt->token_len;
            os_mbuf_prepend(m, len);
            cth16 = (struct coap_tcp_hdr16 *)m->om_data;
            cth16->type = COAP_TCP_TYPE16;
            cth16->token_len = pkt->token_len;
            cth16->data_len = htons(data_len - COAP_TCP_LENGTH16_OFF);
            cth16->code = pkt->code;
            memcpy(cth16 + 1, pkt->token, pkt->token_len);
        } else {
            len = sizeof(struct coap_tcp_hdr32) + pkt->token_len;
            os_mbuf_prepend(m, len);
            cth32 = (struct coap_tcp_hdr32 *)m->om_data;
            cth32->type = COAP_TCP_TYPE32;
            cth32->token_len = pkt->token_len;
            cth32->data_len = htonl(data_len - COAP_TCP_LENGTH32_OFF);
            cth32->code = pkt->code;
            memcpy(cth32 + 1, pkt->token, pkt->token_len);
        }
    }

    if (pkt->payload_m) {
        assert(pkt->payload_len <= OS_MBUF_PKTLEN(pkt->payload_m));
        if (pkt->payload_len < OS_MBUF_PKTLEN(pkt->payload_m)) {
            os_mbuf_adj(pkt->payload_m,
                        OS_MBUF_PKTLEN(pkt->payload_m) - pkt->payload_len);
        }
        os_mbuf_concat(m, pkt->payload_m);
    }
    OC_LOG_DEBUG("coap_tx: serialized %u B (header len %u, payload len %u)\n",
        OS_MBUF_PKTLEN(m), OS_MBUF_PKTLEN(m) - pkt->payload_len,
        pkt->payload_len);

    return 0;
err_mem:
    if (pkt->payload_m) {
        os_mbuf_free_chain(pkt->payload_m);
    }
    STATS_INC(coap_stats, oerr);
    return -1;
}
/*---------------------------------------------------------------------------*/
void
coap_send_message(struct os_mbuf *m, int dup)
{
    OC_LOG_INFO("coap_send_message(): (%u) %s\n", OS_MBUF_PKTLEN(m),
      dup ? "dup" : "");

    STATS_INC(coap_stats, oframe);

    if (dup) {
        m = os_mbuf_dup(m);
        if (!m) {
            STATS_INC(coap_stats, oerr);
            return;
        }
    }
    oc_send_message(m);
}

/*
 * Given COAP message header, return the number of bytes to expect for
 * this frame.
 */
uint16_t
coap_tcp_msg_size(uint8_t *hdr, int datalen)
{
    struct coap_tcp_hdr0 *cth0;
    struct coap_tcp_hdr8 *cth8;
    struct coap_tcp_hdr16 *cth16;
    struct coap_tcp_hdr32 *cth32;

    if (datalen < sizeof(*cth32)) {
        return -1;
    }
    cth0 = (struct coap_tcp_hdr0 *)hdr;
    if (cth0->data_len < COAP_TCP_TYPE8) {
        return cth0->data_len + sizeof(*cth0) + cth0->token_len;
    } else if (cth0->data_len == COAP_TCP_TYPE8) {
        cth8 = (struct coap_tcp_hdr8 *)hdr;
        return cth8->data_len + sizeof(*cth8) + cth8->token_len +
               COAP_TCP_LENGTH8_OFF;
    } else if (cth0->data_len == COAP_TCP_TYPE16) {
        cth16 = (struct coap_tcp_hdr16 *)hdr;
        return ntohs(cth16->data_len) + sizeof(*cth16) + cth16->token_len +
               COAP_TCP_LENGTH16_OFF;
    } else if (cth0->data_len == COAP_TCP_TYPE32) {
        cth32 = (struct coap_tcp_hdr32 *)hdr;
        return ntohl(cth32->data_len) + sizeof(*cth32) + cth32->token_len +
               COAP_TCP_LENGTH32_OFF;
    } else {
        /* never here */
        return -1;
    }
}

/*---------------------------------------------------------------------------*/
coap_status_t
coap_parse_message(struct coap_packet_rx *pkt, struct os_mbuf **mp)
{
    struct os_mbuf *m;
    int is_tcp;
    struct coap_udp_hdr udp;
    union {
        struct coap_tcp_hdr0 c0;
        struct coap_tcp_hdr8 c8;
        struct coap_tcp_hdr16 c16;
        struct coap_tcp_hdr32 c32;
    } cth;
    uint8_t tmp[4];
    uint16_t cur_opt;
    unsigned int opt_num = 0;
    unsigned int opt_delta = 0;
    size_t opt_len = 0;
    uint8_t data_len;
    int rc;

    m = *mp;
    /* initialize packet */
    memset(pkt, 0, sizeof(*pkt));

    STATS_INC(coap_stats, iframe);

    is_tcp = oc_endpoint_use_tcp(OC_MBUF_ENDPOINT(m));

    /*
     * Make sure that the header is in contiguous area of memory.
     */
    opt_len = sizeof(struct coap_tcp_hdr32);
    if (opt_len > OS_MBUF_PKTLEN(m)) {
        opt_len = OS_MBUF_PKTLEN(m);
    }
    if (m->om_len < opt_len) {
        m = os_mbuf_pullup(m, opt_len);
        *mp = m;
        if (!m) {
            STATS_INC(coap_stats, imem);
            return INTERNAL_SERVER_ERROR_5_00;
        }
    }
    pkt->m = m;

    /* parse header fields */
    if (!is_tcp) {
        cur_opt = sizeof(udp);
        rc = os_mbuf_copydata(m, 0, sizeof(udp), &udp);
        if (rc != 0) {
err_short:
            STATS_INC(coap_stats, ilen);
            return BAD_REQUEST_4_00;
        }
        pkt->version = udp.version;
        pkt->type = udp.type;
        pkt->token_len = udp.token_len;
        pkt->code = udp.code;
        pkt->mid = ntohs(udp.id);
        if (pkt->version != 1) {
            coap_error_message = "CoAP version must be 1";
            STATS_INC(coap_stats, ierr);
            return BAD_REQUEST_4_00;
        }
    } else {
        /*
         * We cannot just look at the data length, as token might or might
         * not be present. Need to figure out which header is present
         * programmatically.
         */
        rc = os_mbuf_copydata(m, 0, sizeof(cth.c0), &cth.c0);
        if (rc != 0) {
            goto err_short;
        }
        data_len = cth.c0.data_len;

        if (data_len < 13) {
            cur_opt = sizeof(cth.c0);
            if (m->om_len < cur_opt) {
                goto err_short;
            }
            pkt->token_len = cth.c0.token_len;
            pkt->code = cth.c0.code;
        } else if (data_len == 13) {
            cur_opt = sizeof(cth.c8);
            rc = os_mbuf_copydata(m, 0, sizeof(cth.c8), &cth.c8);
            if (rc != 0) {
                goto err_short;
            }
            pkt->token_len = cth.c8.token_len;
            pkt->code = cth.c8.code;
        } else if (data_len == 14) {
            cur_opt = sizeof(cth.c16);
            rc = os_mbuf_copydata(m, 0, sizeof(cth.c16), &cth.c16);
            if (rc != 0) {
                goto err_short;
            }
            pkt->token_len = cth.c16.token_len;
            pkt->code = cth.c16.code;
        } else {
            cur_opt = sizeof(cth.c32);
            rc = os_mbuf_copydata(m, 0, sizeof(cth.c32), &cth.c32);
            if (rc != 0) {
                goto err_short;
            }
            pkt->token_len = cth.c32.token_len;
            pkt->code = cth.c32.code;
        }
    }
    if (pkt->token_len > COAP_TOKEN_LEN) {
        coap_error_message = "Token Length must not be more than 8";
        STATS_INC(coap_stats, ierr);
        return BAD_REQUEST_4_00;
    }

    if (os_mbuf_copydata(m, cur_opt, pkt->token_len, pkt->token)) {
        goto err_short;
    }
    cur_opt += pkt->token_len;

    OC_LOG_DEBUG("Token (len %u) ", pkt->token_len);
    OC_LOG_HEX(LOG_LEVEL_DEBUG, pkt->token, pkt->token_len);

    /* parse options */
    memset(pkt->options, 0, sizeof(pkt->options));

    while (cur_opt < OS_MBUF_PKTLEN(m)) {
        /* payload marker 0xFF, currently only checking for 0xF* because rest is
         * reserved */
        if (os_mbuf_copydata(m, cur_opt, 1, tmp)) {
            goto err_short;
        }
        if ((tmp[0] & 0xF0) == 0xF0) {
            pkt->payload_off = ++cur_opt;
            pkt->payload_len = OS_MBUF_PKTLEN(m) - cur_opt;

            /* also for receiving, the Erbium upper bound is MAX_PAYLOAD_SIZE */
            if (pkt->payload_len > MAX_PAYLOAD_SIZE) {
                pkt->payload_len = MAX_PAYLOAD_SIZE;
            }
            break;
        }

        opt_delta = tmp[0] >> 4;
        opt_len = tmp[0] & 0x0F;
        ++cur_opt;

        if (opt_delta == 13) {
            if (os_mbuf_copydata(m, cur_opt, 1, tmp)) {
                goto err_short;
            }
            opt_delta += tmp[0];
            ++cur_opt;
        } else if (opt_delta == 14) {
            if (os_mbuf_copydata(m, cur_opt, 2, tmp)) {
                goto err_short;
            }
            opt_delta += (255 + (tmp[0] << 8) + tmp[1]);
            cur_opt += 2;
        }

        if (opt_len == 13) {
            if (os_mbuf_copydata(m, cur_opt, 1, tmp)) {
                goto err_short;
            }
            opt_len += tmp[0];
            ++cur_opt;
        } else if (opt_len == 14) {
            if (os_mbuf_copydata(m, cur_opt, 2, tmp)) {
                goto err_short;
            }
            opt_len += (255 + (tmp[0] << 8) + tmp[1]);
            cur_opt += 2;
        }

        opt_num += opt_delta;

        if (opt_num < COAP_OPTION_SIZE1) {
            OC_LOG_DEBUG("OPTION %u (delta %u, len %zu): ",
                         opt_num, opt_delta, opt_len);
            SET_OPTION(pkt, opt_num);
        }

        switch (opt_num) {
        case COAP_OPTION_CONTENT_FORMAT:
            pkt->content_format = coap_parse_int_option(m, cur_opt, opt_len);
            OC_LOG_DEBUG("Content-Format [%u]\n", pkt->content_format);
            break;
        case COAP_OPTION_MAX_AGE:
            pkt->max_age = coap_parse_int_option(m, cur_opt, opt_len);
            OC_LOG_DEBUG("Max-Age [%lu]\n", (unsigned long)pkt->max_age);
            break;
#if 0
        case COAP_OPTION_ETAG:
            pkt->etag_len = MIN(COAP_ETAG_LEN, opt_len);
            if (os_mbuf_copydata(m, cur_opt, pkt->etag_len, pkt->etag)) {
                goto err_short;
            }
            OC_LOG_DEBUG("ETag %u ");
            OC_LOG_HEX(LOG_LEVEL_DEBUG, pkt->etag, pkt->etag_len);
            break;
#endif
        case COAP_OPTION_ACCEPT:
            pkt->accept = coap_parse_int_option(m, cur_opt, opt_len);
            OC_LOG_DEBUG("Accept [%u]\n", pkt->accept);
            break;
#if 0
        case COAP_OPTION_IF_MATCH:
            /* TODO support multiple ETags */
            pkt->if_match_len = MIN(COAP_ETAG_LEN, opt_len);
            if (os_mbuf_copydata(m, cur_opt, pkt->if_match_len,
                                 pkt->if_match)) {
                goto err_short;
            }
            OC_LOG_DEBUG("If-Match %u ");
            OC_LOG_HEX(LOG_LEVEL_DEBUG, pkt->if_match, pkt->if_match_len);
            break;
        case COAP_OPTION_IF_NONE_MATCH:
            pkt->if_none_match = 1;
            OC_LOG_DEBUG("If-None-Match\n");
            break;

        case COAP_OPTION_PROXY_URI:
#if COAP_PROXY_OPTION_PROCESSING
            pkt->proxy_uri_off = cur_opt;
            pkt->proxy_uri_len = opt_len;
#endif
            OC_LOG_DEBUG("Proxy-Uri NOT IMPLEMENTED\n");
            coap_error_message =
              "This is a constrained server (MyNewt)";
            STATS_INC(coap_stats, ierr);
            return PROXYING_NOT_SUPPORTED_5_05;
            break;
        case COAP_OPTION_PROXY_SCHEME:
#if COAP_PROXY_OPTION_PROCESSING
            pkt->proxy_scheme_off = cur_opt;
            pkt->proxy_scheme_len = opt_len;
#endif
            OC_LOG_DEBUG("Proxy-Scheme NOT IMPLEMENTED\n");
            coap_error_message =
              "This is a constrained server (MyNewt)";
            STATS_INC(coap_stats, ierr);
            return PROXYING_NOT_SUPPORTED_5_05;
            break;

        case COAP_OPTION_URI_HOST:
            pkt->uri_host_off = cur_opt;
            pkt->uri_host_len = opt_len;
            OC_LOG_DEBUG("Uri-Host ");
            OC_LOG_STR_MBUF(LOG_LEVEL_DEBUG, m, pkt->uri_host_off,
                            pkt->uri_host_len);
            break;
        case COAP_OPTION_URI_PORT:
            pkt->uri_port = coap_parse_int_option(m, cur_opt, opt_len);
            OC_LOG_DEBUG("Uri-Port [%u]\n", pkt->uri_port);
            break;
#endif
        case COAP_OPTION_URI_PATH:
            /* coap_merge_multi_option() operates in-place on the buf */
            coap_merge_multi_option(m, &pkt->uri_path_off, &pkt->uri_path_len,
                                    cur_opt, opt_len, '/');
            OC_LOG_DEBUG("Uri-Path ");
            OC_LOG_STR_MBUF(LOG_LEVEL_DEBUG, m, pkt->uri_path_off,
                            pkt->uri_path_len);
            break;
        case COAP_OPTION_URI_QUERY:
            /* coap_merge_multi_option() operates in-place on the mbuf */
            coap_merge_multi_option(m, &pkt->uri_query_off, &pkt->uri_query_len,
                                    cur_opt, opt_len, '&');
            OC_LOG_DEBUG("Uri-Query ");
            OC_LOG_STR_MBUF(LOG_LEVEL_DEBUG, m, pkt->uri_query_off,
                            pkt->uri_query_len);
            break;
#if 0
        case COAP_OPTION_LOCATION_PATH:
            /* coap_merge_multi_option() operates in-place on the mbuf */
            coap_merge_multi_option(m, &pkt->loc_path_off, &pkt->loc_path_len,
                                    cur_opt, opt_len, '/');
            OC_LOG_DEBUG("Location-Path ");
            OC_LOG_STR_MBUF(LOG_LEVEL_DEBUG, m, pkt->loc_path,
                            pkt->loc_path_len);
            break;
        case COAP_OPTION_LOCATION_QUERY:
            /* coap_merge_multi_option() operates in-place on the mbuf */
            coap_merge_multi_option(m, &pkt->loc_query_off, &pkt->loc_query_len,
                                    cur_opt, opt_len, '&');
            OC_LOG_DEBUG("Location-Query ");
            OC_LOG_STR_MBUF(LOG_LEVEL_DEBUG, m, pkt->loc_query_off,
                            pkt->loc_query_len);
            break;
#endif
        case COAP_OPTION_OBSERVE:
            pkt->observe = coap_parse_int_option(m, cur_opt, opt_len);
            OC_LOG_DEBUG("Observe [%lu]\n", (unsigned long)pkt->observe);
            break;
        case COAP_OPTION_BLOCK2:
            pkt->block2_num = coap_parse_int_option(m, cur_opt, opt_len);
            pkt->block2_more = (pkt->block2_num & 0x08) >> 3;
            pkt->block2_size = 16 << (pkt->block2_num & 0x07);
            pkt->block2_offset =
              (pkt->block2_num & ~0x0000000F) << (pkt->block2_num & 0x07);
            pkt->block2_num >>= 4;
            OC_LOG_DEBUG("Block2 [%lu%s (%u B/blk)]\n",
                         (unsigned long)pkt->block2_num,
                         pkt->block2_more ? "+" : "", pkt->block2_size);
            break;
        case COAP_OPTION_BLOCK1:
            pkt->block1_num = coap_parse_int_option(m, cur_opt, opt_len);
            pkt->block1_more = (pkt->block1_num & 0x08) >> 3;
            pkt->block1_size = 16 << (pkt->block1_num & 0x07);
            pkt->block1_offset =
              (pkt->block1_num & ~0x0000000F) << (pkt->block1_num & 0x07);
            pkt->block1_num >>= 4;
            OC_LOG_DEBUG("Block1 [%lu%s (%u B/blk)]\n",
                         (unsigned long)pkt->block1_num,
                         pkt->block1_more ? "+" : "", pkt->block1_size);
            break;
        case COAP_OPTION_SIZE2:
            pkt->size2 = coap_parse_int_option(m, cur_opt, opt_len);
            OC_LOG_DEBUG("Size2 [%lu]\n", (unsigned long)pkt->size2);
            break;
        case COAP_OPTION_SIZE1:
            pkt->size1 = coap_parse_int_option(m, cur_opt, opt_len);
            OC_LOG_DEBUG("Size1 [%lu]\n", (unsigned long)pkt->size1);
            break;
        default:
            OC_LOG_DEBUG("unknown (%u)\n", opt_num);
            /* check if critical (odd) */
            if (opt_num & 1) {
                coap_error_message = "Unsupported critical option";
                STATS_INC(coap_stats, ierr);
                return BAD_OPTION_4_02;
            }
        }
        cur_opt += opt_len;
    } /* for */

    return NO_ERROR;
}

#if 0
int
coap_get_query_variable(coap_packet_t *const pkt, const char *name,
  const char **output)
{
    if (IS_OPTION(pkt, COAP_OPTION_URI_QUERY)) {
        return coap_get_variable(pkt->uri_query, pkt->uri_query_len,
                                 name, output);
    }
    return 0;
}
int
coap_get_post_variable(coap_packet_t *pkt, const char *name,
                       const char **output)
{
    if (pkt->payload_len) {
        return coap_get_variable(pkt->payload, pkt->payload_len,
                                 name, output);
    }
    return 0;
}
#endif
/*---------------------------------------------------------------------------*/
int
coap_set_status_code(coap_packet_t *const packet, unsigned int code)
{
    if (code <= 0xFF) {
        packet->code = (uint8_t)code;
        return 1;
    } else {
        return 0;
    }
}
/*---------------------------------------------------------------------------*/
int
coap_set_token(coap_packet_t *pkt, const uint8_t *token, size_t token_len)
{
    pkt->token_len = MIN(COAP_TOKEN_LEN, token_len);
    memcpy(pkt->token, token, pkt->token_len);

    return pkt->token_len;
}
#ifdef OC_CLIENT
int
coap_get_header_content_format(struct coap_packet_rx *pkt, unsigned int *format)
{
    if (!IS_OPTION(pkt, COAP_OPTION_CONTENT_FORMAT)) {
        return 0;
    }
    *format = pkt->content_format;
    return 1;
}
#endif
int
coap_set_header_content_format(coap_packet_t *pkt, unsigned int format)
{
    pkt->content_format = format;
    SET_OPTION(pkt, COAP_OPTION_CONTENT_FORMAT);
    return 1;
}
/*---------------------------------------------------------------------------*/
#if 0
int
coap_get_header_accept(struct coap_packet_rx *pkt, unsigned int *accept)
{
    if (!IS_OPTION(pkt, COAP_OPTION_ACCEPT)) {
        return 0;
    }
    *accept = pkt->accept;
    return 1;
}
#endif

#ifdef OC_CLIENT
int
coap_set_header_accept(coap_packet_t *pkt, unsigned int accept)
{
    pkt->accept = accept;
    SET_OPTION(pkt, COAP_OPTION_ACCEPT);
    return 1;
}
#endif
/*---------------------------------------------------------------------------*/
#if 0
int
coap_get_header_max_age(struct coap_packet_rx *pkt, uint32_t *age)
{
    if (!IS_OPTION(pkt, COAP_OPTION_MAX_AGE)) {
        *age = COAP_DEFAULT_MAX_AGE;
    } else {
        *age = pkt->max_age;
    }
    return 1;
}
#endif
int
coap_set_header_max_age(coap_packet_t *pkt, uint32_t age)
{
    pkt->max_age = age;
    SET_OPTION(pkt, COAP_OPTION_MAX_AGE);
    return 1;
}
/*---------------------------------------------------------------------------*/
#if 0
int
coap_get_header_etag(struct coap_packet_rx *pkt, const uint8_t **etag)
{
    if (!IS_OPTION(pkt, COAP_OPTION_ETAG)) {
        return 0;
    }
    *etag = pkt->etag;
    return pkt->etag_len;
}

int
coap_set_header_etag(coap_packet_t *pkt, const uint8_t *etag,
  size_t etag_len)
{
    pkt->etag_len = MIN(COAP_ETAG_LEN, etag_len);
    memcpy(pkt->etag, etag, pkt->etag_len);

    SET_OPTION(pkt, COAP_OPTION_ETAG);
    return pkt->etag_len;
}
/*---------------------------------------------------------------------------*/
/*FIXME support multiple ETags */

int
coap_get_header_if_match(struct coap_packet_rx *pkt, const uint8_t **etag)
{
    if (!IS_OPTION(pkt, COAP_OPTION_IF_MATCH)) {
        return 0;
    }
    *etag = pkt->if_match;
    return pkt->if_match_len;
}

int
coap_set_header_if_match(coap_packet_t *pkt, const uint8_t *etag,
  size_t etag_len)
{
    pkt->if_match_len = MIN(COAP_ETAG_LEN, etag_len);
    memcpy(pkt->if_match, etag, pkt->if_match_len);

    SET_OPTION(pkt, COAP_OPTION_IF_MATCH);
    return pkt->if_match_len;
}

/*---------------------------------------------------------------------------*/
int
coap_get_header_if_none_match(struct coap_packet_rx *pkt)
{
    return IS_OPTION(pkt, COAP_OPTION_IF_NONE_MATCH) ? 1 : 0;
}

int
coap_set_header_if_none_match(coap_packet_t *pkt)
{
    SET_OPTION(pkt, COAP_OPTION_IF_NONE_MATCH);
    return 1;
}
/*---------------------------------------------------------------------------*/
int
coap_get_header_proxy_uri(struct coap_packet_rx *pkt, const char **uri)
{
    if (!IS_OPTION(pkt, COAP_OPTION_PROXY_URI)) {
        return 0;
    }
    *uri = pkt->proxy_uri;
    return pkt->proxy_uri_len;
}

int
coap_set_header_proxy_uri(coap_packet_t *pkt, const char *uri)
{
    /*TODO Provide alternative that sets Proxy-Scheme and Uri-* options and provide er-coap-conf define */

    pkt->proxy_uri = uri;
    pkt->proxy_uri_len = strlen(uri);

    SET_OPTION(pkt, COAP_OPTION_PROXY_URI);
    return pkt->proxy_uri_len;
}
/*---------------------------------------------------------------------------*/
int
coap_get_header_uri_host(struct coap_packet_rx *pkt, const char **host)
{
    if (!IS_OPTION(pkt, COAP_OPTION_URI_HOST)) {
        return 0;
    }
    *host = pkt->uri_host;
    return pkt->uri_host_len;
}

int
coap_set_header_uri_host(coap_packet_t *pkt, const char *host)
{
    pkt->uri_host = (char *)host;
    pkt->uri_host_len = strlen(host);

    SET_OPTION(pkt, COAP_OPTION_URI_HOST);
    return pkt->uri_host_len;
}
#endif
/*---------------------------------------------------------------------------*/
int
coap_get_header_uri_path(struct coap_packet_rx *pkt, char *path, int maxlen)
{
    if (!IS_OPTION(pkt, COAP_OPTION_URI_PATH)) {
        return 0;
    }
    maxlen = min(maxlen, pkt->uri_path_len);
    os_mbuf_copydata(pkt->m, pkt->uri_path_off, maxlen, path);
    return maxlen;
}
#ifdef OC_CLIENT
int
coap_set_header_uri_path(coap_packet_t *pkt, const char *path)
{
    while (path[0] == '/') {
        ++path;
    }
    pkt->uri_path = (char *)path;
    pkt->uri_path_len = strlen(path);

    SET_OPTION(pkt, COAP_OPTION_URI_PATH);
    return pkt->uri_path_len;
}
#endif
/*---------------------------------------------------------------------------*/
int
coap_get_header_uri_query(struct coap_packet_rx *pkt, char *query, int maxlen)
{
    if (!IS_OPTION(pkt, COAP_OPTION_URI_QUERY)) {
        return 0;
    }
    maxlen = min(maxlen, pkt->uri_query_len);
    os_mbuf_copydata(pkt->m, pkt->uri_query_off, maxlen, query);
    return maxlen;
}
#ifdef OC_CLIENT
int
coap_set_header_uri_query(coap_packet_t *pkt, const char *query)
{
    while (query[0] == '?') {
        ++query;
    }
    pkt->uri_query = (char *)query;
    pkt->uri_query_len = strlen(query);

    SET_OPTION(pkt, COAP_OPTION_URI_QUERY);
    return pkt->uri_query_len;
}
#endif
/*---------------------------------------------------------------------------*/
#if 0
int
coap_get_header_location_path(struct coap_packet_rx *pkt, const char **path)
{
    if (!IS_OPTION(pkt, COAP_OPTION_LOCATION_PATH)) {
        return 0;
    }
    *path = pkt->location_path;
    return pkt->location_path_len;
}

int
coap_set_header_location_path(coap_packet_t *pkt, const char *path)
{
    char *query;

    while(path[0] == '/') {
        ++path;
    }
    if ((query = strchr(path, '?'))) {
        coap_set_header_location_query(packet, query + 1);
        pkt->location_path_len = query - path;
    } else {
        pkt->location_path_len = strlen(path);
    }
    pkt->location_path = path;

    if (pkt->location_path_len > 0) {
        SET_OPTION(pkt, COAP_OPTION_LOCATION_PATH);
    }
    return pkt->location_path_len;
}

#if 0
/*---------------------------------------------------------------------------*/
int
coap_get_header_location_query(struct coap_packet_rx *pkt, const char **query)
{
    if (!IS_OPTION(pkt, COAP_OPTION_LOCATION_QUERY)) {
        return 0;
    }
    *query = pkt->location_query;
    return pkt->location_query_len;
}
#endif
int
coap_set_header_location_query(coap_packet_t *pkt, const char *query)
{
    while (query[0] == '?') {
        ++query;
    }
    pkt->loc_query = query;
    pkt->loc_query_len = strlen(query);

    SET_OPTION(pkt, COAP_OPTION_LOCATION_QUERY);
    return pkt->loc_query_len;
}
#endif
/*---------------------------------------------------------------------------*/
int
coap_get_header_observe(struct coap_packet_rx *pkt, uint32_t *observe)
{
    if (!IS_OPTION(pkt, COAP_OPTION_OBSERVE)) {
        return 0;
    }
    *observe = pkt->observe;
    return 1;
}

int
coap_set_header_observe(coap_packet_t *pkt, uint32_t observe)
{
    pkt->observe = observe;
    SET_OPTION(pkt, COAP_OPTION_OBSERVE);
    return 1;
}

/*---------------------------------------------------------------------------*/
int
coap_get_header_block2(struct coap_packet_rx *pkt, uint32_t *num,
                       uint8_t *more, uint16_t *size, uint32_t *offset)
{
    if (!IS_OPTION(pkt, COAP_OPTION_BLOCK2)) {
        return 0;
    }
    /* pointers may be NULL to get only specific block parameters */
    if (num != NULL) {
        *num = pkt->block2_num;
    }
    if (more != NULL) {
        *more = pkt->block2_more;
    }
    if (size != NULL) {
        *size = pkt->block2_size;
    }
    if (offset != NULL) {
        *offset = pkt->block2_offset;
    }
    return 1;
}

int
coap_set_header_block2(coap_packet_t *pkt, uint32_t num,
                       uint8_t more, uint16_t size)
{
    if (size < 16) {
        return 0;
    }
    if (size > 2048) {
        return 0;
    }
    if (num > 0x0FFFFF) {
        return 0;
    }
    pkt->block2_num = num;
    pkt->block2_more = more ? 1 : 0;
    pkt->block2_size = size;

    SET_OPTION(pkt, COAP_OPTION_BLOCK2);
    return 1;
}
/*---------------------------------------------------------------------------*/
int
coap_get_header_block1(struct coap_packet_rx *pkt, uint32_t *num, uint8_t *more,
                       uint16_t *size, uint32_t *offset)
{
    if (!IS_OPTION(pkt, COAP_OPTION_BLOCK1)) {
        return 0;
    }
    /* pointers may be NULL to get only specific block parameters */
    if (num != NULL) {
        *num = pkt->block1_num;
    }
    if (more != NULL) {
        *more = pkt->block1_more;
    }
    if (size != NULL) {
        *size = pkt->block1_size;
    }
    if (offset != NULL) {
        *offset = pkt->block1_offset;
    }
    return 1;
}

int
coap_set_header_block1(coap_packet_t *pkt, uint32_t num, uint8_t more,
                       uint16_t size)
{
    if (size < 16) {
        return 0;
    }
    if (size > 2048) {
        return 0;
    }
    if (num > 0x0FFFFF) {
        return 0;
    }
    pkt->block1_num = num;
    pkt->block1_more = more;
    pkt->block1_size = size;

    SET_OPTION(pkt, COAP_OPTION_BLOCK1);
    return 1;
}
/*---------------------------------------------------------------------------*/
#if 0
int coap_get_header_size2(struct coap_packet_rx * const pkt, uint32_t *size)
{
    if (!IS_OPTION(pkt, COAP_OPTION_SIZE2)) {
        return 0;
    }
    *size = pkt->size2;
    return 1;
}

int
coap_set_header_size2(coap_packet_t *pkt, uint32_t size)
{
    pkt->size2 = size;
    SET_OPTION(pkt, COAP_OPTION_SIZE2);
    return 1;
}
/*---------------------------------------------------------------------------*/
int
coap_get_header_size1(struct coap_packet_rx *pkt, uint32_t *size)
{
    if (!IS_OPTION(pkt, COAP_OPTION_SIZE1)) {
        return 0;
    }
    *size = pkt->size1;
    return 1;
}
int
coap_set_header_size1(coap_packet_t *pkt, uint32_t size)
{
    pkt->size1 = size;
    SET_OPTION(pkt, COAP_OPTION_SIZE1);
    return 1;
}
#endif
/*---------------------------------------------------------------------------*/
int
coap_get_payload_copy(struct coap_packet_rx *pkt, uint8_t *payload, int maxlen)
{
    if (pkt->payload_len) {
        maxlen = min(maxlen, pkt->payload_len);
        os_mbuf_copydata(pkt->m, pkt->payload_off, maxlen, payload);
        return maxlen;
    } else {
        return 0;
    }
}

int
coap_get_payload(struct coap_packet_rx *pkt, struct os_mbuf **mp, uint16_t *off)
{
    if (pkt->payload_len) {
        *off = pkt->payload_off;
    } else {
        *off = OS_MBUF_PKTLEN(pkt->m);
    }
    *mp = pkt->m;
    return pkt->payload_len;
}

int
coap_set_payload(coap_packet_t *pkt, struct os_mbuf *m, size_t length)
{
    pkt->payload_m = os_mbuf_dup(m);
    if (!pkt->payload_m) {
        return -1;
    }
    pkt->payload_len = MIN(OS_MBUF_PKTLEN(m), length);

    return pkt->payload_len;
}
/*---------------------------------------------------------------------------*/
