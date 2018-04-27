#ifndef _GNSS_TYPES_H_
#define _GNSS_TYPES_H_

#include <stdint.h>
#include "os/mynewt.h"
#include "gnss/q.h"

struct gnss;

struct gnss_event {
    struct os_event os_event;
    uint8_t type;
};

typedef struct gnss_event gnss_event_t;
typedef struct gnss gnss_t;


#if   MYNEWT_VAL(GNSS_USE_FLOAT) == 2
typedef __fp16 gnss_float_t;
#define GNSS_FLOAT_0 0.0f
#define GNSS_SYSFLOAT(x) (x)
#elif MYNEWT_VAL(GNSS_USE_FLOAT) == 1
typedef float  gnss_float_t;
#define GNSS_FLOAT_0 0.0f
#define GNSS_SYSFLOAT(x) (x)
#elif MYNEWT_VAL(GNSS_USE_FLOAT) == 0
typedef gnss_q_t  gnss_float_t;
#define GNSS_FLOAT_0 0
#define GNSS_SYSFLOAT(x) gnss_q_to_f(x)
#else
#error "Unknown float definition for GNSS_USE_FLOAT"
#endif

typedef struct gnss_date {
    uint32_t year         : 16;
    uint32_t month        :  4;
    uint32_t day          :  5;
    uint32_t present      :  1;
} gnss_date_t;

typedef struct gnss_time {
    uint32_t hours        :  5;
    uint32_t minutes      :  6; 
    uint32_t seconds      :  6;    /**< possible leap second included */
    uint32_t microseconds : 10; 
    uint32_t present      :  1;
} gnss_time_t;

typedef struct gnss_sat_info {
    uint32_t prn          :  8;	    /**< Satellit PRN number             */
    uint32_t elevation    :  7;	    /**< Eleveation (degrees) (0-90)     */
    uint32_t azimuth      :  9;	    /**< Azimuth (True North degrees) (0-359) */
    uint32_t snr          :  7;     /**< SNR (dB) (0-99)                 */
} gnss_sat_info_t;


#endif
