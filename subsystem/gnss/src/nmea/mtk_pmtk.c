#include <string.h>
#include <gnss/nmea.h>
#include <gnss/log.h>

int
gnss_nmea_decoder_pmtk(struct gnss_nmea_pmtk *pmtk, char *field, int fid) {
    int rc = 1;
    union {
	long sys_msg;
	long cmd;
	long status;
    } local;

    if (fid == 0) { /* PMTK */
	pmtk->type = strtoul(&field[4], NULL, 10);
	return 1;
    }

    switch(pmtk->type) {
    case GNSS_NMEA_PMTK_TYPE_ACK:
	switch(fid) {
	case 1:	    /* Cmd */
	    rc = gnss_nmea_field_parse_long(field, &local.cmd);
	    if (rc <= 0) {
		break;
	    }
	    if (local.cmd <= 1000) {
		pmtk->ack.cmd = local.cmd;
	    } else {
		rc = 0;
	    }
	    break;

	case 2:	    /* Status */
	    rc = gnss_nmea_field_parse_long(field, &local.status);
	    if (rc <= 0) {
		break;
	    }
	    if (local.status <= 3) {
		pmtk->ack.status = local.status;
	    } else {
		rc = 0;
	    }
	    break;

	default:
	    /* Ignore other fields */
	    break;
	}
	break;
	
    case GNSS_NMEA_PMTK_TYPE_SYS_MSG:
	switch(fid) {
	case 1:	    /* Systeme message */
	    rc = gnss_nmea_field_parse_long(field, &local.sys_msg);
	    if (rc <= 0) {
		break;
	    }
	    if (local.sys_msg <= 3) {
		pmtk->sys_msg = local.sys_msg;
	    } else {
		rc = 0;
	    }
	    break;

	default:
	    /* Ignore other fields */
	    break;
	}

    case GNSS_NMEA_PMTK_TYPE_TXT_MSG:
	switch(fid) {
	case 1:	    /* Systeme text message */
	    strncpy(pmtk->txt_msg, field, sizeof(pmtk->txt_msg));
	    pmtk->txt_msg[sizeof(pmtk->txt_msg) - 1] = '\0'; 
	    break;

	default:
	    /* Ignore other fields */
	    break;
	}

    default:
	rc = 0;
	break;
    }
    
    return rc;
}

void
gnss_nmea_log_pmtk(struct gnss_nmea_pmtk *pmtk)
{
    static char *status = "???";
    
    switch(pmtk->type) {
    case GNSS_NMEA_PMTK_TYPE_ACK:
	switch(pmtk->ack.status) {
	case GNSS_NMEA_PMTK_ACK_INVALID_COMMAND:
	    status = "Invalid";
	    break;
	case GNSS_NMEA_PMTK_ACK_UNSUPPORTED_COMMAND:
	    status = "Unsupported";
	    break;
	case GNSS_NMEA_PMTK_ACK_ACTION_FAILED:
	    status = "Failed";
	    break;
	case GNSS_NMEA_PMTK_ACK_ACTION_SUCCESSFUL:
	    status = "Successful";
	    break;
	}
	GNSS_LOG_INFO("PMTK[ACK]: Cmd = %d, Status = %s\n",
		      pmtk->ack.cmd, status);
	break;

    case GNSS_NMEA_PMTK_TYPE_SYS_MSG:
	switch(pmtk->sys_msg) {
	case GNSS_NMEA_PMTK_SYS_MSG_UNKNOWN:
	    status = "Unknown";
	    break;
	case GNSS_NMEA_PMTK_SYS_MSG_STARTUP:
	    status = "Startup";
	    break;
	case GNSS_NMEA_PMTK_SYS_MSG_EPO:
	    status = "EPO";
	    break;
	case GNSS_NMEA_PMTK_SYS_MSG_NORMAL:
	    status = "Normal";
	    break;
	}	    
	GNSS_LOG_INFO("PMTK[SYS_MSG]: Status = %s\n", status);
	break;

    case GNSS_NMEA_PMTK_TYPE_TXT_MSG:
	GNSS_LOG_INFO("PMTK[TXT_MSG]: %s\n",
		      pmtk->txt_msg);
	break;

    default:
	GNSS_LOG_INFO("PMTK: <unknown>\n");
	break;
    }
}
