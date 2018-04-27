#ifndef _GNSS_UBX_NMEA_H_
#define _GNSS_UBX_NMEA_H_

#include <stdint.h>
#include <stdbool.h>
#include <gnss/types.h>
#include <gnss/ubx.h>
#include <gnss/nmea.h>

/**
 * UBX+NMEA parser
 *
 * This parser is able to handle UBX and NMEA communication on
 * the same transport layer.
 *
 * For more infomation see the documentation for UBX parser and NMEA parser
 */




/**
 * Parser context for UBX+NMEA.
 *  (no serviceable part inside)
 */
struct gnss_ubx_nmea {
    uint8_t  sync_char;
    uint8_t  type;
    union {
        struct gnss_ubx  ubx;
        struct gnss_nmea nmea;
    };
};
    
/**
 * Initialize the protocol layer for parsing UBX+NMEA sentence.
 *
 * @param ctx           GNSS context 
 * @param ubx_nmea      Context
 *
 * @return true on success
 */
#if (MYNEWT_VAL(GNSS_USE_UBX_PROTOCOL ) > 0) && \
    (MYNEWT_VAL(GNSS_USE_NMEA_PROTOCOL) > 0)
bool gnss_ubx_nmea_init(gnss_t *ctx, struct gnss_ubx_nmea *ubx_nmea);
#endif

/* Byte decoder for UBX+NMEA protocol */
int gnss_ubx_nmea_byte_decoder(gnss_t *ctx, struct gnss_ubx_nmea *gun, uint8_t byte);

#endif
