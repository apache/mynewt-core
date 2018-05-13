#include <gnss/nmea.h>
#include <gnss/log.h>

/*
 * SEE: http://www.catb.org/gpsd/NMEA.html#_gsa_gps_dop_and_active_satellites
 */

int
gnss_nmea_decoder_gsa(struct gnss_nmea_gsa *gsa, char *field, int fid) {
    int rc = 1;
    union {
	long fix_mode;
	long sid;
    } local;
    
    switch(fid) {
    case  0:	/* xxGSA */
	break;

    case  1:	/* Selection Mode */
	rc = gnss_nmea_field_parse_char(field, &gsa->fix_mode_selection);
	break;

    case  2:	/* Mode */
	rc = gnss_nmea_field_parse_long(field, &local.fix_mode);
	if (rc <= 0) {
	    break;
	}
	if (local.fix_mode <= 3) {
	    gsa->fix_mode = local.fix_mode;
	} else {
	    rc = 0;
	}
	break;

    case  3:	/* ID of satelitte used for fix */
    case  4:
    case  5:
    case  6:
    case  7:
    case  8:
    case  9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
	rc = gnss_nmea_field_parse_long(field, &local.sid);
	if (rc <= 0) {
	    break;
	}
	if (local.sid <= 255) {
	    gsa->sid[fid-3] = local.sid;
	} else {
	    rc = 0;
	}
	break;

    case 15:	/* PDOP */
	rc = gnss_nmea_field_parse_float(field, &gsa->pdop);
	break;

    case 16:	/* HDOP */
	rc = gnss_nmea_field_parse_float(field, &gsa->hdop);
	break;

    case 17:	/* VDOP */
	rc = gnss_nmea_field_parse_float(field, &gsa->vdop);
	break;
	
    default:
	assert(0);
	break;
    }

    return rc;
}

void
gnss_nmea_log_gsa(struct gnss_nmea_gsa *gsa)
{
    if (gsa->fix_mode > GNSS_NMEA_FIX_MODE_NO) {
	GNSS_LOG_INFO("GSA: "
		      "FIX = %dD / "
		      "PDOP = %f / HDOP = %f / VDOP = %f / "
		      "Sats=%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d"
		      "\n",
		      gsa->fix_mode,
		      GNSS_SYSFLOAT(gsa->pdop),
		      GNSS_SYSFLOAT(gsa->hdop),
		      GNSS_SYSFLOAT(gsa->vdop),
		      gsa->sid[ 0], gsa->sid[ 1],
		      gsa->sid[ 2], gsa->sid[ 3],
		      gsa->sid[ 4], gsa->sid[ 5],
		      gsa->sid[ 6], gsa->sid[ 7],
		      gsa->sid[ 8], gsa->sid[ 9],
		      gsa->sid[10], gsa->sid[11]);
    } else {
	GNSS_LOG_INFO("GSA: <no fix>\n");
    }
}
