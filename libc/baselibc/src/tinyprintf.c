/*
File: tinyprintf.c

Copyright (C) 2004  Kustaa Nyholm
Copyright (C) 2010  CJlano
Copyright (C) 2011  Petteri Aimonen

This file is dual-licensed. You can use either of these licenses:

1) GNU LGPL
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

2) BSD
Copyright (c) 2004,2012 Kustaa Nyholm / SpareTimeLabs
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of Kustaa Nyholm or SpareTimeLabs nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDER BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

/* This is a smaller implementation of printf-family of functions,
 * based on tinyprintf code by Kustaa Nyholm.
 * The formats supported by this implementation are:
 *     'd' 'u' 'c' 's' 'x' 'X' 'p'.
 * Zero padding and field width are also supported.
 * If the library is compiled with 'PRINTF_SUPPORT_LONG' defined then the
 * long specifier is also supported.
 * Otherwise it is ignored, so on 32 bit platforms there is no point to use
 * PRINTF_SUPPORT_LONG because int == long.
 */

#include <stdarg.h>
#include <stdio.h>
#include <inttypes.h>

struct param {
    unsigned char width; /**< field width */
    char lz;            /**< Leading zeros */
    char sign:1;        /**<  The sign to display (if any) */
    char alt:1;         /**< alternate form */
    char uc:1;          /**<  Upper case (for base16 only) */
    char left:1;        /**<  Force text to left (padding on right) */
    char base;  /**<  number base (e.g.: 8, 10, 16) */
    char *bf;           /**<  Buffer to output */
};

static void ui2a(unsigned long long int num, struct param *p)
{
    int n = 0;
    unsigned long long int d = 1;
    char *bf = p->bf;
    while (num / d >= p->base)
        d *= p->base;
    while (d != 0) {
        unsigned long long  dgt = num / d;
        num %= d;
        d /= p->base;
        if (n || dgt > 0 || d == 0) {
            *bf++ = dgt + (dgt < 10 ? '0' : (p->uc ? 'A' : 'a') - 10);
            ++n;
        }
    }
    *bf = 0;
}

static void i2a(long long int num, struct param *p)
{
    if (num < 0) {
        num = -num;
        p->sign = 1;
    }
    ui2a(num, p);
}

static int a2d(char ch)
{
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    else if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    else if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    else
        return -1;
}

static char a2i(char ch, const char **src, int base, unsigned char *nump)
{
    const char *p = *src;
    int num = 0;
    int digit;
    while ((digit = a2d(ch)) >= 0) {
        if (digit > base)
            break;
        num = num * base + digit;
        ch = *p++;
    }
    *src = p;
    *nump = num;
    return ch;
}

static int putf(FILE *putp, char c)
{
    if (fputc(c, putp) == EOF)
        return 0;
    else
        return 1;
}

static unsigned putchw(FILE *putp, struct param *p)
{
    unsigned written = 0;
    char ch;
    int n = p->width;
    char *bf = p->bf;

    /* Number of filling characters */
    while (*bf++ && n > 0)
        n--;
    if (p->sign)
        n--;
    if (p->alt && p->base == 16)
        n -= 2;
    else if (p->alt && p->base == 8)
        n--;

    /* Unless left-aligned, fill with space, before alternate or sign */
    if (!p->lz && !p->left) {
        while (n-- > 0)
            written += putf(putp, ' ');
    }

    /* print sign */
    if (p->sign)
        written += putf(putp, '-');

    /* Alternate */
    if (p->alt && p->base == 16) {
        written += putf(putp, '0');
        written += putf(putp, (p->uc ? 'X' : 'x'));
    } else if (p->alt && p->base == 8) {
        written += putf(putp, '0');
    }

    /* Fill with zeros, after alternate or sign */
    if (p->lz) {
        while (n-- > 0)
            written += putf(putp, '0');
    }

    /* Put actual buffer */
    bf = p->bf;
    while ((ch = *bf++))
        written += putf(putp, ch);

    /* If left-aligned, pad the end with spaces. */
    if (p->left) {
        while (n-- > 0)
            written += putf(putp, ' ');
    }
    
    return written;
}

static unsigned long long
intarg(int lng, int sign, va_list *va)
{
    unsigned long long val;

    switch (lng) {
    case 0:
        if (sign) {
            val = va_arg(*va, int);
        } else {
            val = va_arg(*va, unsigned int);
        }
        break;

    case 1:
        if (sign) {
            val = va_arg(*va, long);
        } else {
            val = va_arg(*va, unsigned long);
        }
        break;

    case 2:
    default:
        if (sign) {
            val = va_arg(*va, long long);
        } else {
            val = va_arg(*va, unsigned long long);
        }
        break;
    }

    return val;
}

size_t tfp_format(FILE *putp, const char *fmt, va_list va)
{
    size_t written = 0;
    struct param p;
    char bf[23];
    char ch;
    char lng;
    void *v;

    p.bf = bf;

    while ((ch = *(fmt++))) {
        if (ch != '%') {
            written += putf(putp, ch);
        } else {
            /* Init parameter struct */
            p.lz = 0;
            p.alt = 0;
            p.width = 0;
            p.sign = 0;
            p.left = 0;
            p.uc = 0;
            lng = 0;

            /* Flags */
            while ((ch = *(fmt++))) {
                switch (ch) {
                case '0':
                    if (!p.left) {
                        p.lz = 1;
                    }
                    continue;
                case '#':
                    p.alt = 1;
                    continue;
                case '-':
                    p.left = 1;
                    p.lz = 0;
                    continue;
                default:
                    break;
                }
                break;
            }

            /* Width */
            if (ch >= '0' && ch <= '9') {
                ch = a2i(ch, &fmt, 10, &(p.width));
            }
            if (ch == 'l') {
                ch = *(fmt++);
                lng = 1;

                if (ch == 'l') {
                    ch = *(fmt++);
                    lng = 2;
                }
            }

            if (ch == 'z') {
                ch = *(fmt++);
            }

            switch (ch) {
            case 0:
                goto abort;
            case 'u':
                p.base = 10;
                ui2a(intarg(lng, 0, &va), &p);
                written += putchw(putp, &p);
                break;
            case 'd':
            case 'i':
                p.base = 10;
                i2a(intarg(lng, 1, &va), &p);
                written += putchw(putp, &p);
                break;
            case 'x':
            case 'X':
                p.base = 16;
                p.uc = (ch == 'X');
                ui2a(intarg(lng, 0, &va), &p);
                written += putchw(putp, &p);
                break;
            case 'o':
                p.base = 8;
                ui2a(intarg(lng, 0, &va), &p);
                written += putchw(putp, &p);
                break;
            case 'p':
                v = va_arg(va, void *);
                p.base = 16;
                ui2a((uintptr_t)v, &p);
                p.width = 2 * sizeof(void*);
                p.lz = 1;
                written += putf(putp, '0');
                written += putf(putp, 'x');
                written += putchw(putp, &p);
                break;
            case 'c':
                written += putf(putp, (char)(va_arg(va, int)));
                break;
            case 's':
                p.bf = va_arg(va, char *);
                written += putchw(putp, &p);
                p.bf = bf;
                break;
            case '%':
                written += putf(putp, ch);
                break;
            default:
                break;
            }
        }
    }
 abort:;
 
 return written;
}

int vfprintf(FILE *f, const char *fmt, va_list va)
{
    return tfp_format(f, fmt, va);
}

int fprintf(FILE *f, const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    int rv = vfprintf(f, fmt, va);
    va_end(va);
    return rv;
}

int printf(const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    int rv = vfprintf(stdout, fmt, va);
    va_end(va);
    return rv;
}

int vsnprintf(char *str, size_t size, const char *fmt, va_list va)
{
    struct MemFile state;
    FILE *f = fmemopen_w(&state, str, size);
    tfp_format(f, fmt, va);
    if (size > 0) {
        if (state.bytes_written < size) {
            *(state.buffer) = '\0';
        } else {
            str[size - 1] = '\0';
        }
    }
    return state.bytes_written;
}

int snprintf(char *str, size_t size, const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    int rv = vsnprintf(str, size, fmt, va);
    va_end(va);
    return rv;
}
