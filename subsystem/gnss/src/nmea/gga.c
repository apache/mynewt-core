#include <gnss/nmea.h>
#include <gnss/log.h>

/*
 * See: http://www.catb.org/gpsd/NMEA.html#_gga_global_positioning_system_fix_data
 */
bool
gnss_nmea_decoder_gga(struct gnss_nmea_gga *gga, char *field, int fid) {
    bool success = true;
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
	success = gnss_nmea_field_parse_time(field, &gga->time);
	break;

    case  2:	/* Latitude */
	success = gnss_nmea_field_parse_latlng(field, &gga->latitude);
	break;	

    case  3: 	/* N/S indicator */
	success = gnss_nmea_field_parse_and_apply_direction(field, &gga->latitude);
	break;

    case  4: 	/* Longitude */
	success = gnss_nmea_field_parse_latlng(field, &gga->longitude);
	break;

    case  5: 	/* E/W indicator */
	success = gnss_nmea_field_parse_and_apply_direction(field, &gga->longitude);
	break;

    case  6:	/* Fix indicator */
	success = gnss_nmea_field_parse_long(field, &local.fix_indicator) &&
	          (local.fix_indicator <= 8);
	if (success) {
	    gga->fix_indicator = local.fix_indicator;
	}
	break;

    case  7: 	/* Satellites in view */
	success = gnss_nmea_field_parse_long(field, &local.satellites_in_view) &&
	    (local.satellites_in_view <= 12);
	if (success) {
	    gga->satellites_in_view = local.satellites_in_view;
	}
	break;

    case  8:	/* Horizontal dilution of precision */
	success = gnss_nmea_field_parse_float(field, &gga->hdop);
	break;

    case  9:	/* Antenna Altitude */
	success = gnss_nmea_field_parse_float(field, &gga->altitude);
	break;

    case 10:	/* Unit */
#if MYNEWT_VAL(GNSS_NMEA_PARSER_VALIDATING) >= 3
	success = gnss_nmea_field_parse_char(field, &local.unit) &&
	          (local.unit == 'M');
#endif
	break;

    case 11:	/* Geoidal separation */
	success = gnss_nmea_field_parse_float(field, &gga->geoid_separation);
	break;

    case 12:	/* Unit */
#if MYNEWT_VAL(GNSS_NMEA_PARSER_VALIDATING) >= 3
	success = gnss_nmea_field_parse_char(field, &local.unit) &&
	          (local.unit == 'M');
#endif
	break;

    case 13:	/* Seconds since last DGPS */
	success = gnss_nmea_field_parse_long(field, &local.dgps_age) &&
	          (local.dgps_age <= 65535);
	if (success) {
	    gga->dgps_age = local.dgps_age;
	}
	break;

    case 14:	/* Referential station for DGPS */
	success = gnss_nmea_field_parse_long(field, &local.dgps_age) &&
	          (local.dgps_age <= 1023);
	if (success) {
	    gga->dgps_sid = local.dgps_sid;
	}
	break;

    default:
	assert(0);
	break;
    }

    return success;
}

void
gnss_nmea_log_gga(struct gnss_nmea_gga *gga)
{
    if (gga->time.present) {
	LOG_INFO(&_gnss_log, LOG_MODULE_DEFAULT,
		 "GGA: Time = %2d:%02d:%02d.%03d\n",
		 gga->time.hours,
		 gga->time.minutes,
		 gga->time.seconds,
		 gga->time.microseconds / 1000);
    }

    LOG_INFO(&_gnss_log, LOG_MODULE_DEFAULT,
	     "GGA: DGPS = %d (%d)\n", gga->dgps_age, gga->dgps_sid);
    LOG_INFO(&_gnss_log, LOG_MODULE_DEFAULT,
	     "GGA: Geoid sep. = %f\n", GNSS_SYSFLOAT(gga->geoid_separation));
    LOG_INFO(&_gnss_log, LOG_MODULE_DEFAULT,
	     "GGA: LatLng = %f, %f\n",
	     GNSS_SYSFLOAT(gga->latitude), GNSS_SYSFLOAT(gga->longitude));
    LOG_INFO(&_gnss_log, LOG_MODULE_DEFAULT,
	     "GGA: Altitude = %f \n", GNSS_SYSFLOAT(gga->altitude));
    LOG_INFO(&_gnss_log, LOG_MODULE_DEFAULT,
	     "GGA: HDOP = %f\n", GNSS_SYSFLOAT(gga->hdop));
    LOG_INFO(&_gnss_log, LOG_MODULE_DEFAULT,
	     "GGA: FIX = %d\n", gga->fix_indicator);
    LOG_INFO(&_gnss_log, LOG_MODULE_DEFAULT,
	     "GGA: Satellites = %d\n", gga->satellites_in_view);
}
