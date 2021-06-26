/*
 * Copyright (c) 2020 Siddharth Chandrasekaran <siddharth@embedjournal.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ctype.h>

#include <crc/crc16.h>
#include "modlog/modlog.h"

#include <os/os_time.h>

#include "tinycrypt/cbc_mode.h"
#include "tinycrypt/aes.h"
#include "tinycrypt/constants.h"
#if MYNEWT_VAL(TRNG) && !MYNEWT_VAL(OSDP_USE_CRYPTO_HOOK)
#include "trng/trng.h"
#endif /* MYNEWT_VAL(TRNG) && !MYNEWT_VAL(OSDP_USE_CRYPTO_HOOK) */

#include "osdp/osdp_common.h"
#include "osdp/osdp_hooks.h"

/*
 * Set this to size of the max UART buffer length and add block size
 * If the entire buffer were to be encrypted, account for the
 * the fact that the output buffer comes with IV appended
 */
#define MAX_UART_BUF (max(MYNEWT_VAL(OSDP_UART_TX_BUFFER_LENGTH), \
                          MYNEWT_VAL(OSDP_UART_RX_BUFFER_LENGTH)))
#define MAX_SCRATCH_LEN (MAX_UART_BUF + TC_AES_BLOCK_SIZE)
static uint8_t scratch_buf1[MAX_SCRATCH_LEN];
#if MYNEWT_VAL(TRNG) && !MYNEWT_VAL(OSDP_USE_CRYPTO_HOOK)
static struct trng_dev *trng = NULL;
#endif /* MYNEWT_VAL(TRNG) && !MYNEWT_VAL(OSDP_USE_CRYPTO_HOOK) */

#define OSDP_LOCK_TMO \
    (OS_TICKS_PER_SEC * MYNEWT_VAL(OSDP_DEVICE_LOCK_TIMEOUT_MS)/1000 + 1)

#if 0
/* see macro in osdp_common.h */
void
osdp_dump(const char *head, uint8_t *buf, int len)
{

}
#endif

uint16_t
osdp_compute_crc16(const uint8_t *buf, size_t len)
{
    return crc16_ccitt(0x1D0F, buf, len);
}

int64_t
osdp_millis_now(void)
{
    return (os_get_uptime_usec() / 1000);
}

int64_t
osdp_millis_since(int64_t last)
{
    return osdp_millis_now() - last;
}

void
osdp_encrypt(uint8_t *key, uint8_t *iv, uint8_t *data, int len)
{
    struct tc_aes_key_sched_struct s;
    (void)tc_aes128_set_encrypt_key(&s, key);

    if (iv != NULL) {
        /* Check if there is enough space for IV + len */
        assert(len <= MAX_SCRATCH_LEN - TC_AES_BLOCK_SIZE);
        if (tc_cbc_mode_encrypt(scratch_buf1, /* out: output buffer */
            len + TC_AES_BLOCK_SIZE, /* outlen: max output size, including size of IV */
            data, /* in: input buffer */
            len, /* inlen: input len */
            iv, /* iv: IV start */
            &s) == TC_CRYPTO_FAIL) {
            OSDP_LOG_ERROR("osdp: sc: CBC ENCRYPT - Failed");
        }

        /* copy ciphertext from offset */
        memcpy(data, scratch_buf1 + TC_AES_BLOCK_SIZE, len);

    } else {
        if (tc_aes_encrypt(data, data, &s) == TC_CRYPTO_FAIL) {
            OSDP_LOG_ERROR("osdp: sc: ECB ENCRYPT - Failed\n");
        }
    }
}

void
osdp_decrypt(uint8_t *key, uint8_t *iv, uint8_t *data, int len)
{
    struct tc_aes_key_sched_struct s;
    (void)tc_aes128_set_decrypt_key(&s, key);

    if (iv != NULL) {
        /* Check if there is enough space for IV + len */
        assert(len <= MAX_SCRATCH_LEN - TC_AES_BLOCK_SIZE);
        /* Create input buffer. IV and cipher text in contiguous buffer per tinycrypt */
        memcpy(scratch_buf1, iv, TC_AES_BLOCK_SIZE);
        memcpy(scratch_buf1 + TC_AES_BLOCK_SIZE, data, len);
        /*
           note commit da923ca upstream(tinycrypt)
           Commit message has this nugget:-
           When decrypting with CBC mode, the in and out buffers should be the same size.
           Even though the IV and ciphertext are contiguous, the in buffer points to the first byte of the ciphertext.
         */
        if (tc_cbc_mode_decrypt(scratch_buf1, /* out: output buffer */
            len + TC_AES_BLOCK_SIZE, /* outlen: same as inlen */
            scratch_buf1 + TC_AES_BLOCK_SIZE, /* in: offset to ciphertext in input buffer */
            len + TC_AES_BLOCK_SIZE, /* inlen: total length of input, iv + buffer length */
            scratch_buf1, /* iv: offset to IV in input buffer */
            &s) == TC_CRYPTO_FAIL) {
            OSDP_LOG_ERROR("osdp: sc: CBC DECRYPT - Failed\n");
        }
        /* Copy decrypted data into output buffer */
        memcpy(data, scratch_buf1, len);
    } else {
        if (tc_aes_decrypt(data, data, &s) == TC_CRYPTO_FAIL) {
            OSDP_LOG_ERROR("osdp: sc: ECB DECRYPT - Failed\n");
        }
    }
}

void
osdp_get_rand(uint8_t *buf, int len)
{
    size_t ret = 0;

#if MYNEWT_VAL(OSDP_USE_CRYPTO_HOOK)
    ret = osdp_hook_crypto_random_bytes(buf, len);
#elif MYNEWT_VAL(TRNG)
    trng = (struct trng_dev *) os_dev_open("trng", OS_WAIT_FOREVER, NULL);
    if (trng == NULL) {
        OSDP_LOG_ERROR("osdp: sc: Could not open TRNG\n");
        buf = NULL;
    }

    ret = trng_read(trng, buf, len);
    while (ret < len) {
        os_sched(NULL);
        ret += trng_read(trng, &buf[ret], len - ret);
    }

    os_dev_close((struct os_dev *)trng);
#endif

    if (ret == 0) {
        buf = NULL;
    }
}

uint32_t
osdp_get_sc_status_mask(osdp_t *ctx)
{
    int i;
    uint32_t mask = 0;
    struct osdp_pd *pd;

    assert(ctx);

    if (ISSET_FLAG(TO_OSDP(ctx), FLAG_CP_MODE)) {
        for (i = 0; i < NUM_PD(ctx); i++) {
            pd = TO_PD(ctx, i);
            if (pd->state == OSDP_CP_STATE_ONLINE &&
                ISSET_FLAG(pd, PD_FLAG_SC_ACTIVE)) {
                mask |= 1 << i;
            }
        }
    } else {
        pd = TO_PD(ctx, 0);
        if (ISSET_FLAG(pd, PD_FLAG_SC_ACTIVE)) {
            mask = 1;
        }
    }

    return mask;
}

uint32_t
osdp_get_status_mask(osdp_t *ctx)
{
    int i;
    uint32_t mask = 0;
    struct osdp_pd *pd;

    assert(ctx);

    if (ISSET_FLAG(TO_OSDP(ctx), FLAG_CP_MODE)) {
        for (i = 0; i < TO_OSDP(ctx)->cp->num_pd; i++) {
            pd = TO_PD(ctx, i);
            if (pd->state == OSDP_CP_STATE_ONLINE) {
                mask |= 1 << i;
            }
        }
    } else {
        /* PD state is either online or offline based on whether it got polled from CP */
        pd = TO_PD(ctx, 0);
        if (ISSET_FLAG(pd, PD_FLAG_CP_POLL_ACTIVE)) {
            mask = 1;
        }
    }

    return mask;
}

/**
 * Lock access to event/command resource.  Blocks until configured timeout.
 *
 * @param The peripheral device to lock.
 *
 * @return 0 on success, non-zero on failure.
 */
int
osdp_device_lock(struct os_mutex *lock)
{
    int rc;

    rc = os_mutex_pend(lock, OSDP_LOCK_TMO);
    if (rc == 0 || rc == OS_NOT_STARTED) {
        return (0);
    }
    return (rc);
}

/**
 * Unlock access to the event/command resource.
 *
 * @param The peripheral device to unlock access to.
 */
void
osdp_device_unlock(struct os_mutex *lock)
{
    os_mutex_release(lock);
}

