#include <gnss/gnss.h>
#include <gnss/nmea.h>
#include <gnss/log.h>

/*
 * See: http://www.catb.org/gpsd/NMEA.html#_vtg_track_made_good_and_ground_speed
 */

int
gnss_nmea_decoder_vtg(struct gnss_nmea_vtg *vtg, char *field, int fid) {
    int rc = 1;
    union {
	char type;
	char unit;
	gnss_float_t speed;
    } local;

    switch(fid) {
    case  0:	/* xxVTG */
	break;

    case  1:	/* Track (True degrees) */
	rc = gnss_nmea_field_parse_float(field, &vtg->track_true);
	break;	

    case  2: 	/* True (T) */
#if MYNEWT_VAL(GNSS_NMEA_PARSER_VALIDATING) >= 3
	rc = gnss_nmea_field_parse_char(field, &local.type);
	if (rc <= 0) {
	    break;
	}
	if (local.type != 'T') {
	    rc = 0;
	}
#endif
	break;

    case  3:	/* Track (Magnetic degrees) */
	rc = gnss_nmea_field_parse_float(field, &vtg->track_magnetic);
	break;	

    case  4: 	/* Magnetic (M) */
#if MYNEWT_VAL(GNSS_NMEA_PARSER_VALIDATING) >= 3
	rc = gnss_nmea_field_parse_char(field, &local.type);
	if (rc <= 0) {
	    break;
	}
	if (local.type != 'M') {
	    rc = 0;
	}
#endif
	break;

    case  5: 	/* Speed */
        rc = gnss_nmea_field_parse_float(field, &local.speed);
	if (rc <= 0) {
	    break;
	}
	vtg->speed = _gnss_nmea_knot_to_mps(local.speed);
        break;

    case  6:    /* Knots (N) */
#if MYNEWT_VAL(GNSS_NMEA_PARSER_VALIDATING) >= 3
        rc = gnss_nmea_field_parse_char(field, &local.unit);
	if (rc <= 0) {
	    break;
	}
	if (local.unit != 'N') {
	    rc = 0;
	}
#endif
        break;

    case  7:    /* Speed */
        rc = gnss_nmea_field_parse_float(field, &local.speed);
	if (rc <= 0) {
	    break;
	}
	if ((vtg->speed == GNSS_FLOAT_0) && (local.speed != GNSS_FLOAT_0)) {
	    vtg->speed = _gnss_nmea_kmph_to_mps(local.speed);
	}
        break;

    case  8:    /* Km/h (K) */
#if MYNEWT_VAL(GNSS_NMEA_PARSER_VALIDATING) >= 3
	rc = gnss_nmea_field_parse_char(field, &local.unit);
	if (rc <= 0) {
	    break;
	}
	if (local.unit != 'K') {
	    rc = 0;
	}
#endif
	break;

    case  9:	/* FAA mode */
	rc = gnss_nmea_field_parse_char(field, &vtg->faa_mode);
	break;

    default:
	assert(0);
	break;
    }

    return rc;
}

void
gnss_nmea_log_vtg(struct gnss_nmea_vtg *vtg)
{
    GNSS_LOG_INFO("VTG: Track = %f°[T] | %f°[M] / Speed = %f m/s / FAA = %c\n",
		  GNSS_SYSFLOAT(vtg->track_true),
		  GNSS_SYSFLOAT(vtg->track_magnetic),
		  GNSS_SYSFLOAT(vtg->speed),
		  vtg->faa_mode);
}
