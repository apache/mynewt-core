#include <string.h>
#include <gnss/gnss.h>
#include <gnss/nmea.h>
#include <gnss/log.h>

static inline int8_t
_gnss_nmea_decoder_pubx_lookup_position_status(char *str)
{
    static struct {
	char   name[3];
	int8_t value;
    } map [] = {
	{ "NF", GNSS_NMEA_PUBX_POSITION_STATUS_NO_FIX                 },
	{ "DR", GNSS_NMEA_PUBX_POSITION_STATUS_DEAD_RECKONING         },
	{ "G2", GNSS_NMEA_PUBX_POSITION_STATUS_STANDALONE_2D          },
	{ "G3", GNSS_NMEA_PUBX_POSITION_STATUS_STANDALONE_3D          },
	{ "D2", GNSS_NMEA_PUBX_POSITION_STATUS_DIFFERENTIAL_2D        },
	{ "D3", GNSS_NMEA_PUBX_POSITION_STATUS_DIFFERENTIAL_3D        },
	{ "RK", GNSS_NMEA_PUBX_POSITION_STATUS_GPS_AND_DEAD_RECKONING },
	{ "TT", GNSS_NMEA_PUBX_POSITION_STATUS_TIME_ONLY              },
    };

    int i;
    for (i = 0 ; i < sizeof(map) / sizeof(map[0]) ; i++) {
	if (!strcmp(str, map[i].name)) {
	    return map[i].value;
	}
    }
    return -1;
}

static inline int
gnss_nmea_decoder_pubx_config(struct gnss_nmea_pubx *pubx, char *field, int fid) {
    int rc = 1;
    union {
	long   port_id;
	long   in_proto;
	long   out_proto;
	long   baudrate;
	long   autobauding;
    } local;

    switch(fid) {
    case  2:	/* Port ID */
	rc = gnss_nmea_field_parse_long(field, &local.port_id);
	if (rc <= 0) {
	    break;
	}
#if MYNEWT_VAL(GNSS_NMEA_PARSER_VALIDATING) >= 2
	if (local.msg_id > 255) {
	    rc = 0;
	    break;
	}
#endif
	pubx->config.port_id = local.port_id;
	break;

    case  3:	/* Input protocol mask */
	rc = gnss_nmea_field_parse_long(field, &local.in_proto);
	if (rc <= 0) {
	    break;
	}
#if MYNEWT_VAL(GNSS_NMEA_PARSER_VALIDATING) >= 2
	if (local.in_proto > 0xFFFF) {
	    rc = 0;
	    break;
	}
#endif
	pubx->config.in_proto = local.in_proto;
	break;

    case  4:	/* Output protocol mask */
	rc = gnss_nmea_field_parse_long(field, &local.out_proto);
	if (rc <= 0) {
	    break;
	}
#if MYNEWT_VAL(GNSS_NMEA_PARSER_VALIDATING) >= 2
	if (local.out_proto > 0xFFFF) {
	    rc = 0;
	    break;
	}
#endif
	pubx->config.out_proto = local.out_proto;
	break;

    case  5:	/* Baud rate */
	rc = gnss_nmea_field_parse_long(field, &local.baudrate);
	if (rc <= 0) {
	    break;
	}
#if MYNEWT_VAL(GNSS_NMEA_PARSER_VALIDATING) >= 2
	if (local.baudrate <= 0) {
	    rc = 0;
	    break;
	}
#endif
	pubx->config.baudrate = local.baudrate;
	break;

    case  6:	/* Autobauding */
	rc = gnss_nmea_field_parse_long(field, &local.autobauding);
	if (rc <= 0) {
	    break;
	}
	pubx->config.autobauding = local.autobauding;
	break;

    case 0:
    case 1:
    default:
	assert(0);
	break;
    }

    return rc;
}

static inline int
gnss_nmea_decoder_pubx_position(struct gnss_nmea_pubx *pubx, char *field, int fid) {
    int rc = 1;
    union {
	long         dgps_age;
	long         sat_count;
	long         dead_reckoning;
	gnss_float_t speed;
	struct {
	    char   name[3];
	    int8_t val;
	} status;
    } local;

    switch(fid) {
    case  2:	/* UTC Time */
	rc = gnss_nmea_field_parse_time(field, &pubx->position.time);
	break;

    case  3:	/* Latitude */
	rc = gnss_nmea_field_parse_latlng(field, &pubx->position.latitude);
	break;	

    case  4: 	/* N/S indicator */
	rc = gnss_nmea_field_parse_and_apply_direction(field, &pubx->position.latitude);
	break;

    case  5: 	/* Longitude */
	rc = gnss_nmea_field_parse_latlng(field, &pubx->position.longitude);
	break;

    case  6: 	/* E/W indicator */
	rc = gnss_nmea_field_parse_and_apply_direction(field, &pubx->position.longitude);
	break;

    case  7:	/* Altitude (above user datum) */
	rc = gnss_nmea_field_parse_float(field, &pubx->position.altitude);
	break;

    case 8:     /* Navigation status */
	rc = gnss_nmea_field_parse_string(field, local.status.name, 3);
	if (rc <= 0) {
	    break;
	}
	local.status.val = _gnss_nmea_decoder_pubx_lookup_position_status(local.status.name);
	if (local.status.val >= 0) {
	    pubx->position.status = local.status.val;
	} else {
	    rc = 0;
	}
	break;

    case 9:     /* Horizontal accuracy estimate */
	rc = gnss_nmea_field_parse_float(field, &pubx->position.hacc);
	break;

    case 10:    /* Vertical accuracy estimate */
	rc = gnss_nmea_field_parse_float(field, &pubx->position.vacc);
	break;

    case 11:    /* Speed over ground (km/s) */
	rc = gnss_nmea_field_parse_float(field, &local.speed);
	if (rc <= 0) {
	    break;
	}
	pubx->position.speed = _gnss_nmea_kmph_to_mps(local.speed);
        break;
	
    case 12:    /* Course over ground (deg) */
        rc = gnss_nmea_field_parse_float(field, &pubx->position.track);
        break;
	
    case 13:    /* Vertical velocity (m/s) */
        rc = gnss_nmea_field_parse_float(field, &pubx->position.velocity);
        break;

    case 14:	/* Seconds since last DGPS */
	rc = gnss_nmea_field_parse_long(field, &local.dgps_age);
	if (rc <= 0) {
	    break;
	}
#if MYNEWT_VAL(GNSS_NMEA_PARSER_VALIDATING) >= 2
	if (local.dgps_age > 0xFFFF) {
	    rc = 0;
	    break;
	}
#endif
	pubx->position.dgps_age = local.dgps_age;
	break;
	
    case 15:	/* VDOP */
	rc = gnss_nmea_field_parse_float(field, &pubx->position.vdop);
	break;

    case 16:	/* HDOP */
	rc = gnss_nmea_field_parse_float(field, &pubx->position.hdop);
	break;

    case 17:	/* TDOP */
	rc = gnss_nmea_field_parse_float(field, &pubx->position.tdop);
	break;
	
    case 18:	/* Number of GPS satelites used */
	rc = gnss_nmea_field_parse_long(field, &local.sat_count);
	if (rc <= 0) {
	    break;
	}	
#if MYNEWT_VAL(GNSS_NMEA_PARSER_VALIDATING) >= 2
	if (local.sat_count > 24) {
	    rc = 0;
	    break;
	}
#endif
	pubx->position.gps_used = local.sat_count;
	break;

    case 19:	/* Number of Glonass satelites used */
	rc = gnss_nmea_field_parse_long(field, &local.sat_count);
	if (rc <= 0) {
	    break;
	}	
#if MYNEWT_VAL(GNSS_NMEA_PARSER_VALIDATING) >= 2
	if (local.sat_count > 24) {
	    rc = 0;
	    break;
	}
#endif
	pubx->position.glonass_used = local.sat_count;
	break;

    case 20:    /* DR (dead reckoning?) used */
	rc = gnss_nmea_field_parse_long(field, &local.dead_reckoning);
	if (rc <= 0) {
	    break;
	}	
	pubx->position.dead_reckoning = local.dead_reckoning > 0;
	break;
	
    case 0:
    case 1:
    default:
	assert(0);
	break;
    }

    return rc;
}

static inline int
gnss_nmea_decoder_pubx_rate(struct gnss_nmea_pubx *pubx, char *field, int fid) {
    int rc = 1;
    union {
	long rate;
    } local;
    uint8_t *rate = NULL;
	
    switch(fid) {
    case 2: rate = &pubx->rate.ddc;    break;
    case 3: rate = &pubx->rate.usart1; break;
    case 4: rate = &pubx->rate.usart2; break;
    case 5: rate = &pubx->rate.usb;    break;
    case 6: rate = &pubx->rate.spi;    break;
    }
    
    if (rate) {
	rc = gnss_nmea_field_parse_long(field, &local.rate);
	if (rc <= 0) {
	    return rc;
	}
#if MYNEWT_VAL(GNSS_NMEA_PARSER_VALIDATING) >= 2
	if (local.rate > 0xFF) {
	    return 0;
	}
#endif
	*rate = local.rate;
    } else {
	assert(0);
    }

    return rc;
}

static inline int
gnss_nmea_decoder_pubx_svstatus(struct gnss_nmea_pubx *pubx, char *field, int fid) {
    return 0;
}

static inline int
gnss_nmea_decoder_pubx_time(struct gnss_nmea_pubx *pubx, char *field, int fid) {
    int rc = 1;

    switch(fid) {
    case 2:  /* UTC Time */
	rc = gnss_nmea_field_parse_time(field, &pubx->time.time);
	break;

    case 3:  /* UTC Date */
	rc = gnss_nmea_field_parse_date(field, &pubx->time.date);
	break;
	
    case 4:  /* UTC Time of Week */
	/* Not Implemented Yet */
	break;

    case 5:  /* UTC Week number */
	/* Not Implemented Yet */
	break;

    case 6:  /* Leap seconds */
	/* Not Implemented Yet */
	break;

    case 7:  /* Receiver clock bias */
	/* Not Implemented Yet */
	break;

    case 8:  /* Receiver clock drift */
	/* Not Implemented Yet */
	break;

    case 9:  /* Time pulse granularity */
	/* Not Implemented Yet */
	break;

    case 0:
    case 1:
    default:
	assert(0);
	break;
    }
    
    return rc;
}

int
gnss_nmea_decoder_pubx(struct gnss_nmea_pubx *pubx, char *field, int fid) {
    int rc = 1;
    union {
	long   msg_id;
    } local;

    switch(fid) {
    case  0:	/* PUBX */
	break;

    case  1:	/* Msg Id */
	rc = gnss_nmea_field_parse_long(field, &local.msg_id);
	if (rc <= 0) {
	    break;
	}
#if MYNEWT_VAL(GNSS_NMEA_PARSER_VALIDATING) >= 1
	if (local.msg_id > 255) {
	    rc = 0;
	    break;
	}
#endif
	pubx->type =  local.msg_id;
	break;

    default:    /* Dispatch to specific pubx parser */
	switch(pubx->type) {
	case GNSS_NMEA_PUBX_TYPE_CONFIG:
	    rc = gnss_nmea_decoder_pubx_config(pubx, field, fid);
	    break;
	case GNSS_NMEA_PUBX_TYPE_POSITION:
	    rc = gnss_nmea_decoder_pubx_position(pubx, field, fid);
	    break;
	case GNSS_NMEA_PUBX_TYPE_RATE:
	    rc = gnss_nmea_decoder_pubx_rate(pubx, field, fid);
	    break;
	case GNSS_NMEA_PUBX_TYPE_SVSTATUS:
	    rc = gnss_nmea_decoder_pubx_svstatus(pubx, field, fid);
	    break;
	case GNSS_NMEA_PUBX_TYPE_TIME:
	    rc = gnss_nmea_decoder_pubx_time(pubx, field, fid);
	    break;
	default:
	    rc = 0;
	    break;
	}
	break;
    }

    return rc;
}

void
gnss_nmea_log_pubx(struct gnss_nmea_pubx *pubx)
{
    switch(pubx->type) {
    case GNSS_NMEA_PUBX_TYPE_CONFIG:
	break;

    case GNSS_NMEA_PUBX_TYPE_POSITION:
	LOG_INFO(&_gnss_log, LOG_MODULE_DEFAULT,
		 "PUBX[%02d|position]: "
		 "Time = %2d:%02d:%02d.%03d\n",
		 pubx->type,
		 pubx->position.time.hours,
		 pubx->position.time.minutes,
		 pubx->position.time.seconds,
		 pubx->position.time.microseconds / 1000);
	
	LOG_INFO(&_gnss_log, LOG_MODULE_DEFAULT,
		 "PUBX[%02d|position]: "
		 "LatLng = %f, %f; Alt=%f\n",
		 pubx->type,
		 GNSS_SYSFLOAT(pubx->position.latitude),
		 GNSS_SYSFLOAT(pubx->position.longitude),
		 GNSS_SYSFLOAT(pubx->position.altitude)
		 );
	
	LOG_INFO(&_gnss_log, LOG_MODULE_DEFAULT,
		 "PUBX[%02d|position]: "
		 "Track = %f° | %f m/s\n",
		 pubx->type,
		 GNSS_SYSFLOAT(pubx->position.track),
		 GNSS_SYSFLOAT(pubx->position.speed));
	
	LOG_INFO(&_gnss_log, LOG_MODULE_DEFAULT,
		 "PUBX[%02d|position]: "
		 "HDOP = %f / VDOP = %f / TDOP = %f\n",
		 pubx->type,
		 GNSS_SYSFLOAT(pubx->position.hdop),
		 GNSS_SYSFLOAT(pubx->position.vdop),
		 GNSS_SYSFLOAT(pubx->position.tdop));
	
	LOG_INFO(&_gnss_log, LOG_MODULE_DEFAULT,
		 "PUBX[%02d|position]: "
		 "HACC = %f / VACC = %f\n",
		 pubx->type,
		 GNSS_SYSFLOAT(pubx->position.hacc),
		 GNSS_SYSFLOAT(pubx->position.vacc));
	break;
	
    case GNSS_NMEA_PUBX_TYPE_RATE:
	LOG_INFO(&_gnss_log, LOG_MODULE_DEFAULT,
		 "PUBX[%02d|rate]: "
		 "DDC=%d / USART1=%d / USART2=%d / USB=%d / SPI=%d\n",
		 pubx->type,
		 pubx->rate.ddc,
		 pubx->rate.usart1,
		 pubx->rate.usart2,
		 pubx->rate.usb,
		 pubx->rate.spi);
	break;
	
    case GNSS_NMEA_PUBX_TYPE_SVSTATUS:
	LOG_INFO(&_gnss_log, LOG_MODULE_DEFAULT,
		 "PUBX[%02d|svstatus]: <not implemented yet>\n");
	break;

    case GNSS_NMEA_PUBX_TYPE_TIME:
	if (pubx->time.date.present) {
	    LOG_INFO(&_gnss_log, LOG_MODULE_DEFAULT,
		     "PUBX[%02d|time]: "
		     "Date = %2d-%02d-%02d\n",
		     pubx->type,
		     pubx->time.date.year,
		     pubx->time.date.month,
		     pubx->time.date.day);
	}
	if (pubx->time.time.present) {
	    LOG_INFO(&_gnss_log, LOG_MODULE_DEFAULT,
		     "PUBX[%02d|time]: "
		     "Time = %2d:%02d:%02d.%03d\n",
		     pubx->type,
		     pubx->time.time.hours,
		     pubx->time.time.minutes,
		     pubx->time.time.seconds,
		     pubx->time.time.microseconds / 1000);
	}
	break;
	
    default:
	LOG_INFO(&_gnss_log, LOG_MODULE_DEFAULT,
		 "PUBX[%02d]: <unknown>\n", pubx->type);
	break;
    }
}
