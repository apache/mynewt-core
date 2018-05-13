#ifndef _GNSS_NMEA_H_
#define _GNSS_NMEA_H_

#include <stdint.h>
#include <stdbool.h>
#include <os/os.h>
#include <syscfg/syscfg.h>

#include <gnss/types.h>
#include <gnss/nmea/mediatek.h>
#include <gnss/nmea/ubx.h>

/**
 * NMEA 0183 parser
 *
 * @note The parser has some limitation as it consider the following
 *       character/delimiter as invalid:
 *        - Start of encapsulation sentence delimiter (!)
 *        - Tag block delimiter (\)
 *        - Code delimiter for HEX representation (^)
 *        - Reserved (~)
 *
 * @see http://www.catb.org/gpsd/NMEA.html
 */


struct gnss_nmea;
    
/**
 * Initialize the protocol layer for parsing NMEA sentence.
 *
 * @param ctx           GNSS context 
 * @param uart          Configuration
 *
 * @return true on success
 */
#if MYNEWT_VAL(GNSS_USE_NMEA) > 0
bool gnss_nmea_init(gnss_t *ctx, struct gnss_nmea *nmea);
#endif

/**
 * Compute CRC used to validate NMEA sentence.
 *
 * @param str           string
 *
 * @return crc
 */
static inline uint8_t
gnss_nmea_crc(char *str) {
    uint8_t crc = 0;
    for ( ; *str ; str++) {
        crc ^= *str;
    }
    return crc;
}

/**
 * NMEA max sentence size (including '$', <CR>, <LF>)
 */
#define GNSS_NMEA_SENTENCE_MAXBYTES                    82

/**
 * Fix type
 */
#define GNNS_NMEA_FIX_TYPE_NOT_AVAILABLE                0
#define GNNS_NMEA_FIX_TYPE_GPS                          1
#define GNNS_NMEA_FIX_TYPE_DIFFERENTIAL_GPS             2
#define GNNS_NMEA_FIX_TYPE_PPS                          3
#define GNNS_NMEA_FIX_TYPE_REAL_TIME_KINEMATIC          4
#define GNNS_NMEA_FIX_TYPE_FLOAT_RTK                    5
#define GNNS_NMEA_FIX_TYPE_DEAD_RECKONING               6
#define GNNS_NMEA_FIX_TYPE_MANUAL_INPUT                 7
#define GNNS_NMEA_FIX_TYPE_SIMULATION                   8

/**
 * FAA modes
 */
#define GNNS_NMEA_FAA_MODE_AUTONOMOUS                 'A'
#define GNNS_NMEA_FAA_MODE_DIFFERENTIAL               'D'
#define GNSS_NMEA_FAA_MODE_DEAD_RECKONING             'E'
#define GNSS_NMEA_FAA_MODE_MANUAL                     'M'
#define GNSS_NMEA_FAA_MODE_SIMULATED                  'S'
#define GNSS_NMEA_FAA_MODE_NOT_VALID                  'N'
#define GNSS_NMEA_FAA_MODE_PRECISE                    'P'

/**
 * Fix mode
 */
#define GNSS_NMEA_FIX_MODE_NO                           1
#define GNSS_NMEA_FIX_MODE_2D                           2
#define GNSS_NMEA_FIX_MODE_3D                           3

/**
 * Talker Id
 *  (using base 36 for code between 00 and ZZ)
 */
#define GNSS_NMEA_TALKER_BD                           409
#define GNSS_NMEA_TALKER_GA                           586
#define GNSS_NMEA_TALKER_GB                           587
#define GNSS_NMEA_TALKER_GL                           597
#define GNSS_NMEA_TALKER_GN                           599
#define GNSS_NMEA_TALKER_GP                           601
#define GNSS_NMEA_TALKER_QZ                           971

#define GNSS_NMEA_TALKER_UBLOX                      60001
#define GNSS_NMEA_TALKER_MTK                        60002

/**
 * Sentence type
 *  (using base 36 for code between 000 and ZZZ)
 */
#define GNSS_NMEA_SENTENCE_GGA                      21322
#define GNSS_NMEA_SENTENCE_GLL                      21513
#define GNSS_NMEA_SENTENCE_GSA                      21754
#define GNSS_NMEA_SENTENCE_GST                      21773
#define GNSS_NMEA_SENTENCE_GSV                      21775
#define GNSS_NMEA_SENTENCE_RMC                      35796
#define GNSS_NMEA_SENTENCE_VTG                      41236
#define GNSS_NMEA_SENTENCE_ZDA                      45838

#define GNSS_NMEA_SENTENCE_PGACK                    60011
#define GNSS_NMEA_SENTENCE_PMTK                     60012
#define GNSS_NMEA_SENTENCE_MCHN                     60013
#define GNSS_NMEA_SENTENCE_PUBX                     60021

#define GNSS_NMEA_SENTENCE_NONE                     65535

/**
 * GGA - Global Positioning System Fix Data
 */
struct gnss_nmea_gga {
    gnss_time_t  time;                  /**< UTC time                      */
    gnss_float_t latitude;              /**< Latitude  (decimal degrees)   */
    gnss_float_t longitude;             /**< Longitude (decimal degrees)   */
    gnss_float_t hdop;                  /**< Horizontal dilution of precision*/
    gnss_float_t altitude;              /**< Altitude (meters)             */
    gnss_float_t geoid_separation;      /**< Geoid sepearation (meters)    */
    uint16_t     dgps_age;              /**< Age of DGPS (seconds)         */
    uint16_t     dgps_sid;              /**< Satelitte Id of DGPS (0-1023) */
    uint8_t      fix_indicator     :4;  /**< Fix quality (0-8)             */
    uint8_t      satellites_in_view:4;  /**< Nb. of satelittes in view (0-12)*/
};

/**
 * GLL - Geographic Position - Latitude/Longitude
 */
struct gnss_nmea_gll {
    gnss_float_t latitude;              /**< Latitude  (decimal degrees)   */
    gnss_float_t longitude;             /**< Longitude (decimal degrees)   */
    gnss_time_t  time;                  /**< UTC Time                      */
    bool         valid;                 /**< Valid                         */
    char         faa_mode;              /**< FAA mode                      */
};

/**
 * GSA - GPS DOP and active satellites
 */
struct gnss_nmea_gsa {
    gnss_float_t pdop;                  /**< PDOP                          */
    gnss_float_t hdop;                  /**< HDOP                          */
    gnss_float_t vdop;                  /**< VDOP                          */
    uint8_t      sid[12];               /**< List of satelittes id         */
    char         fix_mode_selection;    /**< Mode selection                */
    uint8_t      fix_mode;              /**< Fix mode                      */
};

/**
 * GST - GPS Pseudorange Noise Statistics
 */
struct gnss_nmea_gst {
    gnss_time_t  time;                  /**< Time of associated GGA fix    */
    gnss_float_t rms_stddev;            /**< RMS standard deviation        */
    gnss_float_t semi_major_stddev;     /**  Semi-major axis (meters)      */
    gnss_float_t semi_minor_stddev;     /**< Semi-minor axis (meters)      */
    gnss_float_t semi_major_orientation;/**< Orientation (true North degrees)*/
    gnss_float_t latitude_stddev;       /**< Latitutde std. dev. (meters)  */
    gnss_float_t longitude_stddev;      /**< Longitude std. dev. (meters)  */
    gnss_float_t altitude_stddev;       /**< Altitude std. dev. (meters)   */
};

/**
 * GSV - Satellites in view
 */
struct gnss_nmea_gsv {
    gnss_sat_info_t sat_info[4];        /**< Satellite info                */
    uint8_t         msg_count;          /**< Number of messages            */
    uint8_t         msg_idx;            /**< Index of this message         */
    uint8_t         total_sats;         /**< Total number of Sat. in view  */
};

/**
 * RMC - Recommended Minimum Navigation Information
 */
struct gnss_nmea_rmc {
    gnss_date_t  date;                  /**< Date (2-digit year)           */ 
    gnss_time_t  time;                  /**< UTC Time                      */
    gnss_float_t latitude;              /**< Latitude (decimale degrees)   */
    gnss_float_t longitude;             /**< Longitude (decimal degrees)   */
    gnss_float_t speed;                 /**< Speed over ground (m/s)       */
    gnss_float_t track_true;            /**< Track (true degrees)          */
    gnss_float_t declination_magnetic;  /**< Magnetic declination (degrees)*/
    bool         valid;                 /**< Valid fix                     */
    char         faa_mode;              /**< FAA mode                      */
};

/**
 * VTG - Track made good and Ground speed
 */
struct gnss_nmea_vtg {
    gnss_float_t track_true;            /**< True track (true degrees)     */
    gnss_float_t track_magnetic;        /**< Magnetic track (magn. degrees)*/
    gnss_float_t speed;                 /**< Speed (m/s)                   */
    char         faa_mode;              /**< FAA mode                      */
};

/**
 * NMEA message data. (Parsed from text sentence)
 */
union gnss_nmea_data {
    /* Standard sentence
     */
#if MYNEWT_VAL(GNSS_NMEA_USE_GGA) > 0
    struct gnss_nmea_gga gga;           /**< GGA */
#endif
#if MYNEWT_VAL(GNSS_NMEA_USE_GLL) > 0
    struct gnss_nmea_gll gll;           /**< GLL */
#endif
#if MYNEWT_VAL(GNSS_NMEA_USE_GSA) > 0
    struct gnss_nmea_gsa gsa;           /**< GSA */
#endif
#if MYNEWT_VAL(GNSS_NMEA_USE_GST) > 0
    struct gnss_nmea_gst gst;           /**< GST */
#endif
#if MYNEWT_VAL(GNSS_NMEA_USE_GSV) > 0
    struct gnss_nmea_gsv gsv;           /**< GSV */
#endif
#if MYNEWT_VAL(GNSS_NMEA_USE_RMC) > 0
    struct gnss_nmea_rmc rmc;           /**< RMC */
#endif
#if MYNEWT_VAL(GNSS_NMEA_USE_VTG) > 0
    struct gnss_nmea_vtg vtg;           /**< VTG */
#endif
    /* Driver specific
     */
#if MYNEWT_VAL(GNSS_NMEA_USE_PGACK) > 0
    struct gnss_nmea_pgack pgack;       /**< PGACK from MediaTek */
#endif
#if MYNEWT_VAL(GNSS_NMEA_USE_PMTK) > 0
    struct gnss_nmea_pmtk pmtk;         /**< PMTK  from Mediatek */
#endif
#if MYNEWT_VAL(GNSS_NMEA_USE_PUBX) > 0
    struct gnss_nmea_pubx pubx;         /**< PBUX  from Ublox    */
#endif
};

/**
 * NMEA meassage
 */
typedef struct gnss_nmea_message {
    uint16_t talker;                    /**< Talker Id          */
    uint16_t sentence;                  /**< Sentence code      */
    union gnss_nmea_data data;          /**< Data               */
} gnss_nmea_message_t;

/**
 * NMEA event
 */
typedef struct gnss_nmea_event {
    struct gnss_event event; /* Need to be first */
    struct gnss_nmea_message nmea;
} gnss_nmea_event_t;


/**
 */
struct gnss_nmea_rate {
    uint16_t sentence;
    uint16_t rate;
};


/**
 * Send a command using NMEA protocol.
 * Header and checksum are automatically added.
 *
 * @param ctx           GNSS context
 * @param cmd           Command to send
 */
bool gnss_nmea_send_cmd(gnss_t *ctx, char *cmd);


int gnss_nmea_byte_decoder(gnss_t *ctx, struct gnss_nmea *gn, uint8_t byte);

/*
 * @retval -1 for syntax error
 * @retval  0 for semantic error or not handled by parser
 * @retval  1 for success
 */
typedef int (*gnss_nmea_field_decoder_t)(union gnss_nmea_data *, char *field, int fid);



struct gnss_nmea {
    gnss_nmea_field_decoder_t field_decoder;
    uint8_t state;
    uint8_t crc;
    uint8_t fid;
    uint8_t bufcnt;

    gnss_nmea_message_t *msg;
    
    char buffer[MYNEWT_VAL(GNSS_NMEA_FIELD_BUFSIZE)];
};

int gnss_nmea_field_parse_string(const char *str, char *val, uint16_t maxsize);
int gnss_nmea_field_parse_char(const char *str, char *val);
int gnss_nmea_field_parse_long(const char *str, long *val);
int gnss_nmea_field_parse_float(const char *str, gnss_float_t *val);
int gnss_nmea_field_parse_and_apply_direction(const char *str, gnss_float_t *val);
int gnss_nmea_field_parse_latlng(const char *str, gnss_float_t *val);
int gnss_nmea_field_parse_date(const char *str, gnss_date_t *val);
int gnss_nmea_field_parse_time(const char *str, gnss_time_t *val);
int gnss_nmea_field_parse_crc(const char *str, uint8_t *val);

#if MYNEWT_VAL(GNSS_NMEA_USE_GGA) > 0
int gnss_nmea_decoder_gga(struct gnss_nmea_gga *gga, char *field, int fid);
#endif
#if MYNEWT_VAL(GNSS_NMEA_USE_GLL) > 0
int gnss_nmea_decoder_gll(struct gnss_nmea_gll *gll, char *field, int fid);
#endif
#if MYNEWT_VAL(GNSS_NMEA_USE_GSA) > 0
int gnss_nmea_decoder_gsa(struct gnss_nmea_gsa *gsa, char *field, int fid);
#endif
#if MYNEWT_VAL(GNSS_NMEA_USE_GST) > 0
int gnss_nmea_decoder_gst(struct gnss_nmea_gst *gst, char *field, int fid);
#endif
#if MYNEWT_VAL(GNSS_NMEA_USE_GSV) > 0
int gnss_nmea_decoder_gsv(struct gnss_nmea_gsv *gsv, char *field, int fid);
#endif
#if MYNEWT_VAL(GNSS_NMEA_USE_RMC) > 0
int gnss_nmea_decoder_rmc(struct gnss_nmea_rmc *rmc, char *field, int fid);
#endif
#if MYNEWT_VAL(GNSS_NMEA_USE_VTG) > 0
int gnss_nmea_decoder_vtg(struct gnss_nmea_vtg *vtg, char *field, int fid);
#endif

void gnss_nmea_log(struct gnss_nmea_message *nmea);
#if MYNEWT_VAL(GNSS_NMEA_USE_GGA) > 0
void gnss_nmea_log_gga(struct gnss_nmea_gga *gga);
#endif
#if MYNEWT_VAL(GNSS_NMEA_USE_GLL) > 0
void gnss_nmea_log_gll(struct gnss_nmea_gll *gll);
#endif
#if MYNEWT_VAL(GNSS_NMEA_USE_GSA) > 0
void gnss_nmea_log_gsa(struct gnss_nmea_gsa *gsa);
#endif
#if MYNEWT_VAL(GNSS_NMEA_USE_GST) > 0
void gnss_nmea_log_gst(struct gnss_nmea_gst *gst);
#endif
#if MYNEWT_VAL(GNSS_NMEA_USE_GSV) > 0
void gnss_nmea_log_gsv(struct gnss_nmea_gsv *gsv);
#endif
#if MYNEWT_VAL(GNSS_NMEA_USE_RMC) > 0
void gnss_nmea_log_rmc(struct gnss_nmea_rmc *rmc);
#endif
#if MYNEWT_VAL(GNSS_NMEA_USE_VTG) > 0
void gnss_nmea_log_vtg(struct gnss_nmea_vtg *vtg);
#endif

#endif
