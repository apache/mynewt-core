#include <gnss/nmea.h>
#include <gnss/log.h>

/*
 * See: http://www.catb.org/gpsd/NMEA.html#_gsv_satellites_in_view
 */

bool
gnss_nmea_decoder_gsv(struct gnss_nmea_gsv *gsv, char *field, int fid) {
    bool success = true;
    union {
	long msg_count;
	long msg_idx;
	long total_sats;
	long prn;
	long elevation;
	long azimuth;
	long snr;
    } local;

    switch(fid) {
    case  0:	/* xxGSV */
	break;

    case  1:	/* Total number of messages to be transmitted */
	success = gnss_nmea_field_parse_long(field, &local.msg_count) &&
	          (local.msg_count <= 63);
	if (success) {
	    gsv->msg_count = local.msg_count;
	}
	break;	

    case  2: 	/* 1-origin number of this message within current group */
	success = gnss_nmea_field_parse_long(field, &local.msg_idx) &&
	          (local.msg_idx <= 63);
	if (success) {
	    gsv->msg_idx = local.msg_idx;
	}
	break;

    case  3: 	/* Total number of satellites in view */
	success = gnss_nmea_field_parse_long(field, &local.total_sats) &&
	          (local.total_sats <= 255);
	if (success) {
	    gsv->total_sats = local.total_sats;
	}
	break;

    case  4: 	/* Satelitte PRN number */
    case  8:
    case 12:
    case 16:
	success = gnss_nmea_field_parse_long(field, &local.prn) &&
	          (local.prn <= 255);
	if (success) {
	    gsv->sat_info[(fid-4)%4].prn = local.prn;
	}
	break;

    case  5: 	/* Elevation in degree */
    case  9:
    case 13:
    case 17:
	success = gnss_nmea_field_parse_long(field, &local.elevation) &&
	          (local.elevation <= 90);
	if (success) {
	    gsv->sat_info[(fid-4)%4].elevation = local.elevation;
	}
	break;

    case  6: 	/* Azimuth in degree */
    case 10:
    case 14:
    case 18:
	success = gnss_nmea_field_parse_long(field, &local.azimuth) &&
	          (local.azimuth <= 359);
	if (success) {
	    gsv->sat_info[(fid-4)%4].azimuth = local.azimuth;
	}
	break;

    case  7: 	/* SNR */
    case 11:
    case 15:
    case 19:
	success = gnss_nmea_field_parse_long(field, &local.azimuth) &&
	          (local.azimuth <= 100);
	if (success) {
	    gsv->sat_info[(fid-4)%4].snr = local.snr;
	}
	break;

    default:
	assert(0);
	break;
    }

    return success;
}

void
gnss_nmea_log_gsv(struct gnss_nmea_gsv *gsv)
{
    LOG_INFO(&_gnss_log, LOG_MODULE_DEFAULT,
	     "GSV: Count = %d\n", gsv->msg_count);
    LOG_INFO(&_gnss_log, LOG_MODULE_DEFAULT,
	     "GSV: Idx = %d\n", gsv->msg_idx);
    LOG_INFO(&_gnss_log, LOG_MODULE_DEFAULT,
	     "GSV: Total = %d\n", gsv->total_sats);
    for (int i = 0 ; i < 4 ; i++) {
	    LOG_INFO(&_gnss_log, LOG_MODULE_DEFAULT,
		     "GSV: Satellite  = %d, %d, %d, %d\n",
		     gsv->sat_info[i].prn,
		     gsv->sat_info[i].elevation,
		     gsv->sat_info[i].azimuth,
		     gsv->sat_info[i].snr);
    }
}
