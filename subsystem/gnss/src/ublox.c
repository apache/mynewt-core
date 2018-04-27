#include <hal/hal_gpio.h>
#include <gnss/gnss.h>
#include <gnss/nmea.h>
#include <gnss/ublox.h>
#include <gnss/ubx.h>
#include <os/endian.h>

int
gnss_ublox_standby(gnss_t *ctx, int level)
{
    struct gnss_ublox *ubx = ctx->driver.conf;

    /* Sanity check */
    if (level <= GNSS_STANDBY_NONE) {
	return -1;
    }

    /* All the standby mode, even the one using the wakeup_pin, */
    /* Need to be able to send a PUBX command to enter standby  */
    if (ctx->transport.send == NULL) {
	return -1;
    }

    /* Was already in stand by? */
    if (ubx->standby_level != GNSS_STANDBY_NONE) {
	return -1;
    }
    
    switch(level) {
    case GNSS_STANDBY_LIGHT:
    case GNSS_STANDBY_DEEP:
    case GNSS_STANDBY_FULL:
	gnss_ubx_send_cmd(ctx, GNSS_UBX_MSG_RXM_PMREQ,
			   (uint8_t []) { 0x00, 0x00, 0x00, 0x00,
				          0x00, 0x00, 0x00, 0x02 }, 8);
	break;

    default:
	return -1;
    }

    ubx->standby_level = level;
    return 0;
}

int
gnss_ublox_wakeup(gnss_t *ctx)
{
    struct gnss_ublox *ubx = ctx->driver.conf;

    /* All the standby mode, even the one using the wakeup_pin, */
    /* Need to be able to send a PUBX command to enter standby  */
    if (ctx->transport.send == NULL) {
	return -1;
    }
    
    switch(ubx->standby_level) {
    case GNSS_STANDBY_LIGHT:
    case GNSS_STANDBY_DEEP:
    case GNSS_STANDBY_FULL:
	ctx->transport.send(ctx, (uint8_t[]) { 0xFF }, 1);
	break;

    case GNSS_STANDBY_NONE:
    default:
	return -1;
    }
    
    return 0;
}

/* See: UBX-CFG-RST */
int
gnss_ublox_reset(gnss_t *ctx, int type)
{
    struct gnss_ublox *ubx = ctx->driver.conf;
    
    switch(type) {
#if MYNEWT_VAL(GNSS_RX_ONLY) == 0
	
    case GNSS_RESET_HOT:  /* Hot restart */
	gnss_ubx_send_cmd(ctx, GNSS_UBX_MSG_CFG_RST,
			   (uint8_t []) { 0x00, 0x00, 0x01, 0x00 }, 4);
	break;

    case GNSS_RESET_WARM: /* Warm restart */
	gnss_ubx_send_cmd(ctx, GNSS_UBX_MSG_CFG_RST,
			   (uint8_t []) { 0x00, 0x01, 0x01, 0x00 }, 4);
	break;

    case GNSS_RESET_COLD: /* Cold restart */
	gnss_ubx_send_cmd(ctx, GNSS_UBX_MSG_CFG_RST,
			   (uint8_t []) { 0x01, 0xff, 0x01, 0x00 }, 4);
	break;

    case GNSS_RESET_HARD: /* Hard reset */
	if (ubx->reset_pin >= 0) {
#if __TO_BE_TESTED__
	    hal_gpio_write(ubx->reset_pin, 0);
	    os_time_delay(1);
	    hal_gpio_write(ubx->reset_pin, 1);
	    break;
#endif
	}
	/* FALL THROUGH */
	
    case GNSS_RESET_FULL: /* Full cold restart */
	gnss_ubx_send_cmd(ctx, GNSS_UBX_MSG_CFG_RST,
			   (uint8_t []) { 0xff, 0xff, 0x00, 0x00 }, 4);

	break;
#else
    case GNSS_RESET_HOT:  /* Hot restart */
    case GNSS_RESET_WARM: /* Warm restart */
    case GNSS_RESET_COLD: /* Cold restart */
    case GNSS_RESET_FULL: /* Full cold restart */
    case GNSS_RESET_HARD: /* Hard reset */
	if (ubx->reset_pin >= 0) {
#if __TO_BE_TESTED__
	    hal_gpio_write(ubx->reset_pin, 0);
	    os_time_delay(1);
	    hal_gpio_write(ubx->reset_pin, 1);
	    break;
#endif
	}
	return -1;
#endif

    default:
	return -1;
    }
    
    return 0;
}

int
gnss_ublox_on_data_ready(gnss_t *ctx, gnss_data_ready_callback_t cb)
{
    return -1;
}

int
gnss_ublox_is_data_ready(gnss_t *ctx)
{
    return -1;
}

int
gnss_ublox_init(gnss_t *ctx, struct gnss_ublox *ubx) {
    ctx->driver.conf    = ubx;

    ctx->driver.standby       = gnss_ublox_standby;
    ctx->driver.wakeup        = gnss_ublox_wakeup;
    ctx->driver.reset         = gnss_ublox_reset;

    ubx->standby_level  = GNSS_STANDBY_NONE;



#ifdef __TO_BE_TESTED__
    if (ubx->reset_pin >= 0) {
	hal_gpio_init_out(ubx->reset_pin, 1);
    }
    if (ubx->wakeup_pin >= 0) {
	hal_gpio_init_out(ubx->wakeup_pin, 1);
    }
#endif
    
    return 0;
}

int
gnss_ublox_set_bauds(gnss_t *ctx, uint32_t bauds) {
    
    uint8_t cfg_prt[20] = { /* Little endian encoded */
	0x01,                    /*  0 : portID       - UART     */
	0x00,                    /*  1 : -                       */
	0x00, 0x00,              /*  2 : txReady      - none     */
	0xC0, 0x08, 0x00, 0x00,  /*  4 : mode         - 8N1      */
	0x00, 0xC2, 0x01, 0x00,  /*  8 : baudRate     - 115200   */
	0x03, 0x00,              /* 12 : inProtoMask  - NMEA+UBX */
	0x03, 0x00,              /* 14 : outProtoMask - NMEA+UBX */
	0x02, 0x00,              /* 16 : flags        - ext. to. */
	0x00, 0x00               /* 18 : -                       */
    };

    put_le32(&cfg_prt[8], bauds);

    return gnss_ubx_send_cmd(ctx, 0x0600, cfg_prt, 20);
}
