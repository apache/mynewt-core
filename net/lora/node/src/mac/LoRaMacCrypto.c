/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
    (C)2013 Semtech
 ___ _____ _   ___ _  _____ ___  ___  ___ ___
/ __|_   _/_\ / __| |/ / __/ _ \| _ \/ __| __|
\__ \ | |/ _ \ (__| ' <| _| (_) |   / (__| _|
|___/ |_/_/ \_\___|_|\_\_| \___/|_|_\\___|___|
embedded.connectivity.solutions===============

Description: LoRa MAC layer implementation

License: Revised BSD License, see LICENSE.TXT file include in the project

Maintainer: Miguel Luis ( Semtech ), Gregory Cristian ( Semtech ) and Daniel Jäckle ( STACKFORCE )
*/
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "node/utilities.h"

#include "mbedtls/aes.h"
#include "mbedtls/cipher.h"
#include "mbedtls/cmac.h"

#include "node/mac/LoRaMacCrypto.h"

/*!
 * CMAC/AES Message Integrity Code (MIC) Block B0 size
 */
#define LORAMAC_MIC_BLOCK_B0_SIZE                   16

/*!
 * MIC field computation initial data
 */
static uint8_t MicBlockB0[] = { 0x49, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                              };

/*!
 * Contains the computed MIC field.
 *
 * \remark Only the 4 first bytes are used
 */
static uint8_t Mic[16];

/*!
 * Encryption aBlock and sBlock
 */
static uint8_t aBlock[] = { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                          };
static uint8_t sBlock[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                          };

/*!
 * CMAC computation context, set up on first use so that computing a MIC
 * does not allocate memory
 */
static mbedtls_cipher_context_t AesCmacCtx;
static bool AesCmacCtxReady;

/*!
 * Computes the AES-CMAC of up to two buffers with a 128 bit key
 */
static void LoRaMacCmac( const uint8_t *key, const uint8_t *buf1, size_t len1,
                         const uint8_t *buf2, size_t len2, uint8_t *mac )
{
    int rc;

    (void)rc;

    if( !AesCmacCtxReady )
    {
        mbedtls_cipher_init( &AesCmacCtx );
        rc = mbedtls_cipher_setup( &AesCmacCtx,
            mbedtls_cipher_info_from_type( MBEDTLS_CIPHER_AES_128_ECB ) );
        assert( rc == 0 );
        rc = mbedtls_cipher_cmac_starts( &AesCmacCtx, key, 128 );
        assert( rc == 0 );
        AesCmacCtxReady = true;
    }
    else
    {
        rc = mbedtls_cipher_setkey( &AesCmacCtx, key, 128, MBEDTLS_ENCRYPT );
        assert( rc == 0 );
        rc = mbedtls_cipher_cmac_reset( &AesCmacCtx );
        assert( rc == 0 );
    }

    rc = mbedtls_cipher_cmac_update( &AesCmacCtx, buf1, len1 );
    assert( rc == 0 );
    if( len2 > 0 )
    {
        rc = mbedtls_cipher_cmac_update( &AesCmacCtx, buf2, len2 );
        assert( rc == 0 );
    }
    rc = mbedtls_cipher_cmac_finish( &AesCmacCtx, mac );
    assert( rc == 0 );
}

/*!
 * Sets up an AES context for encryption with a 128 bit key
 */
static void LoRaMacAesInit( mbedtls_aes_context *ctx, const uint8_t *key )
{
    int rc;

    (void)rc;

    mbedtls_aes_init( ctx );
    rc = mbedtls_aes_setkey_enc( ctx, key, 128 );
    assert( rc == 0 );
}

/*!
 * Encrypts one block with AES in ECB mode
 */
static void LoRaMacAesEncrypt( mbedtls_aes_context *ctx, const uint8_t *in,
                               uint8_t *out )
{
    int rc;

    (void)rc;

    rc = mbedtls_aes_crypt_ecb( ctx, MBEDTLS_AES_ENCRYPT, in, out );
    assert( rc == 0 );
}

/*!
 * \brief Computes the LoRaMAC frame MIC field
 *
 * \param [IN]  buffer          Data buffer
 * \param [IN]  size            Data buffer size
 * \param [IN]  key             AES key to be used
 * \param [IN]  address         Frame address
 * \param [IN]  dir             Frame direction [0: uplink, 1: downlink]
 * \param [IN]  sequenceCounter Frame sequence counter
 * \param [OUT] mic Computed MIC field
 */
void LoRaMacComputeMic( const uint8_t *buffer, uint16_t size, const uint8_t *key, uint32_t address, uint8_t dir, uint32_t sequenceCounter, uint32_t *mic )
{
    MicBlockB0[5] = dir;

    MicBlockB0[6] = ( address ) & 0xFF;
    MicBlockB0[7] = ( address >> 8 ) & 0xFF;
    MicBlockB0[8] = ( address >> 16 ) & 0xFF;
    MicBlockB0[9] = ( address >> 24 ) & 0xFF;

    MicBlockB0[10] = ( sequenceCounter ) & 0xFF;
    MicBlockB0[11] = ( sequenceCounter >> 8 ) & 0xFF;
    MicBlockB0[12] = ( sequenceCounter >> 16 ) & 0xFF;
    MicBlockB0[13] = ( sequenceCounter >> 24 ) & 0xFF;

    MicBlockB0[15] = size & 0xFF;

    LoRaMacCmac( key, MicBlockB0, LORAMAC_MIC_BLOCK_B0_SIZE, buffer,
                 size & 0xFF, Mic );

    *mic = ( uint32_t )( ( uint32_t )Mic[3] << 24 | ( uint32_t )Mic[2] << 16 | ( uint32_t )Mic[1] << 8 | ( uint32_t )Mic[0] );
}

void LoRaMacPayloadEncrypt( const uint8_t *buffer, uint16_t size, const uint8_t *key, uint32_t address, uint8_t dir, uint32_t sequenceCounter, uint8_t *encBuffer )
{
    uint16_t i;
    uint8_t bufferIndex = 0;
    uint16_t ctr = 1;
    mbedtls_aes_context aes;

    LoRaMacAesInit( &aes, key );

    aBlock[5] = dir;

    aBlock[6] = ( address ) & 0xFF;
    aBlock[7] = ( address >> 8 ) & 0xFF;
    aBlock[8] = ( address >> 16 ) & 0xFF;
    aBlock[9] = ( address >> 24 ) & 0xFF;

    aBlock[10] = ( sequenceCounter ) & 0xFF;
    aBlock[11] = ( sequenceCounter >> 8 ) & 0xFF;
    aBlock[12] = ( sequenceCounter >> 16 ) & 0xFF;
    aBlock[13] = ( sequenceCounter >> 24 ) & 0xFF;

    while( size >= 16 )
    {
        aBlock[15] = ( ( ctr ) & 0xFF );
        ctr++;
        LoRaMacAesEncrypt( &aes, aBlock, sBlock );
        for( i = 0; i < 16; i++ )
        {
            encBuffer[bufferIndex + i] = buffer[bufferIndex + i] ^ sBlock[i];
        }
        size -= 16;
        bufferIndex += 16;
    }

    if( size > 0 )
    {
        aBlock[15] = ( ( ctr ) & 0xFF );
        LoRaMacAesEncrypt( &aes, aBlock, sBlock );
        for( i = 0; i < size; i++ )
        {
            encBuffer[bufferIndex + i] = buffer[bufferIndex + i] ^ sBlock[i];
        }
    }

    mbedtls_aes_free( &aes );
}

void LoRaMacPayloadDecrypt( const uint8_t *buffer, uint16_t size, const uint8_t *key, uint32_t address, uint8_t dir, uint32_t sequenceCounter, uint8_t *decBuffer )
{
    LoRaMacPayloadEncrypt( buffer, size, key, address, dir, sequenceCounter, decBuffer );
}

void LoRaMacJoinComputeMic( const uint8_t *buffer, uint16_t size, const uint8_t *key, uint32_t *mic )
{
    LoRaMacCmac( key, buffer, size & 0xFF, NULL, 0, Mic );

    *mic = ( uint32_t )( ( uint32_t )Mic[3] << 24 | ( uint32_t )Mic[2] << 16 | ( uint32_t )Mic[1] << 8 | ( uint32_t )Mic[0] );
}

void LoRaMacJoinDecrypt( const uint8_t *buffer, uint16_t size, const uint8_t *key, uint8_t *decBuffer )
{
    mbedtls_aes_context aes;

    LoRaMacAesInit( &aes, key );
    LoRaMacAesEncrypt( &aes, buffer, decBuffer );
    // Check if optional CFList is included
    if( size >= 16 )
    {
        LoRaMacAesEncrypt( &aes, buffer + 16, decBuffer + 16 );
    }
    mbedtls_aes_free( &aes );
}

void LoRaMacJoinComputeSKeys( const uint8_t *key, const uint8_t *appNonce, uint16_t devNonce, uint8_t *nwkSKey, uint8_t *appSKey )
{
    uint8_t nonce[16];
    uint8_t *pDevNonce = ( uint8_t * )&devNonce;
    mbedtls_aes_context aes;

    LoRaMacAesInit( &aes, key );

    memset( nonce, 0, sizeof( nonce ) );
    nonce[0] = 0x01;
    memcpy( nonce + 1, appNonce, 6 );
    memcpy( nonce + 7, pDevNonce, 2 );
    LoRaMacAesEncrypt( &aes, nonce, nwkSKey );

    memset( nonce, 0, sizeof( nonce ) );
    nonce[0] = 0x02;
    memcpy( nonce + 1, appNonce, 6 );
    memcpy( nonce + 7, pDevNonce, 2 );
    LoRaMacAesEncrypt( &aes, nonce, appSKey );

    mbedtls_aes_free( &aes );
}
