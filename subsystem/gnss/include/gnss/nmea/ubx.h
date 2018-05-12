#ifndef _GNSS_NMEA_UBX_H_
#define _GNSS_NMEA_UBX_H_

/**
 * Unknown message (aka: parser need to be improved)
 */
#define GNSS_NMEA_PUBX_TYPE_UNKNOWN                     0xFFFF
/**
 * Config
 */
#define GNSS_NMEA_PUBX_TYPE_CONFIG                      41
/**
 * Position
 */
#define GNSS_NMEA_PUBX_TYPE_POSITION                    00
/**
 * Rate
 */
#define GNSS_NMEA_PUBX_TYPE_RATE                        40
/**
 * Satellite Status
 */
#define GNSS_NMEA_PUBX_TYPE_SVSTATUS                    03
/**
 * Time of day and Clock information
 */
#define GNSS_NMEA_PUBX_TYPE_TIME                        04

/* Flags for protocol mask in PUBX Protocol
 */
#define GNSS_NMEA_PUBX_CONFIG_PROTOCOL_UBLOX        0x0001
#define GNSS_NMEA_PUBX_CONFIG_PROTOCOL_NMEA         0x0002
#define GNSS_NMEA_PUBX_CONFIG_PROTOCOL_RTCM         0X0004
#define GNSS_NMEA_PUBX_CONFIG_PROTOCOL_RTCM3        0X0020

/* Name of port in PUBX Config
 */
#define GNSS_NMEA_PUBX_CONFIG_PORT_DDC                   0
#define GNSS_NMEA_PUBX_CONFIG_PORT_UART1                 1
#define GNSS_NMEA_PUBX_CONFIG_PORT_USB                   3
#define GNSS_NMEA_PUBX_CONFIG_PORT_SPI                   4

/* Status for PUBX Position
 */
#define GNSS_NMEA_PUBX_POSITION_STATUS_NO_FIX                     0
#define GNSS_NMEA_PUBX_POSITION_STATUS_DEAD_RECKONING             1
#define GNSS_NMEA_PUBX_POSITION_STATUS_STANDALONE_2D              2
#define GNSS_NMEA_PUBX_POSITION_STATUS_STANDALONE_3D              3
#define GNSS_NMEA_PUBX_POSITION_STATUS_DIFFERENTIAL_2D            4
#define GNSS_NMEA_PUBX_POSITION_STATUS_DIFFERENTIAL_3D            5
#define GNSS_NMEA_PUBX_POSITION_STATUS_GPS_AND_DEAD_RECKONING     6
#define GNSS_NMEA_PUBX_POSITION_STATUS_TIME_ONLY                  7

/**
 * Ublox, PUBX
 */
struct gnss_nmea_pubx {
    uint16_t type;
    union {
        struct {
            uint8_t      port_id;         /**< Port Id                       */
            uint8_t      autobauding;     /**< Auto bauding                  */
            uint16_t     in_proto;        /**< Input protocol mask           */
            uint16_t     out_proto;       /**< Output protocol mask          */
            uint32_t     baudrate;        /**< Baud rate                     */
        } config;
        struct {
            gnss_time_t  time;            /**< UTC time                      */
            gnss_float_t latitude;        /**< Latitude  (decimal degrees)   */
            gnss_float_t longitude;       /**< Longitude (decimal degrees)   */
            gnss_float_t altitude;        /**< Altitude (meters)             */
            gnss_float_t speed;           /**< Speed over ground (m/s)       */
            gnss_float_t track;           /**< Track over ground (degrees)   */
            gnss_float_t velocity;        /**< Vertical velocity (m/s)       */
            gnss_float_t hdop;            /**< Horizontal DOP                */
            gnss_float_t vdop;            /**< Vertical DOP                  */
            gnss_float_t tdop;            /**< Time DOP                      */
            gnss_float_t hacc;            /**< Horizontal accuracy (meters)  */
            gnss_float_t vacc;            /**< Vertical accuracy (meters)    */
            uint16_t     dgps_age;        /**< Age of DGPS (seconds)         */
            uint8_t      gps_used;        /**< Number of satellites used     */
            uint8_t      glonass_used;    /**< Number of satellites used     */
            uint8_t      status:4;        /**< Status                        */
            uint8_t      dead_reckoning:1;/**< Dead reckoning used           */
        } position;
        struct {
            uint8_t      ddc;             /**< Rate on DDC                   */
            uint8_t      usart1;          /**< Rate on USART 1               */
            uint8_t      usart2;          /**< Rate on USART 2               */
            uint8_t      usb;             /**< Rate on USB                   */
            uint8_t      spi;             /**< Rate on SPI                   */
        } rate;
        struct {
            gnss_time_t time;             /**< UTC time                      */
            gnss_date_t date;             /**< UTC date                      */
            /* Other fields Not Implemented Yet */
        } time;
    };
};

#if MYNEWT_VAL(GNSS_NMEA_USE_PUBX) > 0
int gnss_nmea_decoder_pubx(struct gnss_nmea_pubx *pubx, char *field, int fid);
#endif

#if MYNEWT_VAL(GNSS_NMEA_USE_PUBX) > 0
void gnss_nmea_log_pubx(struct gnss_nmea_pubx *pubx);
#endif

#endif
