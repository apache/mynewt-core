#include <gnss/nmea.h>
#include <gnss/log.h>

/*
 * See: http://www.catb.org/gpsd/NMEA.html#_gst_gps_pseudorange_noise_statistics
 */

bool
gnss_nmea_decoder_gst(struct gnss_nmea_gst *gst, char *field, int fid) {
    bool success = true;

    switch(fid) {
    case  0:	/* xxGST */
	break;

    case  1:	/* UTC Time of associated fix */
	success = gnss_nmea_field_parse_time(field, &gst->time);
	break;

    case  2:	/* RMS standard deviation */
	success = gnss_nmea_field_parse_float(field, &gst->rms_stddev);
	break;

    case  3:	/* Standard deviation  of semi-major axis of error ellipse */
	success = gnss_nmea_field_parse_float(field, &gst->semi_major_stddev);
	break;	

    case  4: 	/* Standard deviation of semi-minor axis of error ellipse */
	success = gnss_nmea_field_parse_float(field, &gst->semi_minor_stddev);
	break;

    case  5: 	/* Orientation of semi-major axis of error ellipse */
	success = gnss_nmea_field_parse_float(field, &gst->semi_major_orientation);
	break;

    case  6: 	/* Standard deviation of latitude error */
	success = gnss_nmea_field_parse_float(field, &gst->latitude_stddev);
	break;

    case  7:	/* Standard deviation of longitude error */
	success = gnss_nmea_field_parse_float(field, &gst->longitude_stddev);
	break;

    case  8: 	/* Course over ground */
	success = gnss_nmea_field_parse_float(field, &gst->altitude_stddev);
	break;

    default:
	assert(0);
	break;
    }

    return success;
}

void
gnss_nmea_log_gst(struct gnss_nmea_gst *gst)
{
    if (gst->time.present) {
	LOG_INFO(&_gnss_log, LOG_MODULE_DEFAULT,
		 "GST: Time = %2d:%02d:%02d.%03d / "
		 "RMS = %f / "
		 "SM = %f (%f°) / "
		 "Sm = %f / "
		 "Lat = %f / Lng = %f / Alt = %f\n",
		 gst->time.hours,
		 gst->time.minutes,
		 gst->time.seconds,
		 gst->time.microseconds / 1000,
		 GNSS_SYSFLOAT(gst->rms_stddev),
		 GNSS_SYSFLOAT(gst->semi_major_stddev),
		 GNSS_SYSFLOAT(gst->semi_major_orientation),
		 GNSS_SYSFLOAT(gst->semi_minor_stddev),
		 GNSS_SYSFLOAT(gst->latitude_stddev),
		 GNSS_SYSFLOAT(gst->longitude_stddev),
		 GNSS_SYSFLOAT(gst->altitude_stddev));
    } else {
	LOG_INFO(&_gnss_log, LOG_MODULE_DEFAULT,
		 "GST: <no valid output>\n");
    }
}
