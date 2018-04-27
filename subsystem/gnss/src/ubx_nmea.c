#include <string.h>
#include <gnss/gnss.h>
#include <gnss/ubx_nmea.h>
#include <console/console.h>

/*
 * Decoding of UBX/NMEA sentence
 *
 * @param ctx		GNSS context
 * @param gn		UBX/NMEA decoder context
 * @param byte		byte to decode
 *
 * @return GNSS_BYTE_DECODER_*
 */
int
gnss_ubx_nmea_byte_decoder(gnss_t *ctx, struct gnss_ubx_nmea *gun, uint8_t byte)
{
    int rc;
    
    switch(gun->type) {
    default:
	assert(0);
	/* FALL THROUGH */
	
    /* Detecting decoding type */
    case GNSS_EVENT_UNKNOWN:
	switch(byte) {
	case 0xB5:
	    gun->sync_char = 1;                /* Found UBX first sync-char */
	    return GNSS_BYTE_DECODER_DECODING; /* Decoded char ok           */
	    
	case 0x62:
	    /* Found UBX second sync-char ? */
	    if (gun->sync_char == 1) {
		gun->sync_char = 0;		/* Reset sync-char counter */
		gun->type      = GNSS_EVENT_UBX;/* Switch to UBX decoding  */

		/* Need to reset UBX decoding context */
		memset(&gun->ubx, 0, sizeof(gun->ubx));
		/* Re-generate swallowed char */
		gnss_ubx_byte_decoder(ctx, &gun->ubx, 0xB5);
		/* Go to UBX decoder */
		goto ubx;
	    };
	    break;

	case '$':
	    /* Found NMEA start marker */
	    gun->sync_char = 0;			/* Reset sync-char counter */
	    gun->type      = GNSS_EVENT_NMEA;	/* Switch to NMEA decoding */

	    /* Need to reset NMEA decoding context */
	    memset(&gun->nmea, 0, sizeof(gun->nmea));
	    /* Go to NMEA decoder */
	    goto nmea;
	}

	/* Still trying to find start of frame/sentence */
	gun->sync_char = 0;	                /* Reset sync char counter */
	return GNSS_BYTE_DECODER_SYNCING;       /* No decoded char         */

    /* NMEA decoding */
    nmea:
    case GNSS_EVENT_NMEA:
	rc = gnss_nmea_byte_decoder(ctx, &gun->nmea, byte);
	break;
	
    /* UBX decoding */
    ubx:
    case GNSS_EVENT_UBX:
	rc = gnss_ubx_byte_decoder(ctx, &gun->ubx, byte);
	break;
    }

    /* Check decoding status */
    if (rc != GNSS_BYTE_DECODER_DECODING) {
	assert(gun->sync_char == 0);         /* Should already be cleaned  */
	gun->type = GNSS_EVENT_UNKNOWN;	     /* Switch to unknown decoding */
    }

    return rc;
}

static int
gnss_ubx_nmea_decoder(gnss_t *ctx, uint8_t byte)
{
    int rc;
    struct gnss_ubx_nmea *gun = ctx->protocol.conf;

    rc = gnss_ubx_nmea_byte_decoder(ctx, gun, byte);
    rc = gnss_check_scrambled_transport(ctx, rc);

    return rc;
}

bool
gnss_ubx_nmea_init(gnss_t *ctx, struct gnss_ubx_nmea *ubx_nmea)
{
    ctx->protocol.conf    = ubx_nmea;
    ctx->protocol.decoder = gnss_ubx_nmea_decoder; /* The decoder */
    return true;
}
