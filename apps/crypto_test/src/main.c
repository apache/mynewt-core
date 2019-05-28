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
#include "crypto/crypto.h"
#include "mbedtls/aes.h"
#include "tinycrypt/aes.h"

struct vector_data {
    char *plain;
    char *cipher;
    uint8_t sz;
};

struct test_vectors {
    char *name;
    uint16_t algo;
    uint16_t mode;
    char *key;
    uint16_t keylen;
    char *iv;
    uint8_t len;
    struct vector_data vectors[];
};

/*
 * Test vectors from "NIST Special Publication 800-38A"
 */

#if MYNEWT_VAL(CRYPTOTEST_VECTORS_ECB)
static struct test_vectors aes_128_ecb_vectors = {
    .name = "AES-128-ECB",
    .algo = CRYPTO_ALGO_AES,
    .mode = CRYPTO_MODE_ECB,
    .key = "\x2b\x7e\x15\x16\x28\xae\xd2\xa6\xab\xf7\x15\x88\x09\xcf\x4f\x3c",
    .keylen = 128,
    .iv = NULL,
    .len = 4,
    .vectors = {
        {
            .plain = "\x6b\xc1\xbe\xe2\x2e\x40\x9f\x96\xe9\x3d\x7e\x11\x73\x93\x17\x2a",
            .cipher = "\x3a\xd7\x7b\xb4\x0d\x7a\x36\x60\xa8\x9e\xca\xf3\x24\x66\xef\x97",
        },
        {
            .plain = "\xae\x2d\x8a\x57\x1e\x03\xac\x9c\x9e\xb7\x6f\xac\x45\xaf\x8e\x51",
            .cipher = "\xf5\xd3\xd5\x85\x03\xb9\x69\x9d\xe7\x85\x89\x5a\x96\xfd\xba\xaf",
        },
        {
            .plain = "\x30\xc8\x1c\x46\xa3\x5c\xe4\x11\xe5\xfb\xc1\x19\x1a\x0a\x52\xef",
            .cipher = "\x43\xb1\xcd\x7f\x59\x8e\xce\x23\x88\x1b\x00\xe3\xed\x03\x06\x88",
        },
        {
            .plain = "\xf6\x9f\x24\x45\xdf\x4f\x9b\x17\xad\x2b\x41\x7b\xe6\x6c\x37\x10",
            .cipher = "\x7b\x0c\x78\x5e\x27\xe8\xad\x3f\x82\x23\x20\x71\x04\x72\x5d\xd4",
        },
    },
};
#endif /* MYNEWT_VAL(CRYPTOTEST_VECTORS_ECB) */

#if MYNEWT_VAL(CRYPTOTEST_VECTORS_CBC)
static struct test_vectors aes_128_cbc_vectors = {
    .name = "AES-128-CBC",
    .algo = CRYPTO_ALGO_AES,
    .mode = CRYPTO_MODE_CBC,
    .key = "\x2b\x7e\x15\x16\x28\xae\xd2\xa6\xab\xf7\x15\x88\x09\xcf\x4f\x3c",
    .keylen = 128,
    .iv = "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F",
    .len = 4,
    .vectors = {
        {
            .plain = "\x6b\xc1\xbe\xe2\x2e\x40\x9f\x96\xe9\x3d\x7e\x11\x73\x93\x17\x2a",
            .cipher = "\x76\x49\xab\xac\x81\x19\xb2\x46\xce\xe9\x8e\x9b\x12\xe9\x19\x7d",
        },
        {
            .plain = "\xae\x2d\x8a\x57\x1e\x03\xac\x9c\x9e\xb7\x6f\xac\x45\xaf\x8e\x51",
            .cipher = "\x50\x86\xcb\x9b\x50\x72\x19\xee\x95\xdb\x11\x3a\x91\x76\x78\xb2",
        },
        {
            .plain = "\x30\xc8\x1c\x46\xa3\x5c\xe4\x11\xe5\xfb\xc1\x19\x1a\x0a\x52\xef",
            .cipher = "\x73\xbe\xd6\xb8\xe3\xc1\x74\x3b\x71\x16\xe6\x9e\x22\x22\x95\x16",
        },
        {
            .plain = "\xf6\x9f\x24\x45\xdf\x4f\x9b\x17\xad\x2b\x41\x7b\xe6\x6c\x37\x10",
            .cipher = "\x3f\xf1\xca\xa1\x68\x1f\xac\x09\x12\x0e\xca\x30\x75\x86\xe1\xa7",
        },
    },
};
#endif /* MYNEWT_VAL(CRYPTOTEST_VECTORS_CBC) */

#if MYNEWT_VAL(CRYPTOTEST_VECTORS_CTR)
static struct test_vectors aes_128_ctr_vectors = {
    .name = "AES-128-CTR",
    .algo = CRYPTO_ALGO_AES,
    .mode = CRYPTO_MODE_CTR,
    .key = "\x2b\x7e\x15\x16\x28\xae\xd2\xa6\xab\xf7\x15\x88\x09\xcf\x4f\x3c",
    .keylen = 128,
    .iv = "\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff",
    .len = 4,
    .vectors = {
        {
            .plain = "\x6b\xc1\xbe\xe2\x2e\x40\x9f\x96\xe9\x3d\x7e\x11\x73\x93\x17\x2a",
            .cipher = "\x87\x4d\x61\x91\xb6\x20\xe3\x26\x1b\xef\x68\x64\x99\x0d\xb6\xce",
            .sz = AES_BLOCK_LEN,
        },
        {
            .plain = "\xae\x2d\x8a\x57\x1e\x03\xac\x9c\x9e\xb7\x6f\xac\x45\xaf\x8e\x51",
            .cipher = "\x98\x06\xf6\x6b\x79\x70\xfd\xff\x86\x17\x18\x7b\xb9\xff\xfd\xff",
            .sz = AES_BLOCK_LEN,
        },
        {
            .plain = "\x30\xc8\x1c\x46\xa3\x5c\xe4\x11",
            .cipher = "\x5a\xe4\xdf\x3e\xdb\xd5\xd3\x5e",
            .sz = 8,
        },
        {
            .plain = "\xf6\x9f\x24\x45\xdf\x4f\x9b\x17\xad\x2b\x41\x7b\xe6\x6c\x37\x10",
            .cipher = "\x1e\x03\x1d\xda\x2f\xbe\x03\xd1\x79\x21\x70\xa0\xf3\x00\x9c\xee",
            .sz = AES_BLOCK_LEN,
        },
    },
};
#endif /* MYNEWT_VAL(CRYPTOTEST_VECTORS_CTR) */

static struct test_vectors *all_tests[] = {
#if MYNEWT_VAL(CRYPTOTEST_VECTORS_ECB)
    &aes_128_ecb_vectors,
#endif
#if MYNEWT_VAL(CRYPTOTEST_VECTORS_CBC)
    &aes_128_cbc_vectors,
#endif
#if MYNEWT_VAL(CRYPTOTEST_VECTORS_CTR)
    &aes_128_ctr_vectors,
#endif
    NULL,
};

void
run_test_vectors(struct crypto_dev *crypto, struct test_vectors *test_mode)
{
    uint8_t *key;
    uint8_t *inbuf;
    uint16_t algo;
    uint16_t mode;
    uint16_t keylen;
    uint8_t outbuf[AES_BLOCK_LEN];
    uint8_t iv[AES_BLOCK_LEN];
    uint8_t *ivp;
    struct vector_data *vectors;
    struct vector_data *vector;
    int i;
    uint32_t sz;
    uint32_t asksz;

    algo = test_mode->algo;
    mode = test_mode->mode;
    key = (uint8_t *)test_mode->key;
    keylen = test_mode->keylen;
    if (test_mode->iv) {
        memcpy(iv, (uint8_t *)test_mode->iv, AES_BLOCK_LEN);
        ivp = iv;
    } else {
        ivp = NULL;
    }
    vectors = test_mode->vectors;

    printf("%s enc\n", test_mode->name);
    for (i = 0; i < test_mode->len; i++) {
        printf("\tvector %d: ", i);
        vector = &vectors[i];
        inbuf = (uint8_t *)vector->plain;

        asksz = AES_BLOCK_LEN;
        if (mode == CRYPTO_MODE_CTR) {
            asksz = vector->sz;
        }
        sz = crypto_encrypt_custom(crypto, algo, mode, key, keylen, ivp,
                inbuf, outbuf, asksz);
        if (sz == asksz && memcmp(outbuf, vector->cipher, sz) == 0) {
            printf("ok, sz=%lu\n", sz);
        } else {
            printf("fail\n");
        }
    }

    if (test_mode->iv) {
        memcpy(iv, (uint8_t *)test_mode->iv, AES_BLOCK_LEN);
        ivp = iv;
    }

    printf("%s dec\n", test_mode->name);
    for (i = 0; i < test_mode->len; i++) {
        printf("\tvector %d: ", i);
        vector = &vectors[i];
        inbuf = (uint8_t *)vector->cipher;

        asksz = AES_BLOCK_LEN;
        if (mode == CRYPTO_MODE_CTR) {
            asksz = vector->sz;
        }

        sz = crypto_decrypt_custom(crypto, algo, mode, key, keylen, ivp,
                inbuf, outbuf, asksz);
        if (sz == asksz && memcmp(outbuf, vector->plain, sz) == 0) {
            printf("ok, sz=%lu\n", sz);
        } else {
            printf("fail\n");
        }
    }
}

#if MYNEWT_VAL(CRYPTOTEST_BENCHMARK)
extern uint8_t aes_128_key[];
extern uint8_t aes_128_ecb_input[];
extern uint8_t aes_128_ecb_expected[];

typedef void (* block_encrypt_func_t)(void *, const uint8_t *, uint8_t *);

static void
crypto_enc_block(void *data, const uint8_t *input, uint8_t *output)
{
    (void)crypto_encrypt_aes_ecb((struct crypto_dev *)data, aes_128_key, 128,
            input, output, AES_BLOCK_LEN);
}

static void
mbed_enc_block(void *data, const uint8_t *input, uint8_t *output)
{
    (void)mbedtls_aes_crypt_ecb((mbedtls_aes_context *)data,
            MBEDTLS_AES_ENCRYPT, input, output);
}

static void
tc_enc_block(void *data, const uint8_t *input, uint8_t *output)
{
    (void)tc_aes_encrypt(output, input, (TCAesKeySched_t)data);
}

static void
run_benchmark(char *name, block_encrypt_func_t encfn, void *data, uint8_t iter)
{
    int i, j;
    uint8_t output[AES_BLOCK_LEN];
    uint16_t blkidx;
    os_time_t t;

    printf("%s - running %d iterations of 4096 block encrypt... ", name, iter);
    t = os_time_get();
    for (i = 0; i < iter; i++) {
        for (blkidx = 0; blkidx < 4096; blkidx += AES_BLOCK_LEN) {
            encfn(data, &aes_128_ecb_input[blkidx], output);
            if (memcmp(output, &aes_128_ecb_expected[blkidx],
                        AES_BLOCK_LEN)) {
                printf("fail... blkidx=%u\n", blkidx);
                for (j = 0; j < AES_BLOCK_LEN; j++) {
                    printf("[%02x]<%02x> ", output[j],
                            aes_128_ecb_expected[blkidx + j]);
                }
                return;
            }
        }
    }
    printf("done in %lu ticks\n", os_time_get() - t);
}
#endif /* MYNEWT_VAL(CRYPTOTEST_BENCHMARK) */

#if MYNEWT_VAL(CRYPTOTEST_CONCURRENCY)
static void
concurrency_test_handler(void *arg)
{
    struct os_task *t;
    struct crypto_dev *crypto = (struct crypto_dev *)arg;
    uint8_t output[AES_BLOCK_LEN];
    uint16_t blkidx;
    uint16_t ok, fail;

    t = os_sched_get_current_task();
    blkidx = ok = fail = 0;
    while (blkidx < 4096) {
        (void)crypto_encrypt_aes_ecb(crypto, aes_128_key, 128,
                &aes_128_ecb_input[blkidx], output, AES_BLOCK_LEN);
        if (memcmp(output, &aes_128_ecb_expected[blkidx], AES_BLOCK_LEN)) {
            fail++;
        } else {
            blkidx += AES_BLOCK_LEN;
            ok++;
        }
        os_time_delay(1);
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
run_concurrency_test(struct crypto_dev *crypto)
{
    os_stack_t *pstack;
    int i;

    printf("\n=== Concurrency [%d tasks] ===\n", TASK_AMOUNT);

    assert(TASK_AMOUNT < 10);
    for (i = 0; i < TASK_AMOUNT; i++) {
        pstack = malloc(sizeof(os_stack_t) * STACK_SIZE);
        assert(pstack);

        sprintf(names[i], "task%c", i + '0');
        os_task_init(&tasks[i], names[i], concurrency_test_handler, crypto,
                8 + i, OS_WAIT_FOREVER, pstack, STACK_SIZE);
    }
}
#endif /* MYNEWT_VAL(CRYPTOTEST_CONCURRENCY) */

#if MYNEWT_VAL(CRYPTOTEST_INPLACE)
struct inplace_test {
    uint16_t mode;
    char *name;
    char *iv;
    char *expected;
};

void
run_inplace_test(struct crypto_dev *crypto)
{
    const char *key = "\x2b\x7e\x15\x16\x28\xae\xd2\xa6\xab\xf7\x15\x88\x09\xcf\x4f\x3c";
    const char *inbuf = "\x6b\xc1\xbe\xe2\x2e\x40\x9f\x96\xe9\x3d\x7e\x11\x73\x93\x17\x2a";
    struct inplace_test data[] = {
        {
            .mode = CRYPTO_MODE_ECB,
            .name = "AES-128-ECB",
            .iv = NULL,
            .expected = "\x3a\xd7\x7b\xb4\x0d\x7a\x36\x60\xa8\x9e\xca\xf3\x24\x66\xef\x97",
        },
        {
            .mode = CRYPTO_MODE_CBC,
            .name = "AES-128-CBC",
            .iv = "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F",
            .expected = "\x76\x49\xab\xac\x81\x19\xb2\x46\xce\xe9\x8e\x9b\x12\xe9\x19\x7d",
        },
        {
            .mode = CRYPTO_MODE_CTR,
            .name = "AES-128-CTR",
            .iv = "\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff",
            .expected = "\x87\x4d\x61\x91\xb6\x20\xe3\x26\x1b\xef\x68\x64\x99\x0d\xb6\xce",
        },
    };
    uint8_t i;
    uint8_t buf[AES_BLOCK_LEN];
    uint8_t ivcopy[AES_BLOCK_LEN];

    for (i = 0; i < ARRAY_SIZE(data); i++) {
        /*
         * assure vars are in RAM, and reinitialized from test data
         */
        memcpy(buf, inbuf, AES_BLOCK_LEN);
        memcpy(ivcopy, data[i].iv, AES_BLOCK_LEN);

        printf("%s enc: ", data[i].name);
        (void)crypto_encrypt_custom(crypto, CRYPTO_ALGO_AES, data[i].mode,
                key, 128, ivcopy, buf, buf, AES_BLOCK_LEN);
        if (memcmp(buf, data[i].expected, AES_BLOCK_LEN) == 0) {
            printf("ok\n");
        } else {
            printf("fail\n");
        }

        memcpy(buf, data[i].expected, AES_BLOCK_LEN);
        memcpy(ivcopy, data[i].iv, AES_BLOCK_LEN);

        printf("%s dec: ", data[i].name);
        (void)crypto_decrypt_custom(crypto, CRYPTO_ALGO_AES, data[i].mode,
                key, 128, ivcopy, buf, buf, AES_BLOCK_LEN);
        if (memcmp(buf, inbuf, AES_BLOCK_LEN) == 0) {
            printf("ok\n");
        } else {
            printf("fail\n");
        }
    }
}
#endif /* MYNEWT_VAL(CRYPTOTEST_INPLACE) */

#if MYNEWT_VAL(CRYPTOTEST_IOVEC)
struct iov_data_block {
    char *plain;
    char *cipher;
    uint32_t len;
};

struct iov_data {
    uint16_t mode;
    char *name;
    char *key;
    char *iv;
    uint32_t iovlen;
    struct iov_data_block iov[];
};

static struct iov_data ecb_iovd = {
    .mode = CRYPTO_MODE_ECB,
    .name = "AES-128-ECB",
    .key = "\x2b\x7e\x15\x16\x28\xae\xd2\xa6\xab\xf7\x15\x88\x09\xcf\x4f\x3c",
    .iv = NULL,
    .iovlen = 3,
    .iov = {
        {
            .plain = "\x6b\xc1\xbe\xe2\x2e\x40\x9f\x96\xe9\x3d\x7e\x11\x73\x93\x17\x2a",
            .cipher = "\x3a\xd7\x7b\xb4\x0d\x7a\x36\x60\xa8\x9e\xca\xf3\x24\x66\xef\x97",
            .len = AES_BLOCK_LEN,
        },
        {
            .plain = "\xae\x2d\x8a\x57\x1e\x03\xac\x9c\x9e\xb7\x6f\xac\x45\xaf\x8e\x51",
            .cipher = "\xf5\xd3\xd5\x85\x03\xb9\x69\x9d\xe7\x85\x89\x5a\x96\xfd\xba\xaf",
            .len = AES_BLOCK_LEN,
        },
        {
            .plain = "\x30\xc8\x1c\x46\xa3\x5c\xe4\x11\xe5\xfb\xc1\x19\x1a\x0a\x52\xef"
                     "\xf6\x9f\x24\x45\xdf\x4f\x9b\x17\xad\x2b\x41\x7b\xe6\x6c\x37\x10",
            .cipher = "\x43\xb1\xcd\x7f\x59\x8e\xce\x23\x88\x1b\x00\xe3\xed\x03\x06\x88"
                      "\x7b\x0c\x78\x5e\x27\xe8\xad\x3f\x82\x23\x20\x71\x04\x72\x5d\xd4",
            .len = AES_BLOCK_LEN * 2,
        },
    },
};

static struct iov_data cbc_iovd = {
    .mode = CRYPTO_MODE_CBC,
    .name = "AES-128-CBC",
    .key = "\x2b\x7e\x15\x16\x28\xae\xd2\xa6\xab\xf7\x15\x88\x09\xcf\x4f\x3c",
    .iv = "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F",
    .iovlen = 3,
    .iov = {
        {
            .plain = "\x6b\xc1\xbe\xe2\x2e\x40\x9f\x96\xe9\x3d\x7e\x11\x73\x93\x17\x2a",
            .cipher = "\x76\x49\xab\xac\x81\x19\xb2\x46\xce\xe9\x8e\x9b\x12\xe9\x19\x7d",
            .len = AES_BLOCK_LEN,
        },
        {
            .plain = "\xae\x2d\x8a\x57\x1e\x03\xac\x9c\x9e\xb7\x6f\xac\x45\xaf\x8e\x51"
                     "\x30\xc8\x1c\x46\xa3\x5c\xe4\x11\xe5\xfb\xc1\x19\x1a\x0a\x52\xef",
            .cipher = "\x50\x86\xcb\x9b\x50\x72\x19\xee\x95\xdb\x11\x3a\x91\x76\x78\xb2"
                      "\x73\xbe\xd6\xb8\xe3\xc1\x74\x3b\x71\x16\xe6\x9e\x22\x22\x95\x16",
            .len = AES_BLOCK_LEN * 2,
        },
        {
            .plain = "\xf6\x9f\x24\x45\xdf\x4f\x9b\x17\xad\x2b\x41\x7b\xe6\x6c\x37\x10",
            .cipher = "\x3f\xf1\xca\xa1\x68\x1f\xac\x09\x12\x0e\xca\x30\x75\x86\xe1\xa7",
            .len = AES_BLOCK_LEN,
        },
    },
};

static struct iov_data ctr_iovd = {
    .mode = CRYPTO_MODE_CTR,
    .name = "AES-128-CTR",
    .key = "\x2b\x7e\x15\x16\x28\xae\xd2\xa6\xab\xf7\x15\x88\x09\xcf\x4f\x3c",
    .iv = "\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff",
    .iovlen = 3,
    .iov = {
        {
            .plain = "\x6b\xc1\xbe\xe2\x2e\x40\x9f\x96\xe9\x3d\x7e\x11\x73\x93\x17\x2a"
                     "\xae\x2d\x8a\x57\x1e\x03\xac\x9c\x9e\xb7\x6f\xac\x45\xaf\x8e\x51",
            .cipher = "\x87\x4d\x61\x91\xb6\x20\xe3\x26\x1b\xef\x68\x64\x99\x0d\xb6\xce"
                      "\x98\x06\xf6\x6b\x79\x70\xfd\xff\x86\x17\x18\x7b\xb9\xff\xfd\xff",
            .len = AES_BLOCK_LEN * 2,
        },
        {
            .plain = "\x30\xc8\x1c\x46\xa3\x5c\xe4\x11\xe5\xfb\xc1\x19\x1a\x0a\x52\xef",
            .cipher = "\x5a\xe4\xdf\x3e\xdb\xd5\xd3\x5e\x5b\x4f\x09\x02\x0d\xb0\x3e\xab",
            .len = AES_BLOCK_LEN,
        },
        {
            .plain = "\xf6\x9f\x24\x45\xdf\x4f\x9b\x17\xad\x2b\x41\x7b\xe6\x6c\x37\x10",
            .cipher = "\x1e\x03\x1d\xda\x2f\xbe\x03\xd1\x79\x21\x70\xa0\xf3\x00\x9c\xee",
            .len = AES_BLOCK_LEN,
        },
    },
};

static struct iov_data *all_iovd[] = {
    &ecb_iovd,
    &cbc_iovd,
    &ctr_iovd,
    NULL,
};

void
run_iovec_test(struct crypto_dev *crypto)
{
    struct crypto_iovec *iov;
    struct iov_data *iovp;
    uint8_t i;
    uint8_t j;
    uint32_t len;
    uint32_t total;
    uint8_t iv[AES_BLOCK_LEN];
    bool fail;

    for (j = 0; all_iovd[j] != NULL; j++) {
        iovp = all_iovd[j];

        printf("iov %s enc: ", iovp->name);

        iov = malloc(sizeof(struct crypto_iovec) * iovp->iovlen);
        assert(iov);

        memcpy(iv, iovp->iv, AES_BLOCK_LEN);
        for (i = 0; i < iovp->iovlen; i++) {
            iov[i].iov_base = malloc(iovp->iov[i].len);
            assert(iov[i].iov_base);
            memcpy(iov[i].iov_base, iovp->iov[i].plain, iovp->iov[i].len);
            iov[i].iov_len = iovp->iov[i].len;
        }

        fail = false;
        len = 0;
        total = crypto_encryptv_custom(crypto, CRYPTO_ALGO_AES, iovp->mode,
                iovp->key, 128, iv, iov, iovp->iovlen);
        for (i = 0; i < iovp->iovlen; i++) {
            if (memcmp(iovp->iov[i].cipher, iov[i].iov_base, iov[i].iov_len) != 0) {
                fail = true;
                break;
            }
            len += iov[i].iov_len;
        }

        if (fail || total != len) {
            printf("fail\n");
        } else {
            printf("ok\n");
        }

        /* Free and realloc again in case of HW is not able to encrypt */
        for (i = 0; i < iovp->iovlen; i++) {
            free(iov[i].iov_base);
        }

        printf("iov %s dec: ", iovp->name);

        memcpy(iv, iovp->iv, AES_BLOCK_LEN);
        for (i = 0; i < iovp->iovlen; i++) {
            iov[i].iov_base = malloc(iovp->iov[i].len);
            assert(iov[i].iov_base);
            memcpy(iov[i].iov_base, iovp->iov[i].cipher, iovp->iov[i].len);
            iov[i].iov_len = iovp->iov[i].len;
        }

        fail = false;
        len = 0;
        total = crypto_decryptv_custom(crypto, CRYPTO_ALGO_AES, iovp->mode,
                iovp->key, 128, iv, iov, iovp->iovlen);
        for (i = 0; i < iovp->iovlen; i++) {
            if (memcmp(iovp->iov[i].plain, iov[i].iov_base, iov[i].iov_len) != 0) {
                fail = true;
                break;
            }
            len += iov[i].iov_len;
        }

        if (fail || total != len) {
            printf("fail\n");
        } else {
            printf("ok\n");
        }

        for (i = 0; i < iovp->iovlen; i++) {
            free(iov[i].iov_base);
        }
    }
}
#endif /* MYNEWT_VAL(CRYPTOTEST_IOVEC) */

int
main(void)
{
    struct crypto_dev *crypto;
#if MYNEWT_VAL(CRYPTOTEST_BENCHMARK)
    mbedtls_aes_context mbed_aes;
    struct tc_aes_key_sched_struct tc_aes;
    int iterations;
#endif
    int i;

    sysinit();

    crypto = (struct crypto_dev *) os_dev_open("crypto", OS_TIMEOUT_NEVER, NULL);
    assert(crypto);

    printf("=== Test vectors ===\n");
    for (i = 0; all_tests[i] != NULL; i++) {
        run_test_vectors(crypto, all_tests[i]);
    }

#if MYNEWT_VAL(CRYPTOTEST_INPLACE)
    printf("\n=== In-place encrypt/decrypt ===\n");
    run_inplace_test(crypto);
#endif

#if MYNEWT_VAL(CRYPTOTEST_IOVEC)
    printf("\n=== iovec encrypt/decrypt ===\n");
    run_iovec_test(crypto);
#endif

#if MYNEWT_VAL(CRYPTOTEST_BENCHMARK)
    mbedtls_aes_init(&mbed_aes);
    mbedtls_aes_setkey_enc(&mbed_aes, aes_128_key, 128);

    tc_aes128_set_encrypt_key(&tc_aes, aes_128_key);

    iterations = 30;
    for (i = 1; i <= 3; i++) {
        printf("\n=== Benchmarks - iteration %d ===\n", i);
        run_benchmark("CRYPTO", crypto_enc_block, crypto, iterations);
        run_benchmark("MBEDTLS", mbed_enc_block, &mbed_aes, iterations);
        run_benchmark("TINYCRYPT", tc_enc_block, &tc_aes, iterations);
        os_time_delay(OS_TICKS_PER_SEC);
    }
#endif

#if MYNEWT_VAL(CRYPTOTEST_CONCURRENCY)
    run_concurrency_test(crypto);
#endif

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }

    return 0;
}
