#include <stdint.h>
#include <hal/hal_gpio.h>
#include <gnss/gnss.h>
#include <gnss/i2c.h>

// TODO: move static buffer to i2c context

#define GNSS_MEDIATEK_POLLING_BUFFER_SIZE 20

static uint8_t polling_buffer[GNSS_MEDIATEK_POLLING_BUFFER_SIZE];

extern struct os_eventq *_gnss_internal_evq;

#define GNSS_I2C_POLLING_NOT_READY_ERROR	-3
#define GNSS_I2C_POLLING_IO_ERROR		-2
#define GNSS_I2C_POLLING_DECODING_ERROR		-1
#define GNSS_I2C_POLLING_DONE			 0
#define GNSS_I2C_POLLING_NEED_MORE		 1


/* If using data ready pin, transfert interrupt to callout event
 */
static void
gnss_i2c_data_ready_handler(void *arg)
{
    gnss_t          *ctx   = (gnss_t *)arg;
    struct gnss_i2c *i2c   = ctx->transport.conf;
    os_callout_reset(&i2c->polling, 0);
}

/* Used to perform polling, either because no interrupt
 * can be generated (no data ready pin), or because
 * buffer need a delay for being refilled (and we don't
 * want to hog the cpu)
 */
static void
gnss_i2c_polling_handler(struct os_event *ev)
{
    gnss_t          *ctx   = (gnss_t *)ev->ev_arg;
    struct gnss_i2c *i2c   = ctx->transport.conf;
    uint32_t         ticks = OS_TICKS_PER_SEC / 2;
    
    int rc = gnss_i2c_mediatek_process_buffer(ctx);
    switch(rc) {
    case GNSS_I2C_POLLING_DECODING_ERROR:
	return;
	
    case GNSS_I2C_POLLING_NOT_READY_ERROR:
    case GNSS_I2C_POLLING_IO_ERROR:
    case GNSS_I2C_POLLING_DONE:
	break;
	
    case GNSS_I2C_POLLING_NEED_MORE:
	ticks = GNSS_MS_TO_TICKS(i2c->refill_delay);
	break;
    }

    os_callout_reset(&i2c->polling, ticks);
}


static int
gnss_i2c_send(gnss_t *ctx, uint8_t *bytes, uint16_t size)
{
#if MYNEWT_VAL(GNSS_RX_ONLY) == 0
    int rc;
    struct gnss_i2c *i2c;
    struct hal_i2c_master_data i2c_data;

    if ((bytes == NULL) || (size == 0)) {
	return 0;
    }
    
    i2c      = ctx->transport.conf;
    i2c_data = (struct hal_i2c_master_data) {
        .address = i2c->addr,
        .len     = size,
        .buffer  = bytes,
    };

    rc = hal_i2c_master_write(i2c->dev, &i2c_data, OS_TIMEOUT_NEVER, 1);

    return rc == 0 ? size : -1;
#else
    return -1;
#endif
}


int
gnss_i2c_ddc_process_buffer(gnss_t *ctx)
{
    assert(0); /* To be implemented */
    return 0;
}

int
gnss_i2c_mediatek_process_buffer(gnss_t *ctx)
{
    uint8_t i, a = 0, c = 0, lf = 0;
    struct gnss_i2c *i2c;
    struct hal_i2c_master_data i2c_data;

    i2c      = ctx->transport.conf;
    i2c_data = (struct hal_i2c_master_data) {
        .address = i2c->addr,
        .len     = GNSS_MEDIATEK_POLLING_BUFFER_SIZE,
        .buffer  = polling_buffer,
    };

 refill_buffer:
    /* Read data */
    if (hal_i2c_master_read(i2c->dev, &i2c_data, OS_TIMEOUT_NEVER, 1) != 0) {
	return GNSS_I2C_POLLING_IO_ERROR;
    }
    
    /* Check for unexpected value, not fully booted? */
    if (polling_buffer[0] == '\0') {
	return GNSS_I2C_POLLING_NOT_READY_ERROR;
    }
    
    /* Perform decoding
     */
    for (i = 0 ; i < GNSS_MEDIATEK_POLLING_BUFFER_SIZE ; i++, c++) {
	uint8_t b = polling_buffer[i];
	
	/* Retrieved buffer use <LF> as filling, or to indicate that
	 * a new buffer need to be fetched */
	if (b == '\n') {
	    lf++;

	    /* Two consecutive <LF> */ 
	    if (lf >= 2) {
		/* All the data has already been read? */
		if (a > 0 || lf >= 255) {
		    return GNSS_I2C_POLLING_DONE;
		}

	    /* Last char of buffer */
	    } else if (c == 254) {
		return GNSS_I2C_POLLING_NEED_MORE;
	    }
	    continue;
	} else {
	    lf = 0;
	    a  = 1;
	}
	
	/* Process byte 
	 * As we are discarding <LF>, we will need to explicitely
	 * generate one when a <CR> is matched
	 */
	assert(ctx->protocol.decoder != NULL);
	if (ctx->protocol.decoder(ctx, b) < 0) {
	    return GNSS_I2C_POLLING_DECODING_ERROR;
	}	      
	if (b == '\r') {
	    if (ctx->protocol.decoder(ctx, '\n') < 0) {
		return GNSS_I2C_POLLING_DECODING_ERROR;
	    }
	}
    }

    if (ctx->driver.is_data_ready) {
	if (ctx->driver.is_data_ready(ctx) == 0) {
	    return GNSS_I2C_POLLING_DONE;
	}
    };

    goto refill_buffer;
}

static int
gnss_i2c_start_rx(gnss_t *ctx)
{
    struct gnss_i2c *i2c = ctx->transport.conf;
    
    if (ctx->driver.on_data_ready) {
	return ctx->driver.on_data_ready(ctx, gnss_i2c_data_ready_handler);
    } else {
	return os_callout_reset(&i2c->polling, OS_TICKS_PER_SEC / 2);
    }
}

static int
gnss_i2c_stop_rx(gnss_t *ctx)
{
    struct gnss_i2c *i2c = ctx->transport.conf;

    if (ctx->driver.on_data_ready) {
	ctx->driver.on_data_ready(ctx, NULL);
    }
    os_callout_stop(&i2c->polling);

    return 0;
}

int
gnss_i2c_init(gnss_t *ctx, struct gnss_i2c *i2c)
{
    ctx->transport.conf     = i2c;
    
    ctx->transport.start_rx = gnss_i2c_start_rx;
    ctx->transport.stop_rx  = gnss_i2c_stop_rx;
#if MYNEWT_VAL(GNSS_RX_ONLY) == 0
    ctx->transport.send     = gnss_i2c_send;
#endif

    os_callout_init(&i2c->polling, _gnss_internal_evq,
		    gnss_i2c_polling_handler, ctx);
    
    return 0;
}
