#include <gnss/gnss.h>
#include <gnss/nmea.h>
#include <gnss/log.h>

/*
 * See: http://www.catb.org/gpsd/NMEA.html#_vtg_track_made_good_and_ground_speed
 */

bool
gnss_nmea_decoder_vtg(struct gnss_nmea_vtg *vtg, char *field, int fid) {
    bool success = true;
    union {
	char type;
	char unit;
	gnss_float_t speed;
    } local;

    switch(fid) {
    case  0:	/* xxVTG */
	break;

    case  1:	/* Track (True degrees) */
	success = gnss_nmea_field_parse_float(field, &vtg->track_true);
	break;	

    case  2: 	/* True (T) */
#if MYNEWT_VAL(GNSS_NMEA_PARSER_VALIDATING) >= 3
	success = gnss_nmea_field_parse_char(field, &local.type) &&
	          (local.type == 'T');
#endif
	break;

    case  3:	/* Track (Magnetic degrees) */
	success = gnss_nmea_field_parse_float(field, &vtg->track_magnetic);
	break;	

    case  4: 	/* Magnetic (M) */
#if MYNEWT_VAL(GNSS_NMEA_PARSER_VALIDATING) >= 3
	success = gnss_nmea_field_parse_char(field, &local.type) &&
	          (local.type == 'M');
#endif
	break;

    case  5: 	/* Speed */
        success = gnss_nmea_field_parse_float(field, &local.speed);
        if (success) {
            vtg->speed = _gnss_nmea_knot_to_mps(local.speed);
        }
        break;

    case  6:    /* Knots (N) */
#if MYNEWT_VAL(GNSS_NMEA_PARSER_VALIDATING) >= 3
        success = gnss_nmea_field_parse_char(field, &local.unit) &&
                  (local.unit == 'N');
#endif
        break;

    case  7:    /* Speed */
        success = gnss_nmea_field_parse_float(field, &local.speed);
        if (success) {
            if ((vtg->speed == GNSS_FLOAT_0) && (local.speed != GNSS_FLOAT_0)) {
                vtg->speed = _gnss_nmea_kmph_to_mps(local.speed);
            }
        }
        break;

    case  8:    /* Km/h (K) */
#if MYNEWT_VAL(GNSS_NMEA_PARSER_VALIDATING) >= 3
	success = gnss_nmea_field_parse_char(field, &local.unit) &&
	          (local.unit == 'K');
#endif
	break;

    case  9:	/* FAA mode */
	success = gnss_nmea_field_parse_char(field, &vtg->faa_mode);
	break;

    default:
	assert(0);
	break;
    }

    return success;
}

void
gnss_nmea_log_vtg(struct gnss_nmea_vtg *vtg)
{
    LOG_INFO(&_gnss_log, LOG_MODULE_DEFAULT,
	     "VTG: Track = %f°[T] | %f°[M] / Speed = %f m/s / FAA = %c\n",
	     GNSS_SYSFLOAT(vtg->track_true),
	     GNSS_SYSFLOAT(vtg->track_magnetic),
	     GNSS_SYSFLOAT(vtg->speed),
	     vtg->faa_mode);
}
