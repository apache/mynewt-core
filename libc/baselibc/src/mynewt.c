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

#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <console/console.h>
#include <os/os_time.h>
#include <os/os_mutex.h>

static size_t
stdin_read(FILE *fp, char *bp, size_t n)
{
    return 0;
}

static size_t
stdout_write(FILE *fp, const char *bp, size_t n)
{
    console_write(bp, n);
    return n;
}

static struct File_methods _stdin_methods = {
    .write = stdout_write,
    .read = stdin_read
};

static struct File _stdin = {
    .vmt = &_stdin_methods
};

struct File *const stdin = &_stdin;
struct File *const stdout = &_stdin;
struct File *const stderr = &_stdin;

int __attribute__((weak))
fflush(FILE *stream)
{
    return 0;
}

static const char *
eat_spaces(const char *p)
{
    while (isspace(*p)) {
        ++p;
    }

    return p;
}

static const char *
eat_sign(const char *p, int *sign)
{
    *sign = 1;

    if (*p == '+' || *p == '-') {
        *sign = ',' - *p;
        ++p;
    }

    return p;
}

static const char *
eat_base(const char *p, int *base)
{
    if (*p == '0' && (p[1] == 'x' || p[1] == 'X')) {
        *base = 16;
        p += 2;
    } else {
        *base = 10;
    }
    return p;
}

static const char *
eat_leading_zeros(const char *p)
{
    while (*p == '0') {
        ++p;
    }

    return p;
}

static const char *
eat_digits(const char *p, unsigned int *n, bool frac, int *exp)
{
    unsigned int val = *n;

    while (isdigit(*p)) {
        if (val < 429496729) {
            val = val * 10 + *p - '0';
            if (frac) {
                --*exp;
            }
        } else if (!frac && exp) {
            ++*exp;
        }
        ++p;
    }
    *n = val;

    return p;
}

static float
fexp10(int mant, int n)
{
    float result = (float)mant;
    float mul = 10.0f;
    bool plus = n >= 0;

    n = abs(n);

    for (; n != 0; n >>= 1) {
        if (n & 1) {
            if (plus) {
                result *= mul;
            } else {
                result /= mul;
            }
        }
        mul = mul * mul;
    }
    return result;
}

static bool
eat_dot(const char **s)
{
    if (**s == '.') {
        ++*s;
        return true;
    }
    return false;
}

static bool
eat_e(const char **s)
{
    if (**s == 'E' || **s == 'e') {
        ++*s;
        return true;
    }
    return false;
}

float
strtof(const char *str, char **strend)
{
    const char *p = str;
    int sign;
    int base;
    unsigned int mant = 0;
    int exp = 0;
    int exp_sign = 1;
    float f = 0;

    p = eat_spaces(p);
    p = eat_sign(p, &sign);
    p = eat_base(p, &base);
    p = eat_leading_zeros(p);
    if (base == 10) {
        p = eat_digits(p, &mant, false, &exp);
        if (eat_dot(&p)) {
            if (mant == 0) {
                const char *s = p;
                p = eat_leading_zeros(p);
                exp = s - p;
            }
            p = eat_digits(p, &mant, true, &exp);
        }
        if (eat_e(&p)) {
            unsigned int exp1 = 0;

            p = eat_sign(p, &exp_sign);
            p = eat_leading_zeros(p);
            p = eat_digits(p, &exp1, false, NULL);
            exp += exp_sign * exp1;
        }
        if (exp > 45) {
            f = exp_sign == 1 ? INFINITY : 0.0f;
        } else if (exp == 0) {
            f = (float)mant;
        } else {
            f = fexp10(mant, exp);
        }
        f *= sign;
    } else if (base == 16) {
        /* Not implemented */
    }
    if (strend) {
        *strend = (char *)p;
    }
    return f;
}

#if MYNEWT_VAL(BASELIBC_THREAD_SAFE_HEAP_ALLOCATION)

static struct os_mutex heap_mutex;

bool heap_lock(void)
{
    return os_mutex_pend(&heap_mutex, OS_TIMEOUT_NEVER) == 0;
}

void heap_unlock(void)
{
    os_mutex_release(&heap_mutex);
}

void
baselibc_init(void)
{
    /* Setup mutex to be used for malloc/calloc/free */
    os_mutex_init(&heap_mutex);
    set_malloc_locking(heap_lock, heap_unlock);
}

#endif
