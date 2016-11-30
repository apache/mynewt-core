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

#include "coap.h"
#include "transactions.h"

#ifdef OC_SECURITY
#include "security/oc_dtls.h"
#endif

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
coap_parse_int_option(uint8_t *bytes, size_t length)
{
  uint32_t var = 0;
  int i = 0;

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
static size_t
coap_set_option_header(unsigned int delta, size_t length, uint8_t *buffer)
{
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

  LOG("WRITTEN %zu B opt header\n", 1 + written);

  return ++written;
}
/*---------------------------------------------------------------------------*/
static size_t
coap_serialize_int_option(unsigned int number, unsigned int current_number,
                          uint8_t *buffer, uint32_t value)
{
  size_t i = 0;

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
  LOG("OPTION %u (delta %u, len %zu)\n", number, number - current_number, i);

  i = coap_set_option_header(number - current_number, i, buffer);

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
  return i;
}
/*---------------------------------------------------------------------------*/
static size_t
coap_serialize_array_option(unsigned int number, unsigned int current_number,
                            uint8_t *buffer, uint8_t *array, size_t length,
                            char split_char)
{
  size_t i = 0;

  LOG("ARRAY type %u, len %zu, full [%.*s]\n", number, length, (int)length,
      array);

  if (split_char != '\0') {
    int j;
    uint8_t *part_start = array;
    uint8_t *part_end = NULL;
    size_t temp_length;

    for (j = 0; j <= length + 1; ++j) {
      LOG("STEP %u/%zu (%c)\n", j, length, array[j]);
      if (array[j] == split_char || j == length) {
        part_end = array + j;
        temp_length = part_end - part_start;

        i += coap_set_option_header(number - current_number, temp_length,
                                    &buffer[i]);
        memcpy(&buffer[i], part_start, temp_length);
        i += temp_length;

        LOG("OPTION type %u, delta %u, len %zu, part [%.*s]\n", number,
            number - current_number, i, (int)temp_length, part_start);

        ++j; /* skip the splitter */
        current_number = number;
        part_start = array + j;
      }
    } /* for */
  } else {
    i += coap_set_option_header(number - current_number, length, &buffer[i]);
    memcpy(&buffer[i], array, length);
    i += length;

    LOG("OPTION type %u, delta %u, len %zu\n", number, number - current_number,
        length);
  }

  return i;
}
/*---------------------------------------------------------------------------*/
static void
coap_merge_multi_option(char **dst, size_t *dst_len, uint8_t *option,
                        size_t option_len, char separator)
{
  /* merge multiple options */
  if (*dst_len > 0) {
    /* dst already contains an option: concatenate */
    (*dst)[*dst_len] = separator;
    *dst_len += 1;

    /* memmove handles 2-byte option headers */
    memmove((*dst) + (*dst_len), option, option_len);

    *dst_len += option_len;
  } else {
    /* dst is empty: set to option */
    *dst = (char *)option;
    *dst_len = option_len;
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
    if((start == buffer || start[-1] == '&') && start[name_len] == '='
       && strncmp(name, start, name_len) == 0) {

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
}
/*---------------------------------------------------------------------------*/
uint16_t
coap_get_mid(void)
{
    return ++current_mid;
}
/*---------------------------------------------------------------------------*/
void
coap_init_message(coap_packet_t *coap_pkt, coap_message_type_t type,
                  uint8_t code, uint16_t mid)
{
    /* Important thing */
    memset(coap_pkt, 0, sizeof(coap_packet_t));

    coap_pkt->type = type;
    coap_pkt->code = code;
    coap_pkt->mid = mid;
}
/*---------------------------------------------------------------------------*/
size_t
coap_serialize_message(coap_packet_t *pkt, uint8_t *buffer)
{
    uint8_t *option;
    unsigned int current_number = 0;

    /* Initialize */
    pkt->buffer = buffer;
    pkt->version = 1;

    LOG("-Serializing MID %u to %p, ", pkt->mid, pkt->buffer);

    /* set header fields */
    pkt->buffer[0] = 0x00;
    pkt->buffer[0] |= COAP_HEADER_VERSION_MASK &
                         (pkt->version) << COAP_HEADER_VERSION_POSITION;
    pkt->buffer[0] |=
      COAP_HEADER_TYPE_MASK & (pkt->type) << COAP_HEADER_TYPE_POSITION;
    pkt->buffer[0] |= COAP_HEADER_TOKEN_LEN_MASK &
                         (pkt->token_len)
                           << COAP_HEADER_TOKEN_LEN_POSITION;
    pkt->buffer[1] = pkt->code;
    pkt->buffer[2] = (uint8_t)((pkt->mid) >> 8);
    pkt->buffer[3] = (uint8_t)(pkt->mid);

    /* empty packet, dont need to do more stuff */
    if (!pkt->code) {
        LOG("-Done serializing empty message at %p-\n", pkt->buffer);
        return 4;
    }

    /* set Token */
    LOG("Token (len %u)", pkt->token_len);
    option = pkt->buffer + COAP_HEADER_LEN;
    for (current_number = 0; current_number < pkt->token_len;
         ++current_number) {
        LOG(" %02X", pkt->token[current_number]);
        *option = pkt->token[current_number];
        ++option;
    }
    LOG("-\n");

    /* Serialize options */
    current_number = 0;

    LOG("-Serializing options at %p-\n", option);
#if 0
    /* The options must be serialized in the order of their number */
    COAP_SERIALIZE_BYTE_OPTION(pkt, COAP_OPTION_IF_MATCH, if_match, "If-Match");
    COAP_SERIALIZE_STRING_OPTION(pkt, COAP_OPTION_URI_HOST, uri_host, '\0',
                                 "Uri-Host");
    COAP_SERIALIZE_BYTE_OPTION(pkt, COAP_OPTION_ETAG, etag, "ETag");
    COAP_SERIALIZE_INT_OPTION(pkt, COAP_OPTION_IF_NONE_MATCH,
        content_format - pkt-> content_format /* hack to get a zero field */,
                              "If-None-Match");
#endif
    COAP_SERIALIZE_INT_OPTION(pkt, COAP_OPTION_OBSERVE, observe, "Observe");
#if 0
    COAP_SERIALIZE_INT_OPTION(pkt, COAP_OPTION_URI_PORT, uri_port, "Uri-Port");
    COAP_SERIALIZE_STRING_OPTION(pkt, COAP_OPTION_LOCATION_PATH, location_path,
                                 '/', "Location-Path");
#endif
    COAP_SERIALIZE_STRING_OPTION(pkt, COAP_OPTION_URI_PATH, uri_path, '/',
                                 "Uri-Path");
    LOG("Serialize content format: %d\n", pkt->content_format);
    COAP_SERIALIZE_INT_OPTION(pkt, COAP_OPTION_CONTENT_FORMAT, content_format,
                              "Content-Format");
#if 0
    COAP_SERIALIZE_INT_OPTION(pkt, COAP_OPTION_MAX_AGE, max_age, "Max-Age");
#endif
    COAP_SERIALIZE_STRING_OPTION(pkt, COAP_OPTION_URI_QUERY, uri_query, '&',
                                 "Uri-Query");
    COAP_SERIALIZE_INT_OPTION(pkt, COAP_OPTION_ACCEPT, accept, "Accept");
#if 0
    COAP_SERIALIZE_STRING_OPTION(pkt, COAP_OPTION_LOCATION_QUERY,
                                 location_query, '&', "Location-Query");
#endif
    COAP_SERIALIZE_BLOCK_OPTION(pkt, COAP_OPTION_BLOCK2, block2, "Block2");
    COAP_SERIALIZE_BLOCK_OPTION(pkt, COAP_OPTION_BLOCK1, block1, "Block1");
    COAP_SERIALIZE_INT_OPTION(pkt, COAP_OPTION_SIZE2, size2, "Size2");
#if 0
    COAP_SERIALIZE_STRING_OPTION(pkt, COAP_OPTION_PROXY_URI, proxy_uri, '\0',
                                 "Proxy-Uri");
    COAP_SERIALIZE_STRING_OPTION(pkt, COAP_OPTION_PROXY_SCHEME, proxy_scheme,
                                 '\0', "Proxy-Scheme");
#endif
    COAP_SERIALIZE_INT_OPTION(pkt, COAP_OPTION_SIZE1, size1, "Size1");

    LOG("-Done serializing at %p----\n", option);

    /* Pack payload */
    if ((option - pkt->buffer) <= COAP_MAX_HEADER_SIZE) {
        /* Payload marker */
        if (pkt->payload_len) {
            *option = 0xFF;
            ++option;
        }
        memmove(option, pkt->payload, pkt->payload_len);
    } else {
        /* an error occurred: caller must check for !=0 */
        pkt->buffer = NULL;
        coap_error_message = "Serialized header exceeds COAP_MAX_HEADER_SIZE";
        return 0;
    }

    LOG("-Done %u B (header len %u, payload len %u)-\n",
        (unsigned int)(pkt->payload_len + option - buffer),
        (unsigned int)(option - buffer), (unsigned int)pkt->payload_len);

    LOG("Dump [0x%02X %02X %02X %02X  %02X %02X %02X %02X]\n",
        pkt->buffer[0], pkt->buffer[1], pkt->buffer[2], pkt->buffer[3],
        pkt->buffer[4], pkt->buffer[5], pkt->buffer[6], pkt->buffer[7]);
    return (option - buffer) + pkt->payload_len; /* packet length */
}
/*---------------------------------------------------------------------------*/
void
coap_send_message(oc_message_t *message)
{
  LOG("-sending OCF message (%u)-\n", (unsigned int) message->length);

  oc_send_message(message);
}

/*---------------------------------------------------------------------------*/
coap_status_t
coap_parse_message(coap_packet_t *pkt, uint8_t *data, uint16_t data_len)
{
    uint8_t *current_option;
    unsigned int option_number = 0;
    unsigned int option_delta = 0;
    size_t option_length = 0;

    /* initialize packet */
    memset(pkt, 0, sizeof(coap_packet_t));
    /* pointer to packet bytes */
    pkt->buffer = data;

    /* parse header fields */
    pkt->version = (COAP_HEADER_VERSION_MASK & pkt->buffer[0]) >>
                    COAP_HEADER_VERSION_POSITION;
    pkt->type = (COAP_HEADER_TYPE_MASK & pkt->buffer[0]) >>
                 COAP_HEADER_TYPE_POSITION;
    pkt->token_len = (COAP_HEADER_TOKEN_LEN_MASK & pkt->buffer[0]) >>
                      COAP_HEADER_TOKEN_LEN_POSITION;
    pkt->code = pkt->buffer[1];
    pkt->mid = pkt->buffer[2] << 8 | pkt->buffer[3];

    if (pkt->version != 1) {
        coap_error_message = "CoAP version must be 1";
        return BAD_REQUEST_4_00;
    }
    if (pkt->token_len > COAP_TOKEN_LEN) {
        coap_error_message = "Token Length must not be more than 8";
        return BAD_REQUEST_4_00;
    }

    current_option = data + COAP_HEADER_LEN;

    memcpy(pkt->token, current_option, pkt->token_len);
    LOG("Token (len %u) [0x%02X%02X%02X%02X%02X%02X%02X%02X]\n",
      pkt->token_len,
      pkt->token[0], pkt->token[1], pkt->token[2], pkt->token[3], pkt->token[4],
      pkt->token[5], pkt->token[6], pkt->token[7]); /* XXX always prints 8 */

    /* parse options */
    memset(pkt->options, 0, sizeof(pkt->options));
    current_option += pkt->token_len;

    while (current_option < data + data_len) {
        /* payload marker 0xFF, currently only checking for 0xF* because rest is
         * reserved */
        if ((current_option[0] & 0xF0) == 0xF0) {
            pkt->payload = ++current_option;
            pkt->payload_len = data_len - (pkt->payload - data);

            /* also for receiving, the Erbium upper bound is MAX_PAYLOAD_SIZE */
            if (pkt->payload_len > MAX_PAYLOAD_SIZE) {
                pkt->payload_len = MAX_PAYLOAD_SIZE;
                /* null-terminate payload */
            }
            pkt->payload[pkt->payload_len] = '\0';

            break;
        }

        option_delta = current_option[0] >> 4;
        option_length = current_option[0] & 0x0F;
        ++current_option;

        if (option_delta == 13) {
            option_delta += current_option[0];
            ++current_option;
        } else if (option_delta == 14) {
            option_delta += 255;
            option_delta += current_option[0] << 8;
            ++current_option;
            option_delta += current_option[0];
            ++current_option;
        }

        if (option_length == 13) {
            option_length += current_option[0];
            ++current_option;
        } else if (option_length == 14) {
            option_length += 255;
            option_length += current_option[0] << 8;
            ++current_option;
            option_length += current_option[0];
            ++current_option;
        }

        option_number += option_delta;

        if (option_number < COAP_OPTION_SIZE1) {
            LOG("OPTION %u (delta %u, len %zu): ", option_number, option_delta,
              option_length);
            SET_OPTION(pkt, option_number);
        }

        switch (option_number) {
        case COAP_OPTION_CONTENT_FORMAT:
            pkt->content_format =
              coap_parse_int_option(current_option, option_length);
            LOG("Content-Format [%u]\n", pkt->content_format);
            break;
        case COAP_OPTION_MAX_AGE:
            pkt->max_age = coap_parse_int_option(current_option, option_length);
            LOG("Max-Age [%lu]\n", (unsigned long)pkt->max_age);
            break;
#if 0
        case COAP_OPTION_ETAG:
            pkt->etag_len = MIN(COAP_ETAG_LEN, option_length);
            memcpy(pkt->etag, current_option, pkt->etag_len);
            LOG("ETag %u [0x%02X%02X%02X%02X%02X%02X%02X%02X]\n",
              pkt->etag_len,
              pkt->etag[0], pkt->etag[1], pkt->etag[2], pkt->etag[3],
              pkt->etag[4], pkt->etag[5], pkt->etag[6], pkt->etag[7]);
            /*FIXME always prints 8 bytes */
            break;
#endif
        case COAP_OPTION_ACCEPT:
            pkt->accept = coap_parse_int_option(current_option, option_length);
            LOG("Accept [%u]\n", pkt->accept);
            break;
#if 0
        case COAP_OPTION_IF_MATCH:
            /* TODO support multiple ETags */
            pkt->if_match_len = MIN(COAP_ETAG_LEN, option_length);
            memcpy(pkt->if_match, current_option, pkt->if_match_len);
            LOG("If-Match %u [0x%02X%02X%02X%02X%02X%02X%02X%02X]\n",
              pkt->if_match_len, pkt->if_match[0], pkt->if_match[1],
              pkt->if_match[2], pkt->if_match[3], pkt->if_match[4],
              pkt->if_match[5], pkt->if_match[6], pkt->if_match[7]);
            break;
        case COAP_OPTION_IF_NONE_MATCH:
            pkt->if_none_match = 1;
            LOG("If-None-Match\n");
            break;

        case COAP_OPTION_PROXY_URI:
#if COAP_PROXY_OPTION_PROCESSING
            pkt->proxy_uri = (char *)current_option;
            pkt->proxy_uri_len = option_length;
#endif
            LOG("Proxy-Uri NOT IMPLEMENTED [%.*s]\n", (int)pkt->proxy_uri_len,
              pkt->proxy_uri);
            coap_error_message =
              "This is a constrained server (MyNewt)";
            return PROXYING_NOT_SUPPORTED_5_05;
            break;
        case COAP_OPTION_PROXY_SCHEME:
#if COAP_PROXY_OPTION_PROCESSING
            pkt->proxy_scheme = (char *)current_option;
            pkt->proxy_scheme_len = option_length;
#endif
            LOG("Proxy-Scheme NOT IMPLEMENTED [%.*s]\n",
              (int)pkt->proxy_scheme_len, pkt->proxy_scheme);
            coap_error_message =
              "This is a constrained server (Contiki)";
            return PROXYING_NOT_SUPPORTED_5_05;
            break;

        case COAP_OPTION_URI_HOST:
            pkt->uri_host = (char *)current_option;
            pkt->uri_host_len = option_length;
            LOG("Uri-Host [%.*s]\n", (int)pkt->uri_host_len, pkt->uri_host);
            break;
        case COAP_OPTION_URI_PORT:
            pkt->uri_port =
              coap_parse_int_option(current_option, option_length);
            LOG("Uri-Port [%u]\n", pkt->uri_port);
            break;
#endif
        case COAP_OPTION_URI_PATH:
            /* coap_merge_multi_option() operates in-place on the IPBUF, but
             * final packet field should be const string -> cast to string */
            coap_merge_multi_option((char **)&(pkt->uri_path),
                                    &(pkt->uri_path_len), current_option,
                                    option_length, '/');
            LOG("Uri-Path [%.*s]\n", (int)pkt->uri_path_len, pkt->uri_path);
            break;
        case COAP_OPTION_URI_QUERY:
            /* coap_merge_multi_option() operates in-place on the IPBUF, but
             * final packet field should be const string -> cast to string */
            coap_merge_multi_option((char **)&(pkt->uri_query),
                                    &(pkt->uri_query_len), current_option,
                                    option_length, '&');
            LOG("Uri-Query [%.*s]\n", (int)pkt->uri_query_len, pkt->uri_query);
            break;
#if 0
        case COAP_OPTION_LOCATION_PATH:
            /* coap_merge_multi_option() operates in-place on the IPBUF, but
             * final packet field should be const string -> cast to string */
            coap_merge_multi_option((char **)&(pkt->location_path),
                                    &(pkt->location_path_len),
                                    current_option, option_length, '/');
            LOG("Location-Path [%.*s]\n", (int)pkt->location_path_len,
              pkt->location_path);
            break;
        case COAP_OPTION_LOCATION_QUERY:
            /* coap_merge_multi_option() operates in-place on the IPBUF, but
             * final packet field should be const string -> cast to string */
            coap_merge_multi_option((char **)&(pkt->location_query),
                                    &(pkt->location_query_len),
                                    current_option, option_length, '&');
            LOG("Location-Query [%.*s]\n", (int)pkt->location_query_len,
                                           pkt->location_query);
      break;
#endif
        case COAP_OPTION_OBSERVE:
            pkt->observe = coap_parse_int_option(current_option, option_length);
            LOG("Observe [%lu]\n", (unsigned long)pkt->observe);
            break;
        case COAP_OPTION_BLOCK2:
            pkt->block2_num =
              coap_parse_int_option(current_option, option_length);
            pkt->block2_more = (pkt->block2_num & 0x08) >> 3;
            pkt->block2_size = 16 << (pkt->block2_num & 0x07);
            pkt->block2_offset = (pkt->block2_num & ~0x0000000F) <<
                                 (pkt->block2_num & 0x07);
            pkt->block2_num >>= 4;
            LOG("Block2 [%lu%s (%u B/blk)]\n", (unsigned long)pkt->block2_num,
              pkt->block2_more ? "+" : "", pkt->block2_size);
            break;
        case COAP_OPTION_BLOCK1:
            pkt->block1_num =
              coap_parse_int_option(current_option, option_length);
            pkt->block1_more = (pkt->block1_num & 0x08) >> 3;
            pkt->block1_size = 16 << (pkt->block1_num & 0x07);
            pkt->block1_offset = (pkt->block1_num & ~0x0000000F) <<
                                 (pkt->block1_num & 0x07);
            pkt->block1_num >>= 4;
            LOG("Block1 [%lu%s (%u B/blk)]\n", (unsigned long)pkt->block1_num,
              pkt->block1_more ? "+" : "", pkt->block1_size);
            break;
        case COAP_OPTION_SIZE2:
            pkt->size2 = coap_parse_int_option(current_option, option_length);
            LOG("Size2 [%lu]\n", (unsigned long)pkt->size2);
            break;
        case COAP_OPTION_SIZE1:
            pkt->size1 = coap_parse_int_option(current_option, option_length);
            LOG("Size1 [%lu]\n", (unsigned long)pkt->size1);
            break;
        default:
            LOG("unknown (%u)\n", option_number);
            /* check if critical (odd) */
            if (option_number & 1) {
                coap_error_message = "Unsupported critical option";
                return BAD_OPTION_4_02;
            }
        }
        current_option += option_length;
    } /* for */
    LOG("-Done parsing-------\n");

    return NO_ERROR;
}
#if 0
int
coap_get_query_variable(coap_packet_t *const coap_pkt, const char *name,
  const char **output)
{
    if (IS_OPTION(coap_pkt, COAP_OPTION_URI_QUERY)) {
        return coap_get_variable(coap_pkt->uri_query, coap_pkt->uri_query_len,
                                 name, output);
    }
    return 0;
}
int
coap_get_post_variable(coap_packet_t *coap_pkt, const char *name,
                       const char **output)
{
    if (coap_pkt->payload_len) {
        return coap_get_variable(coap_pkt->payload, coap_pkt->payload_len,
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
coap_set_token(coap_packet_t *coap_pkt, const uint8_t *token, size_t token_len)
{
    coap_pkt->token_len = MIN(COAP_TOKEN_LEN, token_len);
    memcpy(coap_pkt->token, token, coap_pkt->token_len);

    return coap_pkt->token_len;
}
#ifdef OC_CLIENT
int
coap_get_header_content_format(coap_packet_t *coap_pkt, unsigned int *format)
{
    if (!IS_OPTION(coap_pkt, COAP_OPTION_CONTENT_FORMAT)) {
        return 0;
    }
    *format = coap_pkt->content_format;
    return 1;
}
#endif
int
coap_set_header_content_format(coap_packet_t *coap_pkt, unsigned int format)
{
    coap_pkt->content_format = format;
    SET_OPTION(coap_pkt, COAP_OPTION_CONTENT_FORMAT);
    return 1;
}
/*---------------------------------------------------------------------------*/
#if 0
int
coap_get_header_accept(coap_packet_t *coap_pkt, unsigned int *accept)
{
    if (!IS_OPTION(coap_pkt, COAP_OPTION_ACCEPT)) {
        return 0;
    }
    *accept = coap_pkt->accept;
    return 1;
}
#endif

#ifdef OC_CLIENT
int
coap_set_header_accept(coap_packet_t *coap_pkt, unsigned int accept)
{
    coap_pkt->accept = accept;
    SET_OPTION(coap_pkt, COAP_OPTION_ACCEPT);
    return 1;
}
#endif
/*---------------------------------------------------------------------------*/
#if 0
int
coap_get_header_max_age(coap_packet_t *coap_pkt, uint32_t *age)
{
    if (!IS_OPTION(coap_pkt, COAP_OPTION_MAX_AGE)) {
        *age = COAP_DEFAULT_MAX_AGE;
    } else {
        *age = coap_pkt->max_age;
    }
    return 1;
}
#endif
int
coap_set_header_max_age(coap_packet_t *coap_pkt, uint32_t age)
{
    coap_pkt->max_age = age;
    SET_OPTION(coap_pkt, COAP_OPTION_MAX_AGE);
    return 1;
}
/*---------------------------------------------------------------------------*/
#if 0
int
coap_get_header_etag(coap_packet_t *coap_pkt, const uint8_t **etag)
{
    if (!IS_OPTION(coap_pkt, COAP_OPTION_ETAG)) {
        return 0;
    }
    *etag = coap_pkt->etag;
    return coap_pkt->etag_len;
}

int
coap_set_header_etag(coap_packet_t *coap_pkt, const uint8_t *etag,
  size_t etag_len)
{
    coap_pkt->etag_len = MIN(COAP_ETAG_LEN, etag_len);
    memcpy(coap_pkt->etag, etag, coap_pkt->etag_len);

    SET_OPTION(coap_pkt, COAP_OPTION_ETAG);
    return coap_pkt->etag_len;
}
/*---------------------------------------------------------------------------*/
/*FIXME support multiple ETags */

int
coap_get_header_if_match(coap_packet_t *coap_pkt, const uint8_t **etag)
{
    if (!IS_OPTION(coap_pkt, COAP_OPTION_IF_MATCH)) {
        return 0;
    }
    *etag = coap_pkt->if_match;
    return coap_pkt->if_match_len;
}

int
coap_set_header_if_match(coap_packet_t *coap_pkt, const uint8_t *etag,
  size_t etag_len)
{
    coap_pkt->if_match_len = MIN(COAP_ETAG_LEN, etag_len);
    memcpy(coap_pkt->if_match, etag, coap_pkt->if_match_len);

    SET_OPTION(coap_pkt, COAP_OPTION_IF_MATCH);
    return coap_pkt->if_match_len;
}

/*---------------------------------------------------------------------------*/
int
coap_get_header_if_none_match(coap_packet_t *coap_pkt)
{
    return IS_OPTION(coap_pkt, COAP_OPTION_IF_NONE_MATCH) ? 1 : 0;
}

int
coap_set_header_if_none_match(coap_packet_t *coap_pkt)
{
    SET_OPTION(coap_pkt, COAP_OPTION_IF_NONE_MATCH);
    return 1;
}
/*---------------------------------------------------------------------------*/
int
coap_get_header_proxy_uri(coap_packet_t *coap_pkt, const char **uri)
{
    if (!IS_OPTION(coap_pkt, COAP_OPTION_PROXY_URI)) {
        return 0;
    }
    *uri = coap_pkt->proxy_uri;
    return coap_pkt->proxy_uri_len;
}

int
coap_set_header_proxy_uri(coap_packet_t *coap_pkt, const char *uri)
{
    /*TODO Provide alternative that sets Proxy-Scheme and Uri-* options and provide er-coap-conf define */

    coap_pkt->proxy_uri = uri;
    coap_pkt->proxy_uri_len = strlen(uri);

    SET_OPTION(coap_pkt, COAP_OPTION_PROXY_URI);
    return coap_pkt->proxy_uri_len;
}
/*---------------------------------------------------------------------------*/
int
coap_get_header_uri_host(coap_packet_t *coap_pkt, const char **host)
{
    if (!IS_OPTION(coap_pkt, COAP_OPTION_URI_HOST)) {
        return 0;
    }
    *host = coap_pkt->uri_host;
    return coap_pkt->uri_host_len;
}

int
coap_set_header_uri_host(coap_packet_t *coap_pkt, const char *host)
{
    coap_pkt->uri_host = host;
    coap_pkt->uri_host_len = strlen(host);

    SET_OPTION(coap_pkt, COAP_OPTION_URI_HOST);
    return coap_pkt->uri_host_len;
}
#endif
/*---------------------------------------------------------------------------*/
int
coap_get_header_uri_path(coap_packet_t *coap_pkt, const char **path)
{
    if (!IS_OPTION(coap_pkt, COAP_OPTION_URI_PATH)) {
        return 0;
    }
    *path = coap_pkt->uri_path;
    return coap_pkt->uri_path_len;
}
#ifdef OC_CLIENT
int
coap_set_header_uri_path(coap_packet_t *coap_pkt, const char *path)
{
    while (path[0] == '/') {
        ++path;
    }
    coap_pkt->uri_path = path;
    coap_pkt->uri_path_len = strlen(path);

    SET_OPTION(coap_pkt, COAP_OPTION_URI_PATH);
    return coap_pkt->uri_path_len;
}
#endif
/*---------------------------------------------------------------------------*/
int
coap_get_header_uri_query(coap_packet_t *coap_pkt, const char **query)
{
    if (!IS_OPTION(coap_pkt, COAP_OPTION_URI_QUERY)) {
        return 0;
    }
    *query = coap_pkt->uri_query;
    return coap_pkt->uri_query_len;
}
#ifdef OC_CLIENT
int
coap_set_header_uri_query(coap_packet_t *coap_pkt, const char *query)
{
    while (query[0] == '?') {
        ++query;
    }
    coap_pkt->uri_query = query;
    coap_pkt->uri_query_len = strlen(query);

    SET_OPTION(coap_pkt, COAP_OPTION_URI_QUERY);
    return coap_pkt->uri_query_len;
}
#endif
/*---------------------------------------------------------------------------*/
#if 0
int
coap_get_header_location_path(coap_packet_t *coap_pkt, const char **path)
{
    if (!IS_OPTION(coap_pkt, COAP_OPTION_LOCATION_PATH)) {
        return 0;
    }
    *path = coap_pkt->location_path;
    return coap_pkt->location_path_len;
}

int
coap_set_header_location_path(coap_packet_t *coap_pkt, const char *path)
{
    char *query;

    while(path[0] == '/') {
        ++path;
    }
    if ((query = strchr(path, '?'))) {
        coap_set_header_location_query(packet, query + 1);
        coap_pkt->location_path_len = query - path;
    } else {
        coap_pkt->location_path_len = strlen(path);
    }
    coap_pkt->location_path = path;

    if (coap_pkt->location_path_len > 0) {
        SET_OPTION(coap_pkt, COAP_OPTION_LOCATION_PATH);
    }
    return coap_pkt->location_path_len;
}
/*---------------------------------------------------------------------------*/
int
coap_get_header_location_query(coap_packet_t *coap_pkt, const char **query)
{
    if (!IS_OPTION(coap_pkt, COAP_OPTION_LOCATION_QUERY)) {
        return 0;
    }
    *query = coap_pkt->location_query;
    return coap_pkt->location_query_len;
}
#endif
int
coap_set_header_location_query(coap_packet_t *coap_pkt, const char *query)
{
    while (query[0] == '?') {
        ++query;
    }
    coap_pkt->location_query = query;
    coap_pkt->location_query_len = strlen(query);

    SET_OPTION(coap_pkt, COAP_OPTION_LOCATION_QUERY);
    return coap_pkt->location_query_len;
}
/*---------------------------------------------------------------------------*/
int
coap_get_header_observe(coap_packet_t *coap_pkt, uint32_t *observe)
{
    if (!IS_OPTION(coap_pkt, COAP_OPTION_OBSERVE)) {
        return 0;
    }
    *observe = coap_pkt->observe;
    return 1;
}

int
coap_set_header_observe(coap_packet_t *coap_pkt, uint32_t observe)
{
    coap_pkt->observe = observe;
    SET_OPTION(coap_pkt, COAP_OPTION_OBSERVE);
    return 1;
}
/*---------------------------------------------------------------------------*/
int
coap_get_header_block2(coap_packet_t *coap_pkt, uint32_t *num,
                       uint8_t *more, uint16_t *size, uint32_t *offset)
{
    if (!IS_OPTION(coap_pkt, COAP_OPTION_BLOCK2)) {
        return 0;
    }
    /* pointers may be NULL to get only specific block parameters */
    if (num != NULL) {
        *num = coap_pkt->block2_num;
    }
    if (more != NULL) {
        *more = coap_pkt->block2_more;
    }
    if (size != NULL) {
        *size = coap_pkt->block2_size;
    }
    if (offset != NULL) {
        *offset = coap_pkt->block2_offset;
    }
    return 1;
}

int
coap_set_header_block2(coap_packet_t *coap_pkt, uint32_t num,
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
    coap_pkt->block2_num = num;
    coap_pkt->block2_more = more ? 1 : 0;
    coap_pkt->block2_size = size;

    SET_OPTION(coap_pkt, COAP_OPTION_BLOCK2);
    return 1;
}
/*---------------------------------------------------------------------------*/
int
coap_get_header_block1(coap_packet_t *coap_pkt, uint32_t *num, uint8_t *more,
                       uint16_t *size, uint32_t *offset)
{
    if (!IS_OPTION(coap_pkt, COAP_OPTION_BLOCK1)) {
        return 0;
    }
    /* pointers may be NULL to get only specific block parameters */
    if (num != NULL) {
        *num = coap_pkt->block1_num;
    }
    if (more != NULL) {
        *more = coap_pkt->block1_more;
    }
    if (size != NULL) {
        *size = coap_pkt->block1_size;
    }
    if (offset != NULL) {
        *offset = coap_pkt->block1_offset;
    }
    return 1;
}

int
coap_set_header_block1(coap_packet_t *coap_pkt, uint32_t num, uint8_t more,
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
    coap_pkt->block1_num = num;
    coap_pkt->block1_more = more;
    coap_pkt->block1_size = size;

    SET_OPTION(coap_pkt, COAP_OPTION_BLOCK1);
    return 1;
}
/*---------------------------------------------------------------------------*/
#if 0
int coap_get_header_size2(coap_packet_t * const coap_pkt, uint32_t *size)
{
    if (!IS_OPTION(coap_pkt, COAP_OPTION_SIZE2)) {
        return 0;
    }
    *size = coap_pkt->size2;
    return 1;
}

int
coap_set_header_size2(coap_packet_t *coap_pkt, uint32_t size)
{
    coap_pkt->size2 = size;
    SET_OPTION(coap_pkt, COAP_OPTION_SIZE2);
    return 1;
}
/*---------------------------------------------------------------------------*/
int
coap_get_header_size1(coap_packet_t *coap_pkt, uint32_t *size)
{
    if (!IS_OPTION(coap_pkt, COAP_OPTION_SIZE1)) {
        return 0;
    }
    *size = coap_pkt->size1;
    return 1;
}
int
coap_set_header_size1(coap_packet_t *coap_pkt, uint32_t size)
{
    coap_pkt->size1 = size;
    SET_OPTION(coap_pkt, COAP_OPTION_SIZE1);
    return 1;
}
#endif
/*---------------------------------------------------------------------------*/
int
coap_get_payload(coap_packet_t *coap_pkt, const uint8_t **payload)
{
    if (coap_pkt->payload) {
        *payload = coap_pkt->payload;
        return coap_pkt->payload_len;
    } else {
        *payload = NULL;
        return 0;
    }
}
int
coap_set_payload(coap_packet_t *coap_pkt, const void *payload, size_t length)
{
    coap_pkt->payload = (uint8_t *)payload;
    coap_pkt->payload_len = MIN(MAX_PAYLOAD_SIZE, length);

    return coap_pkt->payload_len;
}
/*---------------------------------------------------------------------------*/
