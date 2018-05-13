#ifndef _GNSS_UBX_H_
#define _GNSS_UBX_H_

#include <stdint.h>
#include <stdbool.h>
#include <os/mynewt.h>
#include <gnss/types.h>

/**
 * UBX parser
 *
 * @see https://www.u-blox.com/sites/default/files/products/documents/u-blox8-M8_ReceiverDescrProtSpec_%28UBX-13003221%29_Public.pdf
 */

/* AssistNow Aiding Messages */
#define GNSS_UBX_MSG_AID_ALM            0x0B30
#define GNSS_UBX_MSG_AID_AOP            0x0B33
#define GNSS_UBX_MSG_AID_HUI            0x0B02
#define GNSS_UBX_MSG_AID_INI            0x0B01
#define GNSS_UBX_MSG_AID_EPH            0x0B31

/* Ack/Nak Messages */
#define GNSS_UBX_MSG_ACK_ACK            0x0501
#define GNSS_UBX_MSG_ACK_NAK            0x0500

/* Configuration Input Messages */
#define GNSS_UBX_MSG_CFG_ANT            0x0613
#define GNSS_UBX_MSG_CFG_BATCH          0x0693
#define GNSS_UBX_MSG_CFG_CFG            0x0609
#define GNSS_UBX_MSG_CFG_DAT            0x0606
#define GNSS_UBX_MSG_CFG_DGNSS          0x0670
#define GNSS_UBX_MSG_CFG_DOSC           0x0661
#define GNSS_UBX_MSG_CFG_DYNSEED        0x0685
#define GNSS_UBX_MSG_CFG_ESRC           0x0660
#define GNSS_UBX_MSG_CFG_FIXSEED        0x0684
#define GNSS_UBX_MSG_CFG_GEOFENCE       0x0669
#define GNSS_UBX_MSG_CFG_GNSS           0x063E
#define GNSS_UBX_MSG_CFG_HNR            0x065C
#define GNSS_UBX_MSG_CFG_INF            0x0602
#define GNSS_UBX_MSG_CFG_ITFM           0x0639
#define GNSS_UBX_MSG_CFG_LOGFILTER      0x0647
#define GNSS_UBX_MSG_CFG_MSG            0x0601
#define GNSS_UBX_MSG_CFG_NAV5           0x0624
#define GNSS_UBX_MSG_CFG_NAVX5          0x0623
#define GNSS_UBX_MSG_CFG_NMEA           0x0617
#define GNSS_UBX_MSG_CFG_ODO            0x061E
#define GNSS_UBX_MSG_CFG_PM2            0x063B
#define GNSS_UBX_MSG_CFG_PMS            0x0686
#define GNSS_UBX_MSG_CFG_PRT            0x0600
#define GNSS_UBX_MSG_CFG_PWR            0x0657
#define GNSS_UBX_MSG_CFG_RATE           0x0608
#define GNSS_UBX_MSG_CFG_RINV           0x0634
#define GNSS_UBX_MSG_CFG_RST            0x0604
#define GNSS_UBX_MSG_CFG_RXM            0x0611
#define GNSS_UBX_MSG_CFG_SBAS           0x0616
#define GNSS_UBX_MSG_CFG_SMGR           0x0662
#define GNSS_UBX_MSG_CFG_TMODE2         0x063D
#define GNSS_UBX_MSG_CFG_TMODE3         0x0671
#define GNSS_UBX_MSG_CFG_TP5            0x0631
#define GNSS_UBX_MSG_CFG_TXSLOT         0x0653
#define GNSS_UBX_MSG_CFG_USB            0x061B

/* External Sensore Fusion Messages */
#define GNSS_UBX_MSG_ESF_INS            0x1015
#define GNSS_UBX_MSG_ESF_MEAS           0x1002
#define GNSS_UBX_MSG_ESF_RAW            0x1003
#define GNSS_UBX_MSG_ESF_STATUS         0x1010

/* High Rate Navigation Results Messages */
#define GNSS_UBX_MSG_HNR_PVT            0x2800

/* Information Messages */
#define GNSS_UBX_MSG_INF_DEBUG          0x0404
#define GNSS_UBX_MSG_INF_ERROR          0x0400
#define GNSS_UBX_MSG_INF_NOTICE         0x0402
#define GNSS_UBX_MSG_INF_TEST           0x0403
#define GNSS_UBX_MSG_INF_WARNING        0x0401

/* Logging Messages */
#define GNSS_UBX_MSG_LOG_BATCH          0x2111
#define GNSS_UBX_MSG_LOG_CREATE         0x2107
#define GNSS_UBX_MSG_LOG_ERASE          0x2103
#define GNSS_UBX_MSG_LOG_FINDTIME       0x210E
#define GNSS_UBX_MSG_LOG_INFO           0x2108
#define GNSS_UBX_MSG_LOG_RETRIEVEBATCH  0x2110
#define GNSS_UBX_MSG_LOG_RETRIEVEPOSE   0x210f
#define GNSS_UBX_MSG_LOG_RETRIEVEPOS    0x210B
#define GNSS_UBX_MSG_LOG_RETRIEVESTRING 0x210d
#define GNSS_UBX_MSG_LOG_RETRIEVE       0x2109
#define GNSS_UBX_MSG_LOG_STRING         0x2104

/* Multiple GNSS Assistance Messages */
#define GNSS_UBX_MSG_MGA_ACK_DATA0      0x1360
#define GNSS_UBX_MSG_MGA_ANO            0x1320
#define GNSS_UBX_MSG_MGA_BDS_EPH        0x1303
#define GNSS_UBX_MSG_MGA_BDS_ALM        0x1303
#define GNSS_UBX_MSG_MGA_BDS_HEALTH     0x1303
#define GNSS_UBX_MSG_MGA_BDS_UTC        0x1303
#define GNSS_UBX_MSG_MGA_BDS_IONO       0x1303
#define GNSS_UBX_MSG_MGA_DBD            0x1380
#define GNSS_UBX_MSG_MGA_FLASH_DATA     0x1321
#define GNSS_UBX_MSG_MGA_FLASH_STOP     0x1321
#define GNSS_UBX_MSG_MGA_FLASH_ACK      0x1321
#define GNSS_UBX_MSG_MGA_GAL_EPH        0x1302
#define GNSS_UBX_MSG_MGA_GAL_ALM        0x1302
#define GNSS_UBX_MSG_MGA_GAL_TIMEOFF    0x1302
#define GNSS_UBX_MSG_MGA_GAL_UTC        0x1302
#define GNSS_UBX_MSG_MGA_GLO_EPH        0x1306
#define GNSS_UBX_MSG_MGA_GLO_ALM        0x1306
#define GNSS_UBX_MSG_MGA_GLO_TIMEOFF    0x1306
#define GNSS_UBX_MSG_MGA_GPS_EPH        0x1300
#define GNSS_UBX_MSG_MGA_GPS_ALM        0x1300
#define GNSS_UBX_MSG_MGA_GPS_HEALTH     0x1300
#define GNSS_UBX_MSG_MGA_GPS_UTC        0x1300
#define GNSS_UBX_MSG_MGA_GPS_IONO       0x1300
#define GNSS_UBX_MSG_MGA_INI_POS_XYZ    0x1340
#define GNSS_UBX_MSG_MGA_INI_POS_LLH    0x1340
#define GNSS_UBX_MSG_MGA_INI_TIME_UTC   0x1340
#define GNSS_UBX_MSG_MGA_INI_TIME_GNSS  0x1340
#define GNSS_UBX_MSG_MGA_INI_CLKD       0x1340
#define GNSS_UBX_MSG_MGA_INI_FREQ       0x1340
#define GNSS_UBX_MSG_MGA_INI_EOP        0x1340
#define GNSS_UBX_MSG_MGA_QZSS_EPH       0x1305
#define GNSS_UBX_MSG_MGA_QZSS_ALM       0x1305
#define GNSS_UBX_MSG_MGA_QZSS_HEALTH    0x1305

/* Monitoring Messages */
#define GNSS_UBX_MSG_MON_BATCH          0x0A32
#define GNSS_UBX_MSG_MON_GNSS           0x0A28
#define GNSS_UBX_MSG_MON_HW2            0x0A0B
#define GNSS_UBX_MSG_MON_HW             0x0A09
#define GNSS_UBX_MSG_MON_IO             0x0A02
#define GNSS_UBX_MSG_MON_MSGPP          0x0A06
#define GNSS_UBX_MSG_MON_PATCH          0x0A27
#define GNSS_UBX_MSG_MON_RXBUF          0x0A07
#define GNSS_UBX_MSG_MON_RXR            0x0A21
#define GNSS_UBX_MSG_MON_SMGR           0x0A2E
#define GNSS_UBX_MSG_MON_TXBUF          0x0A08
#define GNSS_UBX_MSG_MON_VER            0x0A04

/* Navigation Results Messages */
#define GNSS_UBX_MSG_NAV_AOPSTATUS      0x0160
#define GNSS_UBX_MSG_NAV_ATT            0x0105
#define GNSS_UBX_MSG_NAV_CLOCK          0x0122
#define GNSS_UBX_MSG_NAV_DGPS           0x0131
#define GNSS_UBX_MSG_NAV_DOP            0x0104
#define GNSS_UBX_MSG_NAV_EOE            0x0161
#define GNSS_UBX_MSG_NAV_GEOFENCE       0x0139
#define GNSS_UBX_MSG_NAV_HPPOSECEF      0x0113
#define GNSS_UBX_MSG_NAV_HPPOSLLH       0x0114
#define GNSS_UBX_MSG_NAV_ODO            0x0109
#define GNSS_UBX_MSG_NAV_ORB            0x0134
#define GNSS_UBX_MSG_NAV_POSECEF        0x0101
#define GNSS_UBX_MSG_NAV_POSLLH         0x0102
#define GNSS_UBX_MSG_NAV_PVT            0x0107
#define GNSS_UBX_MSG_NAV_RELPOSNED      0x013C
#define GNSS_UBX_MSG_NAV_RESETODO       0x0110
#define GNSS_UBX_MSG_NAV_SAT            0x0135
#define GNSS_UBX_MSG_NAV_SBAS           0x0132
#define GNSS_UBX_MSG_NAV_SOL            0x0106
#define GNSS_UBX_MSG_NAV_STATUS         0x0103
#define GNSS_UBX_MSG_NAV_SVINFO         0x0130
#define GNSS_UBX_MSG_NAV_SVIN           0x013B
#define GNSS_UBX_MSG_NAV_TIMEBDS        0x0124
#define GNSS_UBX_MSG_NAV_TIMEGAL        0x0125
#define GNSS_UBX_MSG_NAV_TIMEGLO        0x0123
#define GNSS_UBX_MSG_NAV_TIMEGPS        0x0120
#define GNSS_UBX_MSG_NAV_TIMELS         0x0126
#define GNSS_UBX_MSG_NAV_TIMEUTC        0x0121
#define GNSS_UBX_MSG_NAV_VELECEF        0x0111
#define GNSS_UBX_MSG_NAV_VELNED         0x0112

/* Receiver %anager Messages */
#define GNSS_UBX_MSG_RXM_IMES           0x0261
#define GNSS_UBX_MSG_RXM_PMREQ          0x0241
#define GNSS_UBX_MSG_RXM_RAWX           0x0215
#define GNSS_UBX_MSG_RXM_RLM            0x0259
#define GNSS_UBX_MSG_RXM_RTCM           0x0232
#define GNSS_UBX_MSG_RXM_SFRBX          0x0213
#define GNSS_UBX_MSG_RXM_SVSI           0x0220

/* Security Feature Messages */
#define GNSS_UBX_MSG_SEC_SIGN           0x2701
#define GNSS_UBX_MSG_SEC_UNIQID         0x2703

/* Timing Messages */
#define GNSS_UBX_MSG_TIM_DOSC           0x0D11
#define GNSS_UBX_MSG_TIM_FCHG           0x0D16
#define GNSS_UBX_MSG_TIM_HOC            0x0D17
#define GNSS_UBX_MSG_TIM_SMEAS          0x0D13
#define GNSS_UBX_MSG_TIM_SVIN           0x0D04
#define GNSS_UBX_MSG_TIM_TM2            0x0D03
#define GNSS_UBX_MSG_TIM_TOS            0x0D12
#define GNSS_UBX_MSG_TIM_TP             0x0D01
#define GNSS_UBX_MSG_TIM_VCOCAL         0x0D15
#define GNSS_UBX_MSG_TIM_VRFY           0x0D06

/* Firmware Update Messages */
#define GNSS_UBX_MSG_UPD_SOS            0x0914

/* GNSS Identifier */
#define GNSS_UBX_GPS			0
#define GNSS_UBX_SBAS			1
#define GNSS_UBX_GALILEO		2
#define GNSS_UBX_BEIDOU			3
#define GNSS_UBX_IMES			4
#define GNSS_UBX_QZSS			5
#define GNSS_UBX_GLONASS		6

/**
 * UBX message
 */
typedef struct gnss_ubx_message {
    uint16_t class_id;
    uint16_t len;
    uint8_t data[160];
} gnss_ubx_message_t;

/**
 * UBX event
 */
typedef struct gnss_ubx_event {
    struct gnss_event event; /* Need to be first */
    struct gnss_ubx_message ubx;
} gnss_ubx_event_t;

/**
 * Parser context for UBX.
 *  (no serviceable part inside)
 */
struct gnss_ubx {
    uint16_t len;
    uint16_t crc;
    uint16_t idx;
    uint8_t  class;
    uint8_t  id;
    uint8_t  crc_a;
    uint8_t  crc_b;
    gnss_ubx_message_t *msg;
};

/**
 * Initialize the protocol layer for parsing UBX sentence.
 *
 * @param ctx           GNSS context 
 * @param ubx           Parser context
 *
 * @return true on success
 */
#if MYNEWT_VAL(GNSS_USE_UBX_PROTOCOL) > 0
bool gnss_ubx_init(gnss_t *ctx, struct gnss_ubx *ubx);
#endif

/**
 * Send a command using UBX protocol.
 * Header and checksum are automatically added.
 *
 * @param ctx           GNSS context
 * @param msg           Message type (Class + Id)
 * @param bytes         Data to be send
 * @param size          Data size
 *
 * @retval < 0 in case of error
 * @retval Number of bytes sent to the wire
 */
int gnss_ubx_send_cmd(gnss_t *ctx, uint16_t msg, uint8_t *bytes, uint16_t size);

/* Byte decoder for UBX protocol */

void gnss_ubx_byte_decoder_reset(struct gnss_ubx *gu);
int gnss_ubx_byte_decoder(gnss_t *ctx, struct gnss_ubx *gu, uint8_t byte);

void gnss_ubx_log(struct gnss_ubx_message *ubx);

#endif
