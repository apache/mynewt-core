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

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "sysinit/sysinit.h"
#include "os/os.h"
#include "console/console.h"
#include "hash/hash.h"
#include "mbedtls/sha256.h"
#include "tinycrypt/sha256.h"

struct vector_data {
    char *in;
    uint16_t inlen;
    char *digest;
};

struct test_vectors {
    char *name;
    uint16_t algo;
    uint8_t digestlen;
    uint8_t len;
    struct vector_data vectors[];
};

/*
 * vectors from:
 *   http://csrc.nist.gov/groups/ST/toolkit/documents/Examples/SHA_All.pdf
 */

static struct test_vectors sha224_vectors = {
    .name = "SHA-224",
    .algo = HASH_ALGO_SHA224,
    .len = 4,
    .digestlen = 28,
    .vectors = {
        {
            .in = "abc",
            .inlen = 3,
            .digest = "\x23\x09\x7d\x22\x34\x05\xd8\x22"
                      "\x86\x42\xa4\x77\xbd\xa2\x55\xb3"
                      "\x2a\xad\xbc\xe4\xbd\xa0\xb3\xf7"
                      "\xe3\x6c\x9d\xa7",
        },
        {
            .in = "",
            .inlen = 0,
            .digest = "\xd1\x4a\x02\x8c\x2a\x3a\x2b\xc9"
                      "\x47\x61\x02\xbb\x28\x82\x34\xc4"
                      "\x15\xa2\xb0\x1f\x82\x8e\xa6\x2a"
                      "\xc5\xb3\xe4\x2f",
        },
        {
            .in = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
            .inlen = 56,
            .digest = "\x75\x38\x8b\x16\x51\x27\x76\xcc"
                      "\x5d\xba\x5d\xa1\xfd\x89\x01\x50"
                      "\xb0\xc6\x45\x5c\xb4\xf5\x8b\x19"
                      "\x52\x52\x25\x25",
        },
        {
            .in = "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmn"
                  "hijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
            .inlen = 112,
            .digest = "\xc9\x7c\xa9\xa5\x59\x85\x0c\xe9"
                      "\x7a\x04\xa9\x6d\xef\x6d\x99\xa9"
                      "\xe0\xe0\xe2\xab\x14\xe6\xb8\xdf"
                      "\x26\x5f\xc0\xb3",
        },
    },
};

static struct test_vectors sha256_vectors = {
    .name = "SHA-256",
    .algo = HASH_ALGO_SHA256,
    .len = 4,
    .digestlen = 32,
    .vectors = {
        {
            .in = "abc",
            .inlen = 3,
            .digest = "\xba\x78\x16\xbf\x8f\x01\xcf\xea"
                      "\x41\x41\x40\xde\x5d\xae\x22\x23"
                      "\xb0\x03\x61\xa3\x96\x17\x7a\x9c"
                      "\xb4\x10\xff\x61\xf2\x00\x15\xad",
        },
        {
            .in = "",
            .inlen = 0,
            .digest = "\xe3\xb0\xc4\x42\x98\xfc\x1c\x14"
                      "\x9a\xfb\xf4\xc8\x99\x6f\xb9\x24"
                      "\x27\xae\x41\xe4\x64\x9b\x93\x4c"
                      "\xa4\x95\x99\x1b\x78\x52\xb8\x55",
        },
        {
            .in = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
            .inlen = 56,
            .digest = "\x24\x8d\x6a\x61\xd2\x06\x38\xb8"
                      "\xe5\xc0\x26\x93\x0c\x3e\x60\x39"
                      "\xa3\x3c\xe4\x59\x64\xff\x21\x67"
                      "\xf6\xec\xed\xd4\x19\xdb\x06\xc1",
        },
        {
            .in = "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmn"
                  "hijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
            .inlen = 112,
            .digest = "\xcf\x5b\x16\xa7\x78\xaf\x83\x80"
                      "\x03\x6c\xe5\x9e\x7b\x04\x92\x37"
                      "\x0b\x24\x9b\x11\xe8\xf0\x7a\x51"
                      "\xaf\xac\x45\x03\x7a\xfe\xe9\xd1",
        },
    },
};

static struct test_vectors *all_tests[] = {
    &sha224_vectors,
    &sha256_vectors,
    NULL,
};

void
run_nist_vectors(struct hash_dev *hash, struct test_vectors *test_mode)
{
    uint8_t *inbuf;
    uint16_t algo;
    uint8_t outbuf[HASH_MAX_DIGEST_LEN];
    struct vector_data *vectors;
    struct vector_data *vector;
    int i;
    uint32_t sz;

    algo = test_mode->algo;
    vectors = test_mode->vectors;
    sz = test_mode->digestlen;

    printf("%s hash\n", test_mode->name);
    for (i = 0; i < test_mode->len; i++) {
        printf("\tvector %d: ", i);
        vector = &vectors[i];
        inbuf = (uint8_t *)vector->in;

        if (!hash_has_support(hash, algo)) {
            printf("not supported\n");
            continue;
        }

        if (hash_custom_process(hash, algo, inbuf, vector->inlen, outbuf) != 0) {
            printf("fail\n");
            continue;
        }

        if (memcmp(outbuf, vector->digest, sz) == 0) {
            printf("ok\n");
        } else {
            printf("invalid\n");
        }
    }
}

struct stream_data {
    char *name;
    uint16_t algo;
    uint8_t digestlen;
    char *digest;
};

static struct stream_data streams[] = {
    {
        .name = "SHA-224",
        .algo = HASH_ALGO_SHA224,
        .digestlen = 28,
        .digest = "\x20\x79\x46\x55\x98\x0c\x91\xd8"
                  "\xbb\xb4\xc1\xea\x97\x61\x8a\x4b"
                  "\xf0\x3f\x42\x58\x19\x48\xb2\xee"
                  "\x4e\xe7\xad\x67",
    },
    {
        .name = "SHA-256",
        .algo = HASH_ALGO_SHA256,
        .digestlen = 32,
        .digest = "\xcd\xc7\x6e\x5c\x99\x14\xfb\x92"
                  "\x81\xa1\xc7\xe2\x84\xd7\x3e\x67"
                  "\xf1\x80\x9a\x48\xa4\x97\x20\x0e"
                  "\x04\x6d\x39\xcc\xc7\x11\x2c\xd0",
    },
};

void
run_stream_test(struct hash_dev *hash)
{
    uint8_t inbuf[HASH_MAX_BLOCK_LEN];
    uint8_t outbuf[HASH_MAX_DIGEST_LEN];
    struct hash_generic_context ctx;
    struct stream_data *stream;
    int i, d;
    int rc;

    for (d = 0; d < sizeof(streams)/sizeof(streams[0]); d++) {
        stream = &streams[d];
        printf("%s: ", stream->name);

        if (!hash_has_support(hash, stream->algo)) {
            printf("unsupported\n");
            continue;
        }

        rc = hash_custom_start(hash, &ctx, stream->algo);
        if (rc) {
            printf("failure\n");
            continue;
        }

        memset(inbuf, 'a', 64);

        /*
         * XXX no test needed because 1000000 is a multiple of 64
         */
        for (i = 0; i < 1000000; i += 64) {
            rc = hash_custom_update(hash, &ctx, stream->algo, inbuf, 64);
            if (rc) {
                printf("failure\n");
                continue;
            }
        }

        rc = hash_custom_finish(hash, &ctx, stream->algo, outbuf);
        if (rc) {
            printf("failure\n");
        }

        if (memcmp(outbuf, stream->digest, stream->digestlen) == 0) {
            printf("ok\n");
        } else {
            printf("invalid\n");
        }
    }
}

typedef void (* hash_start_func_t)(void *, void *);
typedef void (* hash_update_func_t)(void *, const uint8_t *, uint32_t);
typedef void (* hash_finish_func_t)(void *, uint8_t *);

static void
_hash_sha256_start(void *data, void *arg)
{
    (void)hash_sha256_start(data, arg);
}

static void
_hash_sha256_update(void *data, const uint8_t *input, uint32_t inlen)
{
    (void)hash_sha256_update(data, input, inlen);
}

static void
_hash_sha256_finish(void *data, uint8_t *output)
{
    (void)hash_sha256_finish(data, output);
}

static void
mbed_sha256_start(void *data, void *arg)
{
    (void)arg;
    (void)mbedtls_sha256_starts_ret((mbedtls_sha256_context *)data, 0);
}

static void
mbed_sha256_update(void *data, const uint8_t *input, uint32_t inlen)
{
    (void)mbedtls_sha256_update_ret((mbedtls_sha256_context *)data, input,
            (uint16_t)inlen);
}

static void
mbed_sha256_finish(void *data, uint8_t *output)
{
    (void)mbedtls_sha256_finish_ret((mbedtls_sha256_context *)data, output);
}

static void
tc_sha256_start(void *data, void *arg)
{
    (void)arg;
    (void)tc_sha256_init((TCSha256State_t)data);
}

static void
_tc_sha256_update(void *data, const uint8_t *input, uint32_t inlen)
{
    (void)tc_sha256_update((TCSha256State_t)data, input, inlen);
}

static void
tc_sha256_finish(void *data, uint8_t *output)
{
    (void)tc_sha256_final(output, (TCSha256State_t)data);
}

static void
run_sha256_benchmark(char *name, hash_start_func_t start_fn,
        hash_update_func_t update_fn, hash_finish_func_t finish_fn,
        void *ctx, void *start_arg)
{
    int i;
    uint8_t output[SHA256_DIGEST_LEN];
    uint8_t input[SHA256_BLOCK_LEN];
    os_time_t t;
    char *expected_digest = "\xcd\xc7\x6e\x5c\x99\x14\xfb\x92"
                            "\x81\xa1\xc7\xe2\x84\xd7\x3e\x67"
                            "\xf1\x80\x9a\x48\xa4\x97\x20\x0e"
                            "\x04\x6d\x39\xcc\xc7\x11\x2c\xd0";

    printf("%s - running on 1000000 input chars... ", name);
    t = os_time_get();
    memset(input, 'a', SHA256_BLOCK_LEN);
    start_fn(ctx, start_arg);
    for (i = 0; i < 1000000; i += SHA256_BLOCK_LEN) {
        update_fn(ctx, input, SHA256_BLOCK_LEN);
    }
    finish_fn(ctx, output);
    if (memcmp(output, expected_digest, SHA256_DIGEST_LEN)) {
        printf("fail\n");
        return;
    }
    printf("done in %lu ticks\n", os_time_get() - t);
}

static void
concurrency_test_handler(void *arg)
{
    struct os_task *t;
    struct hash_dev *hash = (struct hash_dev *)arg;
    uint16_t ok, fail;
    struct hash_sha256_context ctx;
    uint8_t input[SHA256_BLOCK_LEN];
    uint8_t output[HASH_MAX_BLOCK_LEN];
    char *expected_digest = "\xcd\xc7\x6e\x5c\x99\x14\xfb\x92"
                            "\x81\xa1\xc7\xe2\x84\xd7\x3e\x67"
                            "\xf1\x80\x9a\x48\xa4\x97\x20\x0e"
                            "\x04\x6d\x39\xcc\xc7\x11\x2c\xd0";
    int i, j;

    t = os_sched_get_current_task();
    ok = fail = 0;
    memset(input, 'a', SHA256_BLOCK_LEN);
    for (i = 10; i > 0; i--) {
        (void)hash_custom_start(hash, &ctx, HASH_ALGO_SHA256);
        for (j = 0; j < 1000000; j += SHA256_BLOCK_LEN) {
            hash_custom_update(hash, &ctx, HASH_ALGO_SHA256, input,
                    SHA256_BLOCK_LEN);
        }
        (void)hash_custom_finish(hash, &ctx, HASH_ALGO_SHA256, output);
        if (memcmp(output, expected_digest, SHA256_DIGEST_LEN)) {
            fail++;
        } else {
            ok++;
        }
        os_time_delay(10);
    }

    printf("%s [%d fails / %d ok] done\n", t->t_name, fail, ok);

    while (1) {
        os_time_delay(OS_TICKS_PER_SEC);
    }
}

#define TASK_AMOUNT 8
#define STACK_SIZE 128
static struct os_task tasks[TASK_AMOUNT];
static char names[TASK_AMOUNT][6];
static void
run_concurrency_test(struct hash_dev *hash)
{
    os_stack_t *pstack;
    int i;

    printf("\n=== Concurrency [%d tasks] ===\n", TASK_AMOUNT);

    assert(TASK_AMOUNT < 10);
    for (i = 0; i < TASK_AMOUNT; i++) {
        pstack = malloc(sizeof(os_stack_t) * STACK_SIZE);
        assert(pstack);

        sprintf(names[i], "task%c", i + '0');
        os_task_init(&tasks[i], names[i], concurrency_test_handler, hash,
                8 + i, OS_WAIT_FOREVER, pstack, STACK_SIZE);
    }
}

int
main(void)
{
    struct hash_dev *hash;
    struct hash_sha256_context hash_sha256;
    mbedtls_sha256_context mbed_sha256;
    struct tc_sha256_state_struct tc_sha256;
    int i;

    sysinit();

    hash = (struct hash_dev *) os_dev_open("hash", OS_TIMEOUT_NEVER, NULL);
    assert(hash);

    printf("\n=== NIST vectors ===\n");
    for (i = 0; all_tests[i] != NULL; i++) {
        run_nist_vectors(hash, all_tests[i]);
    }

    printf("\n=== SHA-256 of 1000000 'a' letters ===\n");
    run_stream_test(hash);

    mbedtls_sha256_init(&mbed_sha256);
    tc_sha256_init(&tc_sha256);

    for (i = 1; i <= 3; i++) {
        printf("\n=== Benchmarks - iteration %d ===\n", i);
        run_sha256_benchmark("HASH", _hash_sha256_start, _hash_sha256_update,
                _hash_sha256_finish, &hash_sha256, hash);
        run_sha256_benchmark("MBEDTLS", mbed_sha256_start, mbed_sha256_update,
                mbed_sha256_finish, &mbed_sha256, NULL);
        run_sha256_benchmark("TINYCRYPT", tc_sha256_start, _tc_sha256_update,
                tc_sha256_finish, &tc_sha256, NULL);
        os_time_delay(OS_TICKS_PER_SEC);
    }

    run_concurrency_test(hash);

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }

    return 0;
}
