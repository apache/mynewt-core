#ifndef _GNSS_H_
#define _GNSS_H_

#include <stdint.h>
#include <gnss/types.h>


#define GNSS_BYTE_DECODER_UNHANDLED     3  /* Decoded, but unprocessed     */
#define GNSS_BYTE_DECODER_DECODED       2  /* Fully decoded a blob         */
#define GNSS_BYTE_DECODER_DECODING      1  /* In the decoding process      */
#define GNSS_BYTE_DECODER_SYNCING       0  /* Looking for start marker     */
#define GNSS_BYTE_DECODER_FAILED       -1  /* Failed parsing               */
#define GNSS_BYTE_DECODER_ERROR        -2  /* Parsing error (syntax only)  */
#define GNSS_BYTE_DECODER_ABORTED      -3  /* Decoder gave up              */


#define GNSS_ERROR_NONE			0
#define GNSS_ERROR_SCRAMBLED_TRANSPORT	1


#define GNSS_EVENT_UNKNOWN		0
#define GNSS_EVENT_NMEA			1
#define GNSS_EVENT_UBX                  2


#define GNSS_STANDBY_NONE      		0 /* Reserved */
#define GNSS_STANDBY_LIGHT     		1
#define GNSS_STANDBY_DEEP      		2
#define GNSS_STANDBY_FULL      		3


#define GNSS_RESET_NONE         	0 /* Reserved */
#define GNSS_RESET_HOT          	1
#define GNSS_RESET_WARM         	2
#define GNSS_RESET_COLD         	3
#define GNSS_RESET_FULL         	4
#define GNSS_RESET_HARD         	5


#define GNSS_MS_TO_TICKS(ms) (((ms) * OS_TICKS_PER_SEC + 999) / 1000)


typedef void (*gnss_callback_t)(int type, void *data);
typedef void (*gnss_error_callback_t)(gnss_t *ctx, int error);

typedef void (*gnss_data_ready_callback_t)(void *arg);

typedef int  (*gnss_speed_t)(gnss_t *ctx, uint32_t speed);
typedef int  (*gnss_start_rx_t)(gnss_t *ctx);
typedef int  (*gnss_stop_rx_t)(gnss_t *ctx);
typedef int  (*gnss_send_t)(gnss_t *ctx, uint8_t *bytes, uint16_t size);

typedef int  (*gnss_decoder_t)(gnss_t *ctx, uint8_t byte);

typedef int  (*gnss_standby_t)(gnss_t *ctx, int level);
typedef int  (*gnss_wakeup_t)(gnss_t *ctx);
typedef int  (*gnss_reset_t)(gnss_t *ctx, int type);

typedef int  (*gnss_on_data_ready_t)(gnss_t *ctx, gnss_data_ready_callback_t cb);
typedef int  (*gnss_is_data_ready_t)(gnss_t *ctx);




struct gnss_error_event {
    struct os_event os_event; /* Need to be first */
};


struct gnss {
    struct {
	void                *conf;
	gnss_speed_t         speed;
	gnss_start_rx_t      start_rx;
	gnss_stop_rx_t       stop_rx;
#if MYNEWT_VAL(GNSS_RX_ONLY) == 0
	gnss_send_t          send;
#endif
    } transport;

    struct {
	void                *conf;
	gnss_standby_t       standby;
	gnss_wakeup_t        wakeup;
	gnss_reset_t         reset;
	gnss_on_data_ready_t on_data_ready;
	gnss_is_data_ready_t is_data_ready;
    } driver;

    struct {
	void                *conf;
	gnss_decoder_t       decoder;
    } protocol;

    struct {
	uint16_t             error;
	uint16_t             syncing;
    } decoder;
    
    int                      error;
    struct gnss_error_event  error_event;
    gnss_error_callback_t    error_callback;

    gnss_callback_t          callback;

    struct gnss_event       *event;
};


gnss_event_t *gnss_prepare_event(gnss_t *ctx, uint8_t type);
void gnss_emit_event(gnss_t *ctx);


void gnss_emit_error_event(gnss_t *ctx, unsigned int error);

void gnss_pkg_init(void);


#if MYNEWT_VAL(GNNS_CHECK_SCRAMBLED_TRANSPORT > 0)
int gnss_check_scrambled_transport(gnss_t *ctx, int code);
#else
static inline int
gnss_check_scrambled_transport(gnss_t *ctx, int code) {
    return code;
}
#endif


/* Knot to m/s */
static inline 
gnss_float_t _gnss_nmea_knot_to_mps(gnss_float_t val)
{
#if MYNEWT_VAL(GNSS_USE_FLOAT) > 0
    return val * 0.514444f; 
#else
    return gnss_q_div(val, 16857);  // XXX: macro for conversion?
#endif
}

/* km/h to m/s */
static inline
gnss_float_t _gnss_nmea_kmph_to_mps(gnss_float_t val)
{
#if MYNEWT_VAL(GNSS_USE_FLOAT) > 0
    return val * 0.277778f;
#else
    return gnss_q_div(val, 9102); // XXX: macro for conversion?
#endif
}




/*-- Public API --------------------------------------------------------*/

/**
 * Specify the event queue to use for generating callback.
 * (if not sepecified, the default OS event queue is used)
 *
 * @note Calling this function after initialization result 
 *       in an undefined behaviour
 *
 * @param evq		Event queue
 */
void gnss_eventq_set(struct os_eventq *evq);

/**
 * Specify the event queue to use for processing of interrupt/polling
 * (if not sepecified, the default OS event queue is used)
 *
 * @note Calling this function after initialization result 
 *       in an undefined behaviour
 *
 * @param evq		Event queue
 */
void gnss_internal_eventq_set(struct os_eventq *evq);

/**
 * Initialise GNSS 
 *
 * Further initialisation will be required for:
 *   o transport : gnss_uart_init, gnss_i2c_init
 *   o protocol  : gnss_nmea_init, gnss_ubx_init
 *   o driver    : gnss_mediatek_init, gnss_ublox_init
 * 
 * @param ctx		GNSS context
 * @param callback	Called when a GNSS message is received
 * @param error_callback  Called when an error has been encoutered
 */
void gnss_init(gnss_t *ctx,
	       gnss_callback_t callback,
	       gnss_error_callback_t error_callback);

/**
 * Put device in standby (aka power saving).
 *
 * Positioning system requires various information to compute
 * precise positioning. Entering standby mode can discard some of
 * these values which will need time to acquire again, for example:
 * time, almanach, and ephemeris.
 *
 * @param ctx		GNSS context
 * @param level		Hint to the driver to select an appropriate 
 *                      standby mode
 *                        o GNSS_STANDBY_LIGHT 
 *                        o GNSS_STANDBY_DEEP
 *                        o GNSS_STANDBY_FULL
 *
 * @return < 0 if the operation is unsupported or unsuccessful
 */
static inline int
gnss_standby(gnss_t *ctx, int level)
{
    int rc;
    
    if (ctx->driver.standby == NULL) {
	return -1;
    }

    rc = ctx->driver.standby(ctx, level);
    if (rc < 0) {
	return rc;
    }
    return ctx->transport.stop_rx(ctx);
}

/**
 * Wakeup from power saving
 *
 * @param ctx		GNSS context
 *
 * @return < 0 if the operation is unsupported or unsuccessful
 */
static inline int
gnss_wakeup(gnss_t *ctx)
{
    int rc;
    
    if (ctx->driver.wakeup == NULL) {
	return -1;
    }

    rc = ctx->transport.start_rx(ctx);
    if (rc < 0) {
	return rc;
    }
    return ctx->driver.wakeup(ctx);
}

/**
 * Reset the device
 *
 * @param ctx		GNSS context
 * @param type		Type of reset (it's only a hint)
 *                        o GNSS_RESET_HOT
 *                        o GNSS_RESET_WARM
 *                        o GNSS_RESET_COLD
 *                        o GNSS_RESET_FULL
 *                        o GNSS_RESET_HARD
 *
 * @return < 0 if the operation is unsupported or unsuccessful
 */
static inline int
gnss_reset(gnss_t *ctx, int type)
{
    if (ctx->driver.reset == NULL) {
	return -1;
    }
    return ctx->driver.reset(ctx, type);
}

/**
 * Reset the device
 *
 * @param ctx		GNSS context
 * @param speed		Speed to use to communicate with the device
 *
 * @return < 0 if the operation is unsupported or unsuccessful
 */
static inline int
gnss_speed(gnss_t *ctx, uint32_t speed)
{
    if (ctx->transport.speed == NULL) {
	return -1;
    }
    return ctx->transport.speed(ctx, speed);
}

/**
 * Start receiving GNSS information
 *
 * @param ctx		GNSS context
 * @parem decoder	Decoder used for parsing data
 *
 * @return < 0 if the operation is unsuccessful
 */
static inline int
gnss_start_rx(gnss_t *ctx)
{
    assert(ctx->transport.start_rx != NULL);
    return ctx->transport.start_rx(ctx);
}

/**
 * Stop receiving GNSS information
 *
 * @param ctx		GNSS context
 *
 * @return < 0 if the operation is unsuccessful
 */
static inline int
gnss_stop_rx(gnss_t *ctx)
{
    assert(ctx->transport.stop_rx != NULL);
    return ctx->transport.stop_rx(ctx);
}

/**
 * Send information to the GNSS module.
 *  (Whatever it means for the instanciated driver)
 *
 * @param ctx		GNSS context
 * @param bytes		Data to send
 * @param size		Size of the data to send
 *
 * @return < 0 if the operation is unsupported or unsuccessful
 */
static inline int
gnss_send(gnss_t *ctx, uint8_t *bytes, uint16_t size)
{
#if MYNEWT_VAL(GNSS_RX_ONLY) == 0
    if (ctx->transport.send == NULL) {
	return -1;
    }
    return ctx->transport.send(ctx, bytes, size);
#else
    return -1;
#endif
}

#endif
