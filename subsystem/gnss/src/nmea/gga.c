#include <gnss/nmea.h>
#include <gnss/log.h>

/*
 * See: http://www.catb.org/gpsd/NMEA.html#_gga_global_positioning_system_fix_data
 */
int
gnss_nmea_decoder_gga(struct gnss_nmea_gga *gga, char *field, int fid) {
    int rc = 1;
    union {
	long   fix_indicator;
	long   satellites_in_view;
	long   dgps_age;
	long   dgps_sid;
	char   unit;
    } local;

    switch(fid) {
    case  0:	/* xxGGA */
	break;

    case  1:	/* UTC Time */
	rc = gnss_nmea_field_parse_time(field, &gga->time);
	break;

    case  2:	/* Latitude */
	rc = gnss_nmea_field_parse_latlng(field, &gga->latitude);
	break;	

    case  3: 	/* N/S indicator */
	rc = gnss_nmea_field_parse_and_apply_direction(field, &gga->latitude);
	break;

    case  4: 	/* Longitude */
	rc = gnss_nmea_field_parse_latlng(field, &gga->longitude);
	break;

    case  5: 	/* E/W indicator */
	rc = gnss_nmea_field_parse_and_apply_direction(field, &gga->longitude);
	break;

    case  6:	/* Fix indicator */
	rc = gnss_nmea_field_parse_long(field, &local.fix_indicator);
	if (rc <= 0) {
	    break;
	}
	if (local.fix_indicator <= 8) {
	    gga->fix_indicator = local.fix_indicator;
	} else {
	    rc = 0;
	}
	break;

    case  7: 	/* Satellites in view */
	rc = gnss_nmea_field_parse_long(field, &local.satellites_in_view);
	if (rc <= 0) {
	    break;
	}
	if (local.satellites_in_view <= 12) {
	    gga->satellites_in_view = local.satellites_in_view;
	} else {
	    rc = 0;
	}
	break;

    case  8:	/* Horizontal dilution of precision */
	rc = gnss_nmea_field_parse_float(field, &gga->hdop);
	break;

    case  9:	/* Antenna Altitude */
	rc = gnss_nmea_field_parse_float(field, &gga->altitude);
	break;

    case 10:	/* Unit */
#if MYNEWT_VAL(GNSS_NMEA_PARSER_VALIDATING) >= 3
	rc = gnss_nmea_field_parse_char(field, &local.unit);
	if (rc <= 0) {
	    break;
	}
	if (local.unit != 'M') {
	    rc = 0;
	}
#endif
	break;

    case 11:	/* Geoidal separation */
	rc = gnss_nmea_field_parse_float(field, &gga->geoid_separation);
	break;

    case 12:	/* Unit */
#if MYNEWT_VAL(GNSS_NMEA_PARSER_VALIDATING) >= 3
	rc = gnss_nmea_field_parse_char(field, &local.unit);
	if (rc <= 0) {
	    break;
	}
	if (local.unit != 'M') {
	    rc = 0;
	}
#endif
	break;

    case 13:	/* Seconds since last DGPS */
	rc = gnss_nmea_field_parse_long(field, &local.dgps_age);
	if (rc <= 0) {
	    break;
	}
	if (local.dgps_age <= 65535) {
	    gga->dgps_age = local.dgps_age;
	} else {
	    rc = 0;
	}
	break;

    case 14:	/* Referential station for DGPS */
	rc = gnss_nmea_field_parse_long(field, &local.dgps_age);
	if (rc <= 0) {
	    break;
	}
	if (local.dgps_age <= 1023) {
	    gga->dgps_sid = local.dgps_sid;
	} else {
	    rc = 0;
	}
	break;

    default:
	assert(0);
	break;
    }

    return rc;
}

void
gnss_nmea_log_gga(struct gnss_nmea_gga *gga)
{
    if (gga->time.present) {
	GNSS_LOG_INFO("GGA: Time = %2d:%02d:%02d.%03d\n",
		      gga->time.hours,
		      gga->time.minutes,
		      gga->time.seconds,
		      gga->time.microseconds / 1000);
    }

    GNSS_LOG_INFO("GGA: DGPS = %d (%d)\n", gga->dgps_age, gga->dgps_sid);
    GNSS_LOG_INFO("GGA: Geoid sep. = %f\n", GNSS_SYSFLOAT(gga->geoid_separation));
    GNSS_LOG_INFO("GGA: LatLng = %f, %f\n",
		  GNSS_SYSFLOAT(gga->latitude), GNSS_SYSFLOAT(gga->longitude));
    GNSS_LOG_INFO("GGA: Altitude = %f \n", GNSS_SYSFLOAT(gga->altitude));
    GNSS_LOG_INFO("GGA: HDOP = %f\n", GNSS_SYSFLOAT(gga->hdop));
    GNSS_LOG_INFO("GGA: FIX = %d\n", gga->fix_indicator);
    GNSS_LOG_INFO("GGA: Satellites = %d\n", gga->satellites_in_view);
}
