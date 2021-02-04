/*
 * Copyright (c) 2016, Freescale Semiconductor, Inc.
 * Copyright 2017, 2020 NXP
 * All rights reserved.
 *
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef _FSL_LTC_DPA_H_
#define _FSL_LTC_DPA_H_

#include "fsl_common.h"

/*!
 * @internal @addtogroup ltc_driver_dpa
 * @{
 */

/*******************************************************************************
 * Definitions
 ******************************************************************************/
/* Component ID definition, used by tools. */
#ifndef FSL_COMPONENT_ID
#define FSL_COMPONENT_ID "platform.drivers.ltc_dpa"
#endif

/*! @name Driver version */
/*@{*/
/*! @brief LTC DPA driver version. Version 2.0.8. */
#define FSL_LTC_DPA_DRIVER_VERSION (MAKE_VERSION(2, 0, 8))
/*@}*/

/*! AES block size in bytes */
#define LTC_DPA_AES_BLOCK_SIZE 16

/*! AES Input Vector size in bytes */
#define LTC_DPA_AES_IV_SIZE 16

/*! LTC DES block size in bytes */
#define LTC_DPA_DES_BLOCK_SIZE 8
/*! @brief LTC DES key size - 64 bits. */
#define LTC_DPA_DES_KEY_SIZE 8

/*! @brief LTC DES IV size - 8 bytes */
#define LTC_DPA_DES_IV_SIZE 8

/*!
 * @brief User request details, also updated during processing.
 */
typedef uint32_t ltc_dpa_request_t[250];

/*!
 * @brief Algorithm supported by LTC_CMAC_DPA APIs.
 */
typedef enum _ltc_dpa_hash_algo
{
    kLTC_XcbcMacDPA = 0u,
    kLTC_CmacDPA,
} ltc_dpa_hash_algo_t;

/*!
 * @brief Handle for LTC DPA APIs.
 */
typedef struct _ltc_dpa_handle
{
    ltc_dpa_request_t req; /*!< Driver internals derived from user request and updated during request processing */
} ltc_dpa_handle_t;

/*******************************************************************************
 * API
 ******************************************************************************/

#if defined(__cplusplus)
extern "C" {
#endif

/*!
 * @brief Enable clock to LTC module.
 * This function enables clock to the LTC module.
 * @param base LTC peripheral base address
 */
void LTC_InitDPA(LTC_Type *base);

#if defined(FSL_FEATURE_LTC_HAS_DPAMS) && FSL_FEATURE_LTC_HAS_DPAMS
/*!
 * @brief Sets the DPA Mask Seed register.
 *
 * The DPA Mask Seed register reseeds the mask that provides resistance against DPA (differential power analysis)
 * attacks on AES or DES keys.
 *
 * Differential Power Analysis Mask (DPA) resistance uses a randomly changing mask that introduces
 * "noise" into the power consumed by the AES or DES. This reduces the signal-to-noise ratio that differential
 * power analysis attacks use to "guess" bits of the key. This randomly changing mask should be
 * seeded at POR, and continues to provide DPA resistance from that point on. However, to provide even more
 * DPA protection it is recommended that the DPA mask be reseeded after every 50,000 blocks have
 * been processed. At that time, software can opt to write a new seed (preferably obtained from an RNG)
 * into the DPA Mask Seed register (DPAMS), or software can opt to provide the new seed earlier or
 * later, or not at all. DPA resistance continues even if the DPA mask is never reseeded.
 *
 * @param base LTC peripheral base address
 * @param mask The DPA mask seed.
 */
void LTC_SetDpaMaskSeedDPA(LTC_Type *base, uint32_t mask);
#endif /* FSL_FEATURE_LTC_HAS_DPAMS */

/*!
 * @brief Init the LTC DPA handle which is used in transactional functions
 *
 * This function creates internal context for the LTC DPA functions.
 * This function also initializes LTC DPA Mask Seed DPAMS register with a 32-bit word,
 * where the word is derived from the seed[].
 *
 * @param base      LTC module base address
 * @param[in,out] handle    Pointer to ltc_dpa_handle_t structure
 * @param seed Pointer to 128-bit entropy input.
 * @return kStatus_Success is successfull or kStatus_InvalidArgument if error.
 */
status_t LTC_CreateHandleDPA(LTC_Type *base, ltc_dpa_handle_t *handle, uint8_t seed[16]);

/*!
 * @brief Set key for LTC DPA AES encryption.
 *
 * This function sets key for usage with LTC DPA AES functions.
 *
 * @param base      LTC module base address
 * @param[in,out] handle    Pointer to ltc_dpa_handle_t structure
 * @param key Input key
 * @param keySize Size of input key in bytes
 * @return Status.
 */
status_t LTC_AES_SetKeyDPA(LTC_Type *base, ltc_dpa_handle_t *handle, const uint8_t *key, size_t keySize);

/*!
 * @brief Enables the Idle time PRNG buffer.
 * This feature is intended to enable pseudo-random numbers generation
 * during system idle time. These idle time pre-generated numbers are simply consumed
 * by LTC_DPA encrypt/decrypt functions.
 *
 * @param[in,out] handle    Pointer to ltc_dpa_handle_t structure
 * @param prngBuffer        Memory buffer to be used as storage for random 32-bit words.
 * @param prngBufferArraySize  Number of items (array size) in the prngBuffer[] array.
 */
status_t LTC_IdleTimePrngBufferEnableDPA(ltc_dpa_handle_t *handle, uint32_t *prngBuffer, size_t prngBufferArraySize);

/*!
 * @brief Runs PRNG algorithm and puts the 32-bit random word to associated memory buffer.
 *
 * This function is intended and designed to be used during system idle time
 * to generate random numbers, to be consumed by LTC_DPA encrypt/decrypt APIs.
 *
 * Example usage FreeRTOS Idle Time Hook:
 * @code
 * static uint32_t s_prngBuffer[2048];
 * LTC_IdleTimePrngBufferEnableDPA(g_ltcDpaHandle, s_prngBuffer, 2048);
 * ...
 * void vApplicationIdleHook(void)
 * {
 *     if (kStatus_Success != LTC_IdleTimePrngBufferPutNextDPA(g_ltcDpaHandle)
 *     {
 *         __WFI();
 *     }
 * }
 * @endcode
 *
 * Example usage in Super Loop (bare metal):
 * @code
 * static uint32_t s_prngBuffer[2048];
 * LTC_IdleTimePrngBufferEnableDPA(g_ltcDpaHandle, s_prngBuffer, 2048);
 * ...
 * while (1)
 * {
 *     ...
 *     ...
 *     ...
 *     if (kStatus_Success != LTC_IdleTimePrngBufferPutNextDPA(g_ltcDpaHandle)
 *     {
 *         __WFI();
 *     }
 * }
 * @endcode
 *
 * @param[in,out] handle    Pointer to ltc_dpa_handle_t structure
 * @return kStatus_Success Sucesfully generated next random number. prngBuffer[] is not full yet.
 * @return kStatus_LTC_PrngRingBufferFull prngBuffer[] is full.
 * @return kStatus_InvalidArgument prngBuffer[] is not configured. Use LTC_IdleTimePrngBufferEnableDPA().
 */
status_t LTC_IdleTimePrngBufferPutNextDPA(ltc_dpa_handle_t *handle);

/*!
 * @internal @addtogroup ltc_driver_aes_with_dpa
 * @{
 */

/*!
 * @brief Encrypts AES using the ECB block mode.
 *
 * Encrypts AES using the ECB block mode.
 *
 * @param base LTC peripheral base address
 * @param handle pointer to ltc_dpa_handle_t structure which stores the transaction state.
 * @param plaintext Input plain text to encrypt
 * @param[out] ciphertext Output cipher text
 * @param size Size of input and output data in bytes. Must be multiple of 16 bytes.
 * @return Status from encrypt operation
 */
status_t LTC_AES_EncryptEcbDPA(
    LTC_Type *base, ltc_dpa_handle_t *handle, const uint8_t *plaintext, uint8_t *ciphertext, size_t size);

/*!
 * @brief Decrypts AES using ECB block mode.
 *
 * Decrypts AES using ECB block mode.
 *
 * @param base LTC peripheral base address
 * @param handle pointer to ltc_dpa_handle_t structure which stores the transaction state.
 * @param ciphertext Input cipher text to decrypt
 * @param[out] plaintext Output plain text
 * @param size Size of input and output data in bytes. Must be multiple of 16 bytes.
 * @return Status from decrypt operation
 */
status_t LTC_AES_DecryptEcbDPA(
    LTC_Type *base, ltc_dpa_handle_t *handle, const uint8_t *ciphertext, uint8_t *plaintext, size_t size);

/*!
 * @brief Encrypts AES using CBC block mode.
 *
 * @param base LTC peripheral base address
 * @param handle pointer to ltc_dpa_handle_t structure which stores the transaction state.
 * @param plaintext Input plain text to encrypt
 * @param[out] ciphertext Output cipher text
 * @param size Size of input and output data in bytes. Must be multiple of 16 bytes.
 * @param iv Input initial vector to combine with the first input block.
 * @return Status from encrypt operation
 */
status_t LTC_AES_EncryptCbcDPA(LTC_Type *base,
                               ltc_dpa_handle_t *handle,
                               const uint8_t *plaintext,
                               uint8_t *ciphertext,
                               size_t size,
                               const uint8_t iv[LTC_DPA_AES_IV_SIZE]);

/*!
 * @brief Decrypts AES using CBC block mode.
 *
 * @param base LTC peripheral base address
 * @param handle pointer to ltc_dpa_handle_t structure which stores the transaction state.
 * @param ciphertext Input cipher text to decrypt
 * @param[out] plaintext Output plain text
 * @param size Size of input and output data in bytes. Must be multiple of 16 bytes.
 * @param iv Input initial vector to combine with the first input block.
 * @return Status from decrypt operation
 */
status_t LTC_AES_DecryptCbcDPA(LTC_Type *base,
                               ltc_dpa_handle_t *handle,
                               const uint8_t *ciphertext,
                               uint8_t *plaintext,
                               size_t size,
                               const uint8_t iv[LTC_DPA_AES_IV_SIZE]);

/*!
 * @brief Encrypts or decrypts AES using CTR block mode.
 *
 * Encrypts or decrypts AES using CTR block mode.
 * AES CTR mode uses only forward AES cipher and same algorithm for encryption and decryption.
 * The only difference between encryption and decryption is that, for encryption, the input argument
 * is plain text and the output argument is cipher text. For decryption, the input argument is cipher text
 * and the output argument is plain text.
 *
 * @param base LTC peripheral base address
 * @param handle pointer to ltc_dpa_handle_t structure which stores the transaction state.
 * @param input Input data for CTR block mode
 * @param[out] output Output data for CTR block mode
 * @param size Size of input and output data in bytes
 * @param[in,out] counter Input counter (updates on return)
 * @param[out] counterlast Output cipher of last counter, for chained CTR calls. NULL can be passed if chained calls are
 * not used.
 * @param[out] szLeft Output number of bytes in left unused in counterlast block. NULL can be passed if chained calls
 * are not used.
 * @return Status from encrypt operation
 */
status_t LTC_AES_CryptCtrDPA(LTC_Type *base,
                             ltc_dpa_handle_t *handle,
                             const uint8_t *input,
                             uint8_t *output,
                             size_t size,
                             uint8_t counter[LTC_DPA_AES_BLOCK_SIZE],
                             uint8_t counterlast[LTC_DPA_AES_BLOCK_SIZE],
                             size_t *szLeft);

#if defined(FSL_FEATURE_LTC_HAS_GCM) && FSL_FEATURE_LTC_HAS_GCM
/*!
 * @brief Encrypts AES and tags using GCM block mode.
 *
 * Encrypts AES and optionally tags using GCM block mode. If plaintext is NULL, only the GHASH is calculated and output
 * in the 'tag' field.
 *
 * @param base LTC peripheral base address
 * @param handle pointer to ltc_dpa_handle_t structure which stores the transaction state.
 * @param plaintext Input plain text to encrypt
 * @param[out] ciphertext Output cipher text.
 * @param size Size of input and output data in bytes
 * @param iv Input initial vector
 * @param ivSize Size of the IV
 * @param aad Input additional authentication data
 * @param aadSize Input size in bytes of AAD
 * @param[out] tag Output hash tag. Set to NULL to skip tag processing.
 * @param tagSize Input size of the tag to generate, in bytes. Must be 4,8,12,13,14,15 or 16.
 * @return Status from encrypt operation
 */
status_t LTC_AES_EncryptTagGcmDPA(LTC_Type *base,
                                  ltc_dpa_handle_t *handle,
                                  const uint8_t *plaintext,
                                  uint8_t *ciphertext,
                                  size_t size,
                                  const uint8_t *iv,
                                  size_t ivSize,
                                  const uint8_t *aad,
                                  size_t aadSize,
                                  uint8_t *tag,
                                  size_t tagSize);

/*!
 * @brief Decrypts AES and authenticates using GCM block mode.
 *
 * Decrypts AES and optionally authenticates using GCM block mode. If ciphertext is NULL, only the GHASH is calculated
 * and compared with the received GHASH in 'tag' field.
 *
 * @param base LTC peripheral base address
 * @param handle pointer to ltc_dpa_handle_t structure which stores the transaction state.
 * @param ciphertext Input cipher text to decrypt
 * @param[out] plaintext Output plain text.
 * @param size Size of input and output data in bytes
 * @param iv Input initial vector
 * @param ivSize Size of the IV
 * @param aad Input additional authentication data
 * @param aadSize Input size in bytes of AAD
 * @param tag Input hash tag to compare. Set to NULL to skip tag processing.
 * @param tagSize Input size of the tag, in bytes. Must be 4, 8, 12, 13, 14, 15, or 16.
 * @return Status from decrypt operation
 */
status_t LTC_AES_DecryptTagGcmDPA(LTC_Type *base,
                                  ltc_dpa_handle_t *handle,
                                  const uint8_t *ciphertext,
                                  uint8_t *plaintext,
                                  size_t size,
                                  const uint8_t *iv,
                                  size_t ivSize,
                                  const uint8_t *aad,
                                  size_t aadSize,
                                  const uint8_t *tag,
                                  size_t tagSize);
#endif /* FSL_FEATURE_LTC_HAS_GCM */

/*!
 * @brief Encrypts AES and tags using CCM block mode.
 *
 * Encrypts AES and optionally tags using CCM block mode.
 *
 * @param base LTC peripheral base address
 * @param handle pointer to ltc_dpa_handle_t structure which stores the transaction state.
 * @param plaintext Input plain text to encrypt
 * @param[out] ciphertext Output cipher text.
 * @param size Size of input and output data in bytes. Zero means authentication only.
 * @param iv Nonce
 * @param ivSize Length of the Nonce in bytes. Must be 7, 8, 9, 10, 11, 12, or 13.
 * @param aad Input additional authentication data. Can be NULL if aadSize is zero.
 * @param aadSize Input size in bytes of AAD. Zero means data mode only (authentication skipped).
 * @param[out] tag Generated output tag. Set to NULL to skip tag processing.
 * @param tagSize Input size of the tag to generate, in bytes. Must be 4, 6, 8, 10, 12, 14, or 16.
 * @return Status from encrypt operation
 */
status_t LTC_AES_EncryptTagCcmDPA(LTC_Type *base,
                                  ltc_dpa_handle_t *handle,
                                  const uint8_t *plaintext,
                                  uint8_t *ciphertext,
                                  size_t size,
                                  const uint8_t *iv,
                                  size_t ivSize,
                                  const uint8_t *aad,
                                  size_t aadSize,
                                  uint8_t *tag,
                                  size_t tagSize);

/*!
 * @brief Decrypts AES and authenticates using CCM block mode.
 *
 * Decrypts AES and optionally authenticates using CCM block mode.
 *
 * @param base LTC peripheral base address
 * @param handle pointer to ltc_dpa_handle_t structure which stores the transaction state.
 * @param ciphertext Input cipher text to decrypt
 * @param[out] plaintext Output plain text.
 * @param size Size of input and output data in bytes. Zero means authentication only.
 * @param iv Nonce
 * @param ivSize Length of the Nonce in bytes. Must be 7, 8, 9, 10, 11, 12, or 13.
 * @param aad Input additional authentication data. Can be NULL if aadSize is zero.
 * @param aadSize Input size in bytes of AAD. Zero means data mode only (authentication skipped).
 * @param tag Received tag. Set to NULL to skip tag processing.
 * @param tagSize Input size of the received tag to compare with the computed tag, in bytes. Must be 4, 6, 8, 10, 12,
 * 14, or 16.
 * @return Status from decrypt operation
 */
status_t LTC_AES_DecryptTagCcmDPA(LTC_Type *base,
                                  ltc_dpa_handle_t *handle,
                                  const uint8_t *ciphertext,
                                  uint8_t *plaintext,
                                  size_t size,
                                  const uint8_t *iv,
                                  size_t ivSize,
                                  const uint8_t *aad,
                                  size_t aadSize,
                                  const uint8_t *tag,
                                  size_t tagSize);

/*!
 *@}
 */ /* end of ltc_driver_aes_with_dpa */

/*!
 * @internal @addtogroup ltc_driver_des_with_dpa
 * @{
 */

/*!
 * @brief Set key for LTC DPA DES operations.
 *
 * This function sets key for usage with LTC DPA DES functions.
 *
 * @param base      LTC module base address
 * @param[in,out] handle    Pointer to ltc_dpa_handle_t structure
 * @param key1 The 1st 8-byte key.
 * @param key2 The 2nd 8-byte key. Can be NULL if running simple DES.
 * @param key3 The 3rd 8-byte key. Can be NULL if running 3DES with 2 keys (16-byte key).
 * @return Status.
 */
status_t LTC_DES_SetKeyDPA(LTC_Type *base,
                           ltc_dpa_handle_t *handle,
                           const uint8_t key1[LTC_DPA_DES_KEY_SIZE],
                           const uint8_t key2[LTC_DPA_DES_KEY_SIZE],
                           const uint8_t key3[LTC_DPA_DES_KEY_SIZE]);

/*!
 * @brief Encrypts DES using ECB block mode.
 *
 * Encrypts DES using ECB block mode.
 *
 * @param base LTC peripheral base address
 * @param handle pointer to ltc_dpa_handle_t structure which stores the transaction state.
 * @param plaintext Input plaintext to encrypt
 * @param[out] ciphertext Output ciphertext
 * @param size Size of input and output data in bytes. Must be multiple of 8 bytes.
 * @return Status from encrypt/decrypt operation
 */
status_t LTC_DES_EncryptEcbDPA(
    LTC_Type *base, ltc_dpa_handle_t *handle, const uint8_t *plaintext, uint8_t *ciphertext, size_t size);

/*!
 * @brief Decrypts DES using ECB block mode.
 *
 * Decrypts DES using ECB block mode.
 *
 * @param base LTC peripheral base address
 * @param handle pointer to ltc_dpa_handle_t structure which stores the transaction state.
 * @param ciphertext Input ciphertext to decrypt
 * @param[out] plaintext Output plaintext
 * @param size Size of input and output data in bytes. Must be multiple of 8 bytes.
 * @return Status from encrypt/decrypt operation
 */
status_t LTC_DES_DecryptEcbDPA(
    LTC_Type *base, ltc_dpa_handle_t *handle, const uint8_t *ciphertext, uint8_t *plaintext, size_t size);

/*!
 * @brief Encrypts DES using CBC block mode.
 *
 * Encrypts DES using CBC block mode.
 *
 * @param base LTC peripheral base address
 * @param handle pointer to ltc_dpa_handle_t structure which stores the transaction state.
 * @param plaintext Input plaintext to encrypt
 * @param[out] ciphertext Ouput ciphertext
 * @param size Size of input and output data in bytes
 * @param iv Input initial vector to combine with the first plaintext block.
 *           The iv does not need to be secret, but it must be unpredictable.
 * @return Status from encrypt/decrypt operation
 */
status_t LTC_DES_EncryptCbcDPA(LTC_Type *base,
                               ltc_dpa_handle_t *handle,
                               const uint8_t *plaintext,
                               uint8_t *ciphertext,
                               size_t size,
                               const uint8_t iv[LTC_DPA_DES_IV_SIZE]);

/*!
 * @brief Decrypts DES using CBC block mode.
 *
 * Decrypts DES using CBC block mode.
 *
 * @param base LTC peripheral base address
 * @param handle pointer to ltc_dpa_handle_t structure which stores the transaction state.
 * @param ciphertext Input ciphertext to decrypt
 * @param[out] plaintext Output plaintext
 * @param size Size of input data in bytes
 * @param iv Input initial vector to combine with the first plaintext block.
 *           The iv does not need to be secret, but it must be unpredictable.
 * @return Status from encrypt/decrypt operation
 */
status_t LTC_DES_DecryptCbcDPA(LTC_Type *base,
                               ltc_dpa_handle_t *handle,
                               const uint8_t *ciphertext,
                               uint8_t *plaintext,
                               size_t size,
                               const uint8_t iv[LTC_DPA_DES_IV_SIZE]);

/*!
 * @brief Encrypts DES using CFB block mode.
 *
 * Encrypts DES using CFB block mode.
 *
 * @param base LTC peripheral base address
 * @param handle pointer to ltc_dpa_handle_t structure which stores the transaction state.
 * @param plaintext Input plaintext to encrypt
 * @param size Size of input data in bytes
 * @param iv Input initial block.
 * @param[out] ciphertext Output ciphertext
 * @return Status from encrypt/decrypt operation
 */
status_t LTC_DES_EncryptCfbDPA(LTC_Type *base,
                               ltc_dpa_handle_t *handle,
                               const uint8_t *plaintext,
                               uint8_t *ciphertext,
                               size_t size,
                               const uint8_t iv[LTC_DPA_DES_IV_SIZE]);

/*!
 * @brief Decrypts DES using CFB block mode.
 *
 * Decrypts DES using CFB block mode.
 *
 * @param base LTC peripheral base address
 * @param handle pointer to ltc_dpa_handle_t structure which stores the transaction state.
 * @param ciphertext Input ciphertext to decrypt
 * @param[out] plaintext Output plaintext
 * @param size Size of input and output data in bytes
 * @param iv Input initial block.
 * @return Status from encrypt/decrypt operation
 */
status_t LTC_DES_DecryptCfbDPA(LTC_Type *base,
                               ltc_dpa_handle_t *handle,
                               const uint8_t *ciphertext,
                               uint8_t *plaintext,
                               size_t size,
                               const uint8_t iv[LTC_DPA_DES_IV_SIZE]);

/*!
 * @brief Encrypts DES using OFB block mode.
 *
 * Encrypts DES using OFB block mode.
 *
 * @param base LTC peripheral base address
 * @param handle pointer to ltc_dpa_handle_t structure which stores the transaction state.
 * @param plaintext Input plaintext to encrypt
 * @param[out] ciphertext Output ciphertext
 * @param size Size of input and output data in bytes
 * @param iv Input unique input vector. The OFB mode requires that the IV be unique
 *           for each execution of the mode under the given key.
 * @return Status from encrypt/decrypt operation
 */
status_t LTC_DES_EncryptOfbDPA(LTC_Type *base,
                               ltc_dpa_handle_t *handle,
                               const uint8_t *plaintext,
                               uint8_t *ciphertext,
                               size_t size,
                               const uint8_t iv[LTC_DPA_DES_IV_SIZE]);

/*!
 * @brief Decrypts DES using OFB block mode.
 *
 * Decrypts DES using OFB block mode.
 *
 * @param base LTC peripheral base address
 * @param handle pointer to ltc_dpa_handle_t structure which stores the transaction state.
 * @param ciphertext Input ciphertext to decrypt
 * @param[out] plaintext Output plaintext
 * @param size Size of input and output data in bytes. Must be multiple of 8 bytes.
 * @param iv Input unique input vector. The OFB mode requires that the IV be unique
 *           for each execution of the mode under the given key.
 * @return Status from encrypt/decrypt operation
 */
status_t LTC_DES_DecryptOfbDPA(LTC_Type *base,
                               ltc_dpa_handle_t *handle,
                               const uint8_t *ciphertext,
                               uint8_t *plaintext,
                               size_t size,
                               const uint8_t iv[LTC_DPA_DES_IV_SIZE]);

/*!
 * @brief Encrypts triple DES using ECB block mode with two keys.
 *
 * Encrypts triple DES using ECB block mode with two keys.
 *
 * @param base LTC peripheral base address
 * @param handle pointer to ltc_dpa_handle_t structure which stores the transaction state.
 * @param plaintext Input plaintext to encrypt
 * @param[out] ciphertext Output ciphertext
 * @param size Size of input and output data in bytes. Must be multiple of 8 bytes.
 * @return Status from encrypt/decrypt operation
 */
status_t LTC_DES2_EncryptEcbDPA(
    LTC_Type *base, ltc_dpa_handle_t *handle, const uint8_t *plaintext, uint8_t *ciphertext, size_t size);

/*!
 * @brief Decrypts triple DES using ECB block mode with two keys.
 *
 * Decrypts triple DES using ECB block mode with two keys.
 *
 * @param base LTC peripheral base address
 * @param handle pointer to ltc_dpa_handle_t structure which stores the transaction state.
 * @param ciphertext Input ciphertext to decrypt
 * @param[out] plaintext Output plaintext
 * @param size Size of input and output data in bytes. Must be multiple of 8 bytes.
 * @return Status from encrypt/decrypt operation
 */
status_t LTC_DES2_DecryptEcbDPA(
    LTC_Type *base, ltc_dpa_handle_t *handle, const uint8_t *ciphertext, uint8_t *plaintext, size_t size);

/*!
 * @brief Encrypts triple DES using CBC block mode with two keys.
 *
 * Encrypts triple DES using CBC block mode with two keys.
 *
 * @param base LTC peripheral base address
 * @param handle pointer to ltc_dpa_handle_t structure which stores the transaction state.
 * @param plaintext Input plaintext to encrypt
 * @param[out] ciphertext Output ciphertext
 * @param size Size of input and output data in bytes
 * @param iv Input initial vector to combine with the first plaintext block.
 *           The iv does not need to be secret, but it must be unpredictable.
 * @return Status from encrypt/decrypt operation
 */
status_t LTC_DES2_EncryptCbcDPA(LTC_Type *base,
                                ltc_dpa_handle_t *handle,
                                const uint8_t *plaintext,
                                uint8_t *ciphertext,
                                size_t size,
                                const uint8_t iv[LTC_DPA_DES_IV_SIZE]);

/*!
 * @brief Decrypts triple DES using CBC block mode with two keys.
 *
 * Decrypts triple DES using CBC block mode with two keys.
 *
 * @param base LTC peripheral base address
 * @param handle pointer to ltc_dpa_handle_t structure which stores the transaction state.
 * @param ciphertext Input ciphertext to decrypt
 * @param[out] plaintext Output plaintext
 * @param size Size of input and output data in bytes
 * @param iv Input initial vector to combine with the first plaintext block.
 *           The iv does not need to be secret, but it must be unpredictable.
 * @return Status from encrypt/decrypt operation
 */
status_t LTC_DES2_DecryptCbcDPA(LTC_Type *base,
                                ltc_dpa_handle_t *handle,
                                const uint8_t *ciphertext,
                                uint8_t *plaintext,
                                size_t size,
                                const uint8_t iv[LTC_DPA_DES_IV_SIZE]);

/*!
 * @brief Encrypts triple DES using CFB block mode with two keys.
 *
 * Encrypts triple DES using CFB block mode with two keys.
 *
 * @param base LTC peripheral base address
 * @param handle pointer to ltc_dpa_handle_t structure which stores the transaction state.
 * @param plaintext Input plaintext to encrypt
 * @param[out] ciphertext Output ciphertext
 * @param size Size of input and output data in bytes
 * @param iv Input initial block.
 * @return Status from encrypt/decrypt operation
 */
status_t LTC_DES2_EncryptCfbDPA(LTC_Type *base,
                                ltc_dpa_handle_t *handle,
                                const uint8_t *plaintext,
                                uint8_t *ciphertext,
                                size_t size,
                                const uint8_t iv[LTC_DPA_DES_IV_SIZE]);

/*!
 * @brief Decrypts triple DES using CFB block mode with two keys.
 *
 * Decrypts triple DES using CFB block mode with two keys.
 *
 * @param base LTC peripheral base address
 * @param handle pointer to ltc_dpa_handle_t structure which stores the transaction state.
 * @param ciphertext Input ciphertext to decrypt
 * @param[out] plaintext Output plaintext
 * @param size Size of input and output data in bytes
 * @param iv Input initial block.
 * @return Status from encrypt/decrypt operation
 */
status_t LTC_DES2_DecryptCfbDPA(LTC_Type *base,
                                ltc_dpa_handle_t *handle,
                                const uint8_t *ciphertext,
                                uint8_t *plaintext,
                                size_t size,
                                const uint8_t iv[LTC_DPA_DES_IV_SIZE]);

/*!
 * @brief Encrypts triple DES using OFB block mode with two keys.
 *
 * Encrypts triple DES using OFB block mode with two keys.
 *
 * @param base LTC peripheral base address
 * @param handle pointer to ltc_dpa_handle_t structure which stores the transaction state.
 * @param plaintext Input plaintext to encrypt
 * @param[out] ciphertext Output ciphertext
 * @param size Size of input and output data in bytes
 * @param iv Input unique input vector. The OFB mode requires that the IV be unique
 *           for each execution of the mode under the given key.
 * @return Status from encrypt/decrypt operation
 */
status_t LTC_DES2_EncryptOfbDPA(LTC_Type *base,
                                ltc_dpa_handle_t *handle,
                                const uint8_t *plaintext,
                                uint8_t *ciphertext,
                                size_t size,
                                const uint8_t iv[LTC_DPA_DES_IV_SIZE]);

/*!
 * @brief Decrypts triple DES using OFB block mode with two keys.
 *
 * Decrypts triple DES using OFB block mode with two keys.
 *
 * @param base LTC peripheral base address
 * @param handle pointer to ltc_dpa_handle_t structure which stores the transaction state.
 * @param ciphertext Input ciphertext to decrypt
 * @param[out] plaintext Output plaintext
 * @param size Size of input and output data in bytes
 * @param iv Input unique input vector. The OFB mode requires that the IV be unique
 *           for each execution of the mode under the given key.
 * @return Status from encrypt/decrypt operation
 */
status_t LTC_DES2_DecryptOfbDPA(LTC_Type *base,
                                ltc_dpa_handle_t *handle,
                                const uint8_t *ciphertext,
                                uint8_t *plaintext,
                                size_t size,
                                const uint8_t iv[LTC_DPA_DES_IV_SIZE]);

/*!
 * @brief Encrypts triple DES using ECB block mode with three keys.
 *
 * Encrypts triple DES using ECB block mode with three keys.
 *
 * @param base LTC peripheral base address
 * @param handle pointer to ltc_dpa_handle_t structure which stores the transaction state.
 * @param plaintext Input plaintext to encrypt
 * @param[out] ciphertext Output ciphertext
 * @param size Size of input and output data in bytes. Must be multiple of 8 bytes.
 * @return Status from encrypt/decrypt operation
 */
status_t LTC_DES3_EncryptEcbDPA(
    LTC_Type *base, ltc_dpa_handle_t *handle, const uint8_t *plaintext, uint8_t *ciphertext, size_t size);

/*!
 * @brief Decrypts triple DES using ECB block mode with three keys.
 *
 * Decrypts triple DES using ECB block mode with three keys.
 *
 * @param base LTC peripheral base address
 * @param handle pointer to ltc_dpa_handle_t structure which stores the transaction state.
 * @param ciphertext Input ciphertext to decrypt
 * @param[out] plaintext Output plaintext
 * @param size Size of input and output data in bytes. Must be multiple of 8 bytes.
 * @return Status from encrypt/decrypt operation
 */
status_t LTC_DES3_DecryptEcbDPA(
    LTC_Type *base, ltc_dpa_handle_t *handle, const uint8_t *ciphertext, uint8_t *plaintext, size_t size);

/*!
 * @brief Encrypts triple DES using CBC block mode with three keys.
 *
 * Encrypts triple DES using CBC block mode with three keys.
 *
 * @param base LTC peripheral base address
 * @param handle pointer to ltc_dpa_handle_t structure which stores the transaction state.
 * @param plaintext Input plaintext to encrypt
 * @param[out] ciphertext Output ciphertext
 * @param size Size of input data in bytes
 * @param iv Input initial vector to combine with the first plaintext block.
 *           The iv does not need to be secret, but it must be unpredictable.
 * @return Status from encrypt/decrypt operation
 */
status_t LTC_DES3_EncryptCbcDPA(LTC_Type *base,
                                ltc_dpa_handle_t *handle,
                                const uint8_t *plaintext,
                                uint8_t *ciphertext,
                                size_t size,
                                const uint8_t iv[LTC_DPA_DES_IV_SIZE]);

/*!
 * @brief Decrypts triple DES using CBC block mode with three keys.
 *
 * Decrypts triple DES using CBC block mode with three keys.
 *
 * @param base LTC peripheral base address
 * @param handle pointer to ltc_dpa_handle_t structure which stores the transaction state.
 * @param ciphertext Input ciphertext to decrypt
 * @param[out] plaintext Output plaintext
 * @param size Size of input and output data in bytes
 * @param iv Input initial vector to combine with the first plaintext block.
 *           The iv does not need to be secret, but it must be unpredictable.
 * @return Status from encrypt/decrypt operation
 */
status_t LTC_DES3_DecryptCbcDPA(LTC_Type *base,
                                ltc_dpa_handle_t *handle,
                                const uint8_t *ciphertext,
                                uint8_t *plaintext,
                                size_t size,
                                const uint8_t iv[LTC_DPA_DES_IV_SIZE]);

/*!
 * @brief Encrypts triple DES using CFB block mode with three keys.
 *
 * Encrypts triple DES using CFB block mode with three keys.
 *
 * @param base LTC peripheral base address
 * @param handle pointer to ltc_dpa_handle_t structure which stores the transaction state.
 * @param plaintext Input plaintext to encrypt
 * @param[out] ciphertext Output ciphertext
 * @param size Size of input and ouput data in bytes
 * @param iv Input initial block.
 * @return Status from encrypt/decrypt operation
 */
status_t LTC_DES3_EncryptCfbDPA(LTC_Type *base,
                                ltc_dpa_handle_t *handle,
                                const uint8_t *plaintext,
                                uint8_t *ciphertext,
                                size_t size,
                                const uint8_t iv[LTC_DPA_DES_IV_SIZE]);

/*!
 * @brief Decrypts triple DES using CFB block mode with three keys.
 *
 * Decrypts triple DES using CFB block mode with three keys.
 *
 * @param base LTC peripheral base address
 * @param handle pointer to ltc_dpa_handle_t structure which stores the transaction state.
 * @param ciphertext Input ciphertext to decrypt
 * @param[out] plaintext Output plaintext
 * @param size Size of input data in bytes
 * @param iv Input initial block.
 * @return Status from encrypt/decrypt operation
 */
status_t LTC_DES3_DecryptCfbDPA(LTC_Type *base,
                                ltc_dpa_handle_t *handle,
                                const uint8_t *ciphertext,
                                uint8_t *plaintext,
                                size_t size,
                                const uint8_t iv[LTC_DPA_DES_IV_SIZE]);

/*!
 * @brief Encrypts triple DES using OFB block mode with three keys.
 *
 * Encrypts triple DES using OFB block mode with three keys.
 *
 * @param base LTC peripheral base address
 * @param handle pointer to ltc_dpa_handle_t structure which stores the transaction state.
 * @param plaintext Input plaintext to encrypt
 * @param[out] ciphertext Output ciphertext
 * @param size Size of input and output data in bytes
 * @param iv Input unique input vector. The OFB mode requires that the IV be unique
 *           for each execution of the mode under the given key.
 * @return Status from encrypt/decrypt operation
 */
status_t LTC_DES3_EncryptOfbDPA(LTC_Type *base,
                                ltc_dpa_handle_t *handle,
                                const uint8_t *plaintext,
                                uint8_t *ciphertext,
                                size_t size,
                                const uint8_t iv[LTC_DPA_DES_IV_SIZE]);

/*!
 * @brief Decrypts triple DES using OFB block mode with three keys.
 *
 * Decrypts triple DES using OFB block mode with three keys.
 *
 * @param base LTC peripheral base address
 * @param handle pointer to ltc_dpa_handle_t structure which stores the transaction state.
 * @param ciphertext Input ciphertext to decrypt
 * @param[out] plaintext Output plaintext
 * @param size Size of input and output data in bytes
 * @param iv Input unique input vector. The OFB mode requires that the IV be unique
 *           for each execution of the mode under the given key.
 * @return Status from encrypt/decrypt operation
 */
status_t LTC_DES3_DecryptOfbDPA(LTC_Type *base,
                                ltc_dpa_handle_t *handle,
                                const uint8_t *ciphertext,
                                uint8_t *plaintext,
                                size_t size,
                                const uint8_t iv[LTC_DPA_DES_IV_SIZE]);

/*!
 *@}
 */ /* end of ltc_driver_des_with_dpa */

/*******************************************************************************
 * CMAC API
 ******************************************************************************/

/*!
 * @internal @addtogroup ltc_driver_cmac_with_dpa
 * @{
 */
/*!
 * @brief Initialize CMAC context
 *
 * This function initialize the CMAC.
 * Key shall be supplied because the underlaying algoritm is AES XCBC-MAC or AES CMAC.
 *
 * For XCBC-MAC, the key length must be 16. For CMAC, the key length can be
 * the AES key lengths supported by AES engine.
 *
 * @param base LTC peripheral base address
 * @param handle pointer to ltc_dpa_handle_t structure which stores the transaction state.
 * @param algo Underlaying algorithm to use for hash computation.
 * @param key Input key (NULL if underlaying algorithm is SHA)
 * @param keySize Size of input key in bytes
 * @return Status of initialization
 */
status_t LTC_CMAC_InitDPA(
    LTC_Type *base, ltc_dpa_handle_t *handle, ltc_dpa_hash_algo_t algo, const uint8_t *key, size_t keySize);

/*!
 * @brief Add data to current CMAC
 *
 * Add data to current CMAC. This can be called repeatedly with an arbitrary amount of data to be
 * hashed.
 *
 * @param base LTC peripheral base address
 * @param handle pointer to ltc_dpa_handle_t structure which stores the transaction state.
 * @param message Input data
 * @param messageSize Size of input data in bytes
 * @return Status of the hash update operation
 */
status_t LTC_CMAC_UpdateDPA(LTC_Type *base, ltc_dpa_handle_t *handle, const uint8_t *message, size_t messageSize);

/*!
 * @brief Finalize hashing
 *
 * Outputs the final hash and erases the context.
 *
 * @param base LTC peripheral base address
 * @param handle pointer to ltc_dpa_handle_t structure which stores the transaction state.
 * @param[out] output Output hash data
 * @param[out] outputSize Output parameter storing the size of the output hash in bytes
 * @return Status of the hash finish operation
 */
status_t LTC_CMAC_FinishDPA(LTC_Type *base, ltc_dpa_handle_t *handle, uint8_t *output, size_t *outputSize);

/*!
 * @brief Create CMAC on given data
 *
 * Perform the full keyed CMAC in one function call.
 *
 * @param base LTC peripheral base address
 * @param handle pointer to ltc_dpa_handle_t structure which stores the transaction state.
 * @param algo Block cipher algorithm to use for CMAC creation
 * @param input Input data
 * @param inputSize Size of input data in bytes
 * @param key Input key
 * @param keySize Size of input key in bytes
 * @param[out] output Output hash data
 * @param[out] outputSize Output parameter storing the size of the output hash in bytes
 * @return Status of the one call hash operation.
 */
status_t LTC_CMAC_DPA(LTC_Type *base,
                      ltc_dpa_handle_t *handle,
                      ltc_dpa_hash_algo_t algo,
                      const uint8_t *input,
                      size_t inputSize,
                      const uint8_t *key,
                      size_t keySize,
                      uint8_t *output,
                      size_t *outputSize);
/*!
 *@}
 */ /* end of ltc_driver_cmac_with_dpa */

/*! @}*/ /* end of ltc_driver_dpa */

#if defined(__cplusplus)
}
#endif

#endif /* _FSL_LTC_DPA_H_ */
