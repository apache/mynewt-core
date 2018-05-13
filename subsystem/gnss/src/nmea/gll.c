#include <gnss/nmea.h>
#include <gnss/log.h>

/*
 * See: http://www.catb.org/gpsd/NMEA.html#_gll_geographic_position_latitude_longitude
 */

int
gnss_nmea_decoder_gll(struct gnss_nmea_gll *gll, char *field, int fid) {
    int rc = 1;
    union {
	char   status;
    } local;

    switch(fid) {
    case  0:	/* xxGLL */
	break;

    case  1:	/* Latitude */
	rc = gnss_nmea_field_parse_latlng(field, &gll->latitude);
	break;
	
    case  2: 	/* N/S indicator */
	rc = gnss_nmea_field_parse_and_apply_direction(field, &gll->latitude);
	break;
	
    case  3: 	/* Longitude */
	rc = gnss_nmea_field_parse_latlng(field, &gll->longitude);
	break;	

    case  4: 	/* E/W indicator */
	rc = gnss_nmea_field_parse_and_apply_direction(field, &gll->longitude);
	break;

    case  5:	/* UTC Time */
	rc = gnss_nmea_field_parse_time(field, &gll->time);
	break;

    case  6:	/* Status */
	rc = gnss_nmea_field_parse_char(field, &local.status);
	if (rc <= 0) {
	    break;
	}
	gll->valid = local.status == 'A';
	break;

    case 7:	/* FAA mode */
	rc = gnss_nmea_field_parse_char(field, &gll->faa_mode);
	break;

    default:
	assert(0);
	break;
    }

    return rc;
}


void
gnss_nmea_log_gll(struct gnss_nmea_gll *gll)
{
    if (gll->time.present) {
	GNSS_LOG_INFO("GLL: Time = %2d:%02d:%02d.%03d\n",
		      gll->time.hours,
		      gll->time.minutes,
		      gll->time.seconds,
		      gll->time.microseconds / 1000);
    }
    if (gll->valid) {
	GNSS_LOG_INFO("GLL: "
		      "LatLng = %f, %f / ",
		      "FAA mode = %c"
		      "\n",
		      GNSS_SYSFLOAT(gll->latitude),
		      GNSS_SYSFLOAT(gll->longitude),
		      gll->faa_mode);
    }

    if (!gll->time.present && !gll->valid) {
	GNSS_LOG_INFO("GLL: <no valid output>\n");
    }
}
