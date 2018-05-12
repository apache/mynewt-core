#ifndef _GNSS_NMEA_MEDIATEK_H_
#define _GNSS_NMEA_MEDIATEK_H_


/**
 * Unknown message (parser need to be improved)
 */
#define GNSS_NMEA_PGACK_UNKNOWN			        0
/**
 * Command Valid
 */
#define GNSS_NMEA_PGACK_COMMAND_VALID			1
/**
 * Command Failed
 */
#define GNSS_NMEA_PGACK_COMMAND_FAILED			2
/**
 * Data not changed
 */
#define GNSS_NMEA_PGACK_DATA_NOT_CHANGED		3
/**
 * Configuration area error
 */
#define GNSS_NMEA_PGACK_CONFIGURATION_AREA_ERROR	4
/**
 * Initialization successful
 */
#define GNSS_NMEA_PGACK_INIT_OK				5


/**
 * Unknown notification
 */
#define GNSS_NMEA_PMTK_SYS_MSG_UNKNOWN 			0
/**
 * Startup notification
 */
#define GNSS_NMEA_PMTK_SYS_MSG_STARTUP 			1
/**
 * Notification for the host aiding EPO
 */
#define GNSS_NMEA_PMTK_SYS_MSG_EPO 			2
/**
 * Notification for the transition to Normal mode was successful
 */
#define GNSS_NMEA_PMTK_SYS_MSG_NORMAL 			3 


/**
 * Invalid command
 */
#define GNSS_NMEA_PMTK_ACK_INVALID_COMMAND		0
/**
 * Unsupported command
 */
#define GNSS_NMEA_PMTK_ACK_UNSUPPORTED_COMMAND		1
/**
 * Valid command but action failed
 */
#define GNSS_NMEA_PMTK_ACK_ACTION_FAILED 		2
/**
 * Valid command and action succeeded
 */
#define GNSS_NMEA_PMTK_ACK_ACTION_SUCCESSFUL		3 


/**
 * MediaTek, PGACK
 */
struct gnss_nmea_pgack {
    uint16_t type;
    union {
	uint8_t  msg;
	struct {
	    gnss_float_t h;
	    gnss_float_t v;
	} epe;
    };
};

#define GNSS_NMEA_PGACK_TYPE_EPE	(0x8000 | 1)


/**
 * MediaTek, PMTK
 */
struct gnss_nmea_pmtk {
    uint16_t type;
    union {
	uint8_t sys_msg;
	char txt_msg[16];
	struct {
	    uint16_t cmd;
	    uint8_t  status;
	} ack;
    };
};

#define GNSS_NMEA_PMTK_TYPE_ACK			1
#define GNSS_NMEA_PMTK_TYPE_SYS_MSG	       10
#define GNSS_NMEA_PMTK_TYPE_TXT_MSG	       11


#if MYNEWT_VAL(GNSS_NMEA_USE_PGACK) > 0
int gnss_nmea_decoder_pgack(struct gnss_nmea_pgack *pgack, char *field, int fid);
#endif

#if MYNEWT_VAL(GNSS_NMEA_USE_PMTK) > 0
int gnss_nmea_decoder_pmtk(struct gnss_nmea_pmtk *pmtk, char *field, int fid);
#endif


#if MYNEWT_VAL(GNSS_NMEA_USE_PGACK) > 0
void gnss_nmea_log_pgack(struct gnss_nmea_pgack *pgack);
#endif
#if MYNEWT_VAL(GNSS_NMEA_USE_PMTK) > 0
void gnss_nmea_log_pmtk(struct gnss_nmea_pmtk *pmtk);
#endif

#endif
