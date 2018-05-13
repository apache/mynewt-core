#include <gnss/nmea.h>
#include <gnss/log.h>

/*
 * See: http://www.catb.org/gpsd/NMEA.html#_gsv_satellites_in_view
 */

int
gnss_nmea_decoder_gsv(struct gnss_nmea_gsv *gsv, char *field, int fid) {
    int rc = 1;
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
	rc = gnss_nmea_field_parse_long(field, &local.msg_count);
	if (rc <= 0) {
	    break;
	}
	if (local.msg_count <= 63) {
	    gsv->msg_count = local.msg_count;
	} else {
	    rc = 0;
	}
	break;	

    case  2: 	/* 1-origin number of this message within current group */
	rc = gnss_nmea_field_parse_long(field, &local.msg_idx);
	if (rc <= 0) {
	    break;
	}
	if (local.msg_idx <= 63) {
	    gsv->msg_idx = local.msg_idx;
	} else {
	    rc = 0;
	}
	break;

    case  3: 	/* Total number of satellites in view */
	rc = gnss_nmea_field_parse_long(field, &local.total_sats);
	if (rc <= 0) {
	    break;
	}
	if (local.total_sats <= 255) {
	    gsv->total_sats = local.total_sats;
	} else {
	    rc = 0;
	}
	break;

    case  4: 	/* Satelitte PRN number */
    case  8:
    case 12:
    case 16:
	rc = gnss_nmea_field_parse_long(field, &local.prn);
	if (rc <= 0) {
	    break;
	}
	if (local.prn <= 255) {
	    gsv->sat_info[(fid-4)%4].prn = local.prn;
	} else {
	    rc = 0;
	}
	break;

    case  5: 	/* Elevation in degree */
    case  9:
    case 13:
    case 17:
	rc = gnss_nmea_field_parse_long(field, &local.elevation);
	if (rc <= 0) {
	    break;
	}
	if (local.elevation <= 90) {
	    gsv->sat_info[(fid-4)%4].elevation = local.elevation;
	} else {
	    rc = 0;
	}
	break;

    case  6: 	/* Azimuth in degree */
    case 10:
    case 14:
    case 18:
	rc = gnss_nmea_field_parse_long(field, &local.azimuth);
	if (rc <= 0) {
	    break;
	}
	if (local.azimuth <= 359) {
	    gsv->sat_info[(fid-4)%4].azimuth = local.azimuth;
	} else {
	    rc = 0;
	}
	break;

    case  7: 	/* SNR */
    case 11:
    case 15:
    case 19:
	rc = gnss_nmea_field_parse_long(field, &local.azimuth);
	if (rc <= 0) {
	    break;
	}
	if (local.azimuth <= 100) {
	    gsv->sat_info[(fid-4)%4].snr = local.snr;
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
gnss_nmea_log_gsv(struct gnss_nmea_gsv *gsv)
{
    GNSS_LOG_INFO("GSV: Count = %d\n", gsv->msg_count);
    GNSS_LOG_INFO("GSV: Idx = %d\n", gsv->msg_idx);
    GNSS_LOG_INFO("GSV: Total = %d\n", gsv->total_sats);
    for (int i = 0 ; i < 4 ; i++) {
	GNSS_LOG_INFO("GSV: Satellite  = %d, %d, %d, %d\n",
		      gsv->sat_info[i].prn,
		      gsv->sat_info[i].elevation,
		      gsv->sat_info[i].azimuth,
		      gsv->sat_info[i].snr);
    }
}
