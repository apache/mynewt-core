#include <string.h>
#include <gnss/nmea.h>
#include <gnss/log.h>

static inline
uint16_t gnss_nmea_pgack_lookup(const char *str) {
    int i;
    static struct {
	const char *str;
	uint16_t    msg;
    } lookup_table[] = {
	{ "Command_valid",       GNSS_NMEA_PGACK_COMMAND_VALID            }, 
	{ "No_Change_Data",      GNSS_NMEA_PGACK_DATA_NOT_CHANGED         },
	{ "GetRec_Error",        GNSS_NMEA_PGACK_CONFIGURATION_AREA_ERROR },
	{ "-1",                  GNSS_NMEA_PGACK_COMMAND_FAILED           },
	{ "SW_INI_ANT_INPUT_OK", GNSS_NMEA_PGACK_INIT_OK                  },
	/* Smell like chinese counterfeit */
	{ "Command_vaild",       GNSS_NMEA_PGACK_COMMAND_VALID            },
	{ NULL,                  GNSS_NMEA_PGACK_UNKNOWN                  }
    };

    for (i = 0 ; lookup_table[i].str ; i++) {
	if (!strcmp(str, lookup_table[i].str)) {
	    break;
	}
    }
    return lookup_table[i].msg;
}

int
gnss_nmea_decoder_pgack(struct gnss_nmea_pgack *pgack, char *field, int fid) {
    int rc = 1;
    union {
	long type;
    } local;

    /* Deal with simple cases */
    switch(fid) {
    case  0:	/* PGACK */
	return 1;

    case  1:
	/*  Estimated Position Error */
	if (!strcmp(field, "EPE")) { 
	    pgack->type = GNSS_NMEA_PGACK_TYPE_EPE;

	/* Id (0-999), recycling type for storing information */
	} else if (gnss_nmea_field_parse_long(field, &local.type)) {
	    if (local.type < 1000) {
		pgack->type = local.type;
	    } else {
		return 0;
	    }

	/* Assume mesage without id */
	} else {
	    pgack->type = 0;
	    pgack->msg  = gnss_nmea_pgack_lookup(field);
	}
	return 1;

    case 2:
	/* If we are in message mode */
	if (pgack->type < 1000) {
	    pgack->msg  = gnss_nmea_pgack_lookup(field);
	    return 1;
	}
    }

    /* -- fid >= 2 -- */

    /* If we are in a dedicated mode */
    switch(pgack->type) {
    case GNSS_NMEA_PGACK_TYPE_EPE:
	if ((field[0] == '\0') || (field[1] != '=')) {
	    return -1;
	}
	switch(field[0]) {
	case 'H':
	    rc = gnss_nmea_field_parse_float(&field[2], &pgack->epe.h);
	    break;
	case 'V':
	    rc = gnss_nmea_field_parse_float(&field[2], &pgack->epe.v);
	    break;
	default:
	    rc = 0;
	}
	break;

    default:
	rc = -1;
    }
    
    return rc;
}

void
gnss_nmea_log_pgack(struct gnss_nmea_pgack *pgack)
{
    if (pgack->type < 1000) {
	LOG_INFO(&_gnss_log, LOG_MODULE_DEFAULT,
		 "PGACK[%d]: %d\n", pgack->type, pgack->msg);
    } else if (pgack->type == GNSS_NMEA_PGACK_TYPE_EPE) {
	LOG_INFO(&_gnss_log, LOG_MODULE_DEFAULT,
		 "PGACK[EPE]: h=%f, v=%f\n",
		 GNSS_SYSFLOAT(pgack->epe.h),
		 GNSS_SYSFLOAT(pgack->epe.v));
    } else {
	LOG_INFO(&_gnss_log, LOG_MODULE_DEFAULT,
		 "PGACK: <unknown>\n");
    }
}
