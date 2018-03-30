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

#ifndef COAP_H
#define COAP_H

#include <stddef.h> /* for size_t */
#include <stdint.h>

#include "os/mynewt.h"

#include <stats/stats.h>

#include "oic/port/mynewt/config.h"
#include "oic/messaging/coap/conf.h"
#include "oic/messaging/coap/constants.h"
#include "oic/oc_log.h"
#include "oic/port/oc_connectivity.h"
#include "oic/port/oc_random.h"

#ifdef __cplusplus
extern "C" {
#endif


#ifndef MAX
#define MAX(n, m) (((n) < (m)) ? (m) : (n))
#endif

#ifndef MIN
#define MIN(n, m) (((n) < (m)) ? (n) : (m))
#endif

#ifndef ABS
#define ABS(n) (((n) < 0) ? -(n) : (n))
#endif

#define COAP_MAX_PACKET_SIZE (COAP_MAX_HEADER_SIZE + MAX_PAYLOAD_SIZE)

/* MAX_PAYLOAD_SIZE can be different from 2^x so we need to get next lower 2^x
 * for COAP_MAX_BLOCK_SIZE */
#ifndef COAP_MAX_BLOCK_SIZE
#define COAP_MAX_BLOCK_SIZE                                                    \
  (MAX_PAYLOAD_SIZE < 32                                                       \
     ? 16                                                                      \
     : (MAX_PAYLOAD_SIZE < 64                                                  \
          ? 32                                                                 \
          : (MAX_PAYLOAD_SIZE < 128                                            \
               ? 64                                                            \
               : (MAX_PAYLOAD_SIZE < 256                                       \
                    ? 128                                                      \
                    : (MAX_PAYLOAD_SIZE < 512                                  \
                         ? 256                                                 \
                         : (MAX_PAYLOAD_SIZE < 1024                            \
                              ? 512                                            \
                              : (MAX_PAYLOAD_SIZE < 2048 ? 1024 : 2048)))))))
#endif /* COAP_MAX_BLOCK_SIZE */

/* bitmap for set options */
enum
{
  OPTION_MAP_SIZE = sizeof(uint8_t) * 8
};

#define SET_OPTION(packet, opt)                                                \
  ((packet)->options[opt / OPTION_MAP_SIZE] |= 1 << (opt % OPTION_MAP_SIZE))
#define IS_OPTION(packet, opt)                                                 \
  ((packet)->options[opt / OPTION_MAP_SIZE] & (1 << (opt % OPTION_MAP_SIZE)))

/*
 * For COAP RX, structure stores the offsets and lengths of option fields
 * within the mbuf chain.
 */
struct coap_packet_rx {
    struct os_mbuf *m;

    uint8_t version;
    coap_message_type_t type;
    uint8_t code;
    uint16_t mid;   /* message ID */

    uint8_t token_len;
    uint8_t token[COAP_TOKEN_LEN];

    /* bitmap to check if option is set */
    uint8_t options[COAP_OPTION_SIZE1 / OPTION_MAP_SIZE + 1];

    /* parse options once and store */
    uint16_t content_format;
    uint32_t max_age;
#if 0
    uint8_t etag_len;
    uint8_t etag[COAP_ETAG_LEN];
#endif
#if COAP_PROXY_OPTION_PROCESSING
    uint16_t proxy_uri_len;
    uint16_t proxy_uri_off;
    uint16_t proxy_scheme_len;
    uint16_t proxy_scheme_off;
#endif
    uint16_t uri_host_len;
    uint16_t uri_host_off;
#if 0
    uint16_t loc_path_len;
    uint16_t loc_path_off;
    uint16_t loc_query_len;
    uint16_t loc_query_off;
#endif
    uint16_t uri_port;
    uint16_t uri_path_len;
    uint16_t uri_path_off;
    uint16_t accept;
    int32_t observe;
#if 0
    uint8_t if_match_len;
    uint8_t if_match[COAP_ETAG_LEN];
#endif
    uint32_t block2_num;
    uint8_t block2_more;
    uint16_t block2_size;
    uint32_t block2_offset;
    uint32_t block1_num;
    uint8_t block1_more;
    uint16_t block1_size;
    uint32_t block1_offset;
    uint32_t size2;
    uint32_t size1;
    uint16_t uri_query_len;
    uint16_t uri_query_off;
    uint8_t if_none_match;

    uint16_t payload_off;
    uint16_t payload_len;
};

/*
 * For CoAP TX, store pointers to user memory. All the TLVs need to be known
 * before header construction can begin.
 */
typedef struct coap_packet {
    uint8_t version;
    coap_message_type_t type;
    uint8_t code;
    uint16_t mid;   /* message ID */

    uint8_t token_len;
    uint8_t token[COAP_TOKEN_LEN];

    /* bitmap to check if option is set */
    uint8_t options[COAP_OPTION_SIZE1 / OPTION_MAP_SIZE + 1];

    /* parse options once and store; allows setting options in random order  */
    uint16_t content_format;
    uint32_t max_age;
#if 0
    uint8_t etag_len;
    uint8_t etag[COAP_ETAG_LEN];
#endif
#if COAP_PROXY_OPTION_PROCESSING
    uint16_t proxy_uri_len;
    char *proxy_uri;
    uint16_t proxy_scheme_len;
    char *proxy_scheme;
#endif
    uint16_t uri_host_len;
    char *uri_host;
#if 0
    uint16_t loc_path_len;
    char *loc_path;
    uint16_t loc_query_len;
    char *loc_query;
#endif
    uint16_t uri_port;
    uint16_t uri_path_len;
    char *uri_path;
    int32_t observe;
    uint16_t accept;
#if 0
    uint8_t if_match_len;
    uint8_t if_match[COAP_ETAG_LEN];
#endif
    uint32_t block2_num;
    uint8_t block2_more;
    uint16_t block2_size;
    uint32_t block2_offset;
    uint32_t block1_num;
    uint8_t block1_more;
    uint16_t block1_size;
    uint32_t block1_offset;
    uint32_t size2;
    uint32_t size1;
    uint16_t uri_query_len;
    char *uri_query;
    uint8_t if_none_match;

    uint16_t payload_len;
    uint8_t *payload;
    struct os_mbuf *payload_m;
} coap_packet_t;

/*
 * COAP statistics
 */
STATS_SECT_START(coap_stats)
    STATS_SECT_ENTRY(iframe)
    STATS_SECT_ENTRY(ierr)
    STATS_SECT_ENTRY(itoobig)
    STATS_SECT_ENTRY(ilen)
    STATS_SECT_ENTRY(imem)
    STATS_SECT_ENTRY(oframe)
    STATS_SECT_ENTRY(oerr)
STATS_SECT_END

extern STATS_SECT_DECL(coap_stats) coap_stats;

/* option format serialization (TX) */
#define COAP_SERIALIZE_INT_OPT(pkt, m, number, field, text)             \
    if (IS_OPTION(pkt, number)) {                                       \
        OC_LOG_DEBUG(" %s [%u]\n", text, (unsigned int)pkt->field);     \
        if (coap_append_int_opt(m, number, current_number, pkt->field)) { \
            goto err_mem;                                               \
        }                                                               \
        current_number = number;                                        \
    }
#define COAP_SERIALIZE_BYTE_OPT(pkt, m, number, field, text)            \
    if (IS_OPTION(pkt, number)) {                                       \
        OC_LOG_DEBUG(" %s %u ", text, pkt->field##_len);                \
        OC_LOG_HEX(LOG_LEVEL_DEBUG, pkt->field, pkt->field##_len);      \
        if (coap_append_array_opt(m, number, current_number, pkt->field, \
                                  pkt->field##_len, '\0')) {            \
            goto err_mem;                                               \
        }                                                               \
        current_number = number;                                        \
    }
#define COAP_SERIALIZE_STRING_OPT(pkt, m, number, field, splitter, text) \
    if (IS_OPTION(pkt, number)) {                                       \
        OC_LOG_DEBUG(" %s", text);                                      \
        OC_LOG_STR(LOG_LEVEL_DEBUG, pkt->field, pkt->field##_len);      \
        if (coap_append_array_opt(m, number, current_number,            \
                                  (uint8_t *)pkt->field,                \
                                  pkt->field##_len, splitter)) {        \
            goto err_mem;                                               \
        }                                                               \
        current_number = number;                                        \
    }
#define COAP_SERIALIZE_BLOCK_OPT(pkt, m, number, field, text)           \
    if (IS_OPTION(pkt, number)) {                                       \
        OC_LOG_DEBUG(" %s [%lu%s (%u B/blk)]\n", text,                  \
                     (unsigned long)pkt->field##_num,                   \
                     pkt->field##_more ? "+" : "", pkt->field##_size);  \
        uint32_t block = pkt->field##_num << 4;                         \
        if (pkt->field##_more) {                                        \
            block |= 0x8;                                               \
        }                                                               \
        block |= 0xF & coap_log_2(pkt->field##_size / 16);              \
        OC_LOG_DEBUG(" %s encoded: 0x%lX\n", text,                      \
                     (unsigned long)block);                             \
        if (coap_append_int_opt(m, number, current_number, block)) {    \
            goto err_mem;                                               \
        }                                                               \
        current_number = number;                                        \
    }

/* to store error code and human-readable payload */
extern coap_status_t erbium_status_code;
extern char *coap_error_message;

void coap_init_connection(void);
uint16_t coap_get_mid(void);

uint16_t coap_tcp_msg_size(uint8_t *hdr, int datalen);

void coap_init_message(coap_packet_t *, coap_message_type_t type,
                       uint8_t code, uint16_t mid);
int coap_serialize_message(coap_packet_t *, struct os_mbuf *m);
void coap_send_message(struct os_mbuf *m, int dup);
coap_status_t coap_parse_message(struct coap_packet_rx *request,
                                 struct os_mbuf **mp);

int coap_get_query_variable(coap_packet_t *, const char *name,
                            const char **output);
int coap_get_post_variable(coap_packet_t *, const char *name,
                           const char **output);

/*---------------------------------------------------------------------------*/

int coap_set_status_code(coap_packet_t *, unsigned int code);

int coap_set_token(coap_packet_t *, const uint8_t *token, size_t token_len);

int coap_get_header_content_format(struct coap_packet_rx *,
                                   unsigned int *format);
int coap_set_header_content_format(coap_packet_t *, unsigned int format);

int coap_get_header_accept(struct coap_packet_rx *, unsigned int *accept);
int coap_set_header_accept(coap_packet_t *, unsigned int accept);

int coap_get_header_max_age(struct coap_packet_rx *, uint32_t *age);
int coap_set_header_max_age(coap_packet_t *, uint32_t age);

int coap_get_header_etag(struct coap_packet_rx *, const uint8_t **etag);
int coap_set_header_etag(coap_packet_t *, const uint8_t *etag, size_t etag_len);

int coap_get_header_if_match(struct coap_packet_rx *, const uint8_t **etag);
int coap_set_header_if_match(coap_packet_t *, const uint8_t *etag,
                             size_t etag_len);

int coap_get_header_if_none_match(struct coap_packet_rx *);
int coap_set_header_if_none_match(coap_packet_t *);

int coap_get_header_proxy_uri(struct coap_packet_rx *,
  const char **uri); /* in-place string might not be 0-terminated. */
int coap_set_header_proxy_uri(coap_packet_t *, const char *uri);

int coap_get_header_proxy_scheme(struct coap_packet_rx *,
  const char **scheme); /* in-place string might not be 0-terminated. */
int coap_set_header_proxy_scheme(coap_packet_t *, const char *scheme);

int coap_get_header_uri_host(struct coap_packet_rx *,
  const char **host); /* in-place string might not be 0-terminated. */
int coap_set_header_uri_host(coap_packet_t *, const char *host);

int coap_get_header_uri_path(struct coap_packet_rx *, char *path, int maxlen);
                              /* in-place string might not be 0-terminated. */
int coap_set_header_uri_path(coap_packet_t *, const char *path);

int coap_get_header_uri_query(struct coap_packet_rx *, char *qry, int maxlen);
                              /* in-place string might not be 0-terminated. */
int coap_set_header_uri_query(coap_packet_t *, const char *query);

int coap_get_header_location_path(struct coap_packet_rx *,
  const char **path); /* in-place string might not be 0-terminated. */
int coap_set_header_location_path(coap_packet_t *,
                                  const char *path); /* also splits optional
                                                        query into
                                                        Location-Query option.
                                                        */

int coap_get_header_location_query(struct coap_packet_rx *,
  const char **query); /* in-place string might not be 0-terminated. */
int coap_set_header_location_query(coap_packet_t *, const char *query);

int coap_get_header_observe(struct coap_packet_rx *, uint32_t *observe);
int coap_set_header_observe(coap_packet_t *, uint32_t observe);

int coap_get_header_block2(struct coap_packet_rx *, uint32_t *num,
                           uint8_t *more, uint16_t *size, uint32_t *offset);
int coap_set_header_block2(coap_packet_t *, uint32_t num, uint8_t more,
                           uint16_t size);

int coap_get_header_block1(struct coap_packet_rx *, uint32_t *num,
                           uint8_t *more, uint16_t *size, uint32_t *offset);
int coap_set_header_block1(coap_packet_t *, uint32_t num, uint8_t more,
                           uint16_t size);

int coap_get_header_size2(struct coap_packet_rx *, uint32_t *size);
int coap_set_header_size2(coap_packet_t *, uint32_t size);

int coap_get_header_size1(struct coap_packet_rx *, uint32_t *size);
int coap_set_header_size1(coap_packet_t *, uint32_t size);

int coap_get_payload_copy(struct coap_packet_rx *, uint8_t *payload,
                          int maxlen);
int coap_get_payload(struct coap_packet_rx *pkt, struct os_mbuf **mp,
                     uint16_t *off);
int coap_set_payload(coap_packet_t *, struct os_mbuf *m, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* COAP_H */
