/*
 * This file is based on roken from the FreeBSD source.  It has been modified
 * to not use malloc() and instead expect static buffers, and tabs have been
 * replaced with spaces.  Also, instead of strlen() on the resulting string,
 * pointer arithmitic is done, as p represents the end of the buffer.
 */

/*
 * Copyright (c) 1995-2001 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
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
 */

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <assert.h>

#include <os/mynewt.h>
#include <base64/base64.h>

static const char base64_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int
pos(char c)
{
    const char *p;
    for (p = base64_chars; *p; p++)
        if (*p == c)
            return p - base64_chars;
    return -1;
}

int
base64_encode(const void *data, int size, char *s, uint8_t should_pad)
{
    char *p;
    int i;
    int c;
    const unsigned char *q;
    char *last;
    int diff;

    p = s;

    q = (const unsigned char *) data;
    last = NULL;
    i = 0;
    while (i < size) {
        c = q[i++];
        c *= 256;
        if (i < size)
            c += q[i];
        i++;
        c *= 256;
        if (i < size)
            c += q[i];
        i++;
        p[0] = base64_chars[(c & 0x00fc0000) >> 18];
        p[1] = base64_chars[(c & 0x0003f000) >> 12];
        p[2] = base64_chars[(c & 0x00000fc0) >> 6];
        p[3] = base64_chars[(c & 0x0000003f) >> 0];
        last = p;
        p += 4;
    }

    if (last) {
        diff = i - size;
        if (diff > 0) {
            if (should_pad) {
                memset(last + (4 - diff), '=', diff);
            } else {
                p = last + (4 - diff);
            }
        }
    }

    *p = 0;

    return (p - s);
}

int
base64_pad(char *buf, int len)
{
    int remainder;

    remainder = len % 4;
    if (remainder == 0) {
        return (0);
    }

    memset(buf, '=', 4 - remainder);

    return (4 - remainder);
}

#define DECODE_ERROR -1

static unsigned int
token_decode(const char *token, int len)
{
    int i;
    unsigned int val = 0;
    int marker = 0;

    if (len < 4) {
        return DECODE_ERROR;
    }

    for (i = 0; i < 4; i++) {
        val *= 64;
        if (token[i] == '=') {
            marker++;
        } else if (marker > 0) {
            return DECODE_ERROR;
        } else {
            val += pos(token[i]);
        }
    }

    if (marker > 2) {
        return DECODE_ERROR;
    }

    return (marker << 24) | val;
}

int
base64_decode(const char *str, void *data)
{
    struct base64_decoder dec = {
        .src = str,
        .dst = data,
    };

    return base64_decoder_go(&dec);
}

int
base64_decode_maxlen(const char *str, void *data, int len)
{
    struct base64_decoder dec = {
        .src = str,
        .dst = data,
        .dst_len = len,
    };

    return base64_decoder_go(&dec);
}

int
base64_decode_len(const char *str)
{
    int len;

    len = strlen(str);
    while (len && str[len - 1] == '=') {
        len--;
    }
    return len * 3 / 4;
}

int
base64_decoder_go(struct base64_decoder *dec)
{
    unsigned int marker;
    unsigned int val;
    uint8_t *dst;
    char sval;
    int read_len;
    int src_len;
    int src_rem;
    int src_off;
    int dst_len;
    int dst_off;
    int i;

    dst = dec->dst;
    dst_off = 0;
    src_off = 0;

    /* A length <= 0 means "unbounded". */
    if (dec->src_len <= 0) {
        src_len = INT_MAX;
    } else {
        src_len = dec->src_len;
    }
    if (dec->dst_len <= 0) {
        dst_len = INT_MAX;
    } else {
        dst_len = dec->dst_len;
    }

    while (1) {
        src_rem = src_len - src_off;
        if (src_rem == 0) {
            /* End of source input. */
            break;
        }

        if (dec->src[src_off] == '\0') {
            /* End of source string. */
            break;
        }

        /* Account for possibility of partial token from previous call. */
        assert(dec->buf_len < 4);
        read_len = 4 - dec->buf_len;

        /* Detect invalid input. */
        for (i = 0; i < read_len; i++) {
            sval = dec->src[src_off + i];
            if (sval == '\0') {
                /* Incomplete input. */
                return -1;
            }
            if (sval != '=' && strchr(base64_chars, sval) == NULL) {
                /* Invalid base64 character. */
                return -1;
            }
        }

        if (src_rem < read_len) {
            /* Input contains a partial token.  Stash it for use during the
             * next call.
             */
            memcpy(&dec->buf[dec->buf_len], &dec->src[src_off], src_rem);
            dec->buf_len += src_rem;
            break;
        }

        /* Copy full token into buf and decode it. */
        memcpy(&dec->buf[dec->buf_len], &dec->src[src_off], read_len);
        val = token_decode(dec->buf, read_len);
        if (val == DECODE_ERROR) {
            return -1;
        }
        src_off += read_len;
        dec->buf_len = 0;

        marker = (val >> 24) & 0xff;

        if (dst_off >= dst_len) {
            break;
        }
        dst[dst_off] = (val >> 16) & 0xff;
        dst_off++;

        if (marker < 2) {
            if (dst_off >= dst_len) {
                break;
            }
            dst[dst_off] = (val >> 8) & 0xff;
            dst_off++;
        }

        if (marker < 1) {
            if (dst_off >= dst_len) {
                break;
            }
            dst[dst_off] = val & 0xff;
            dst_off++;
        }
    }

    return dst_off;
}
