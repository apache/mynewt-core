#include <stdio.h>
#include <hal/hal_gpio.h>
#include <gnss/gnss.h>
#include <gnss/nmea.h>
#include <gnss/mediatek.h>

static int
gnss_mediatek_standby(gnss_t *ctx, int level)
{
#if MYNEWT_VAL(GNSS_RX_ONLY) == 0
    struct gnss_mediatek *mtk = ctx->driver.conf;

    /* Sanity check */
    if (level <= GNSS_STANDBY_NONE) {
	return -1;
    }

    /* All the standby mode, even the one using the wakeup_pin, */
    /* Need to be able to send a PMTK command to enter standby  */
    if (ctx->transport.send == NULL) {
	return -1;
    }

    /* Was already in stand by? */
    if (mtk->standby_level != GNSS_STANDBY_NONE) {
	return -1;
    }
    
    switch(level) {
    case GNSS_STANDBY_LIGHT:
    case GNSS_STANDBY_DEEP:
    case GNSS_STANDBY_FULL:
	if (mtk->wakeup_pin >= 0) {
#ifdef __TO_BE_TESTED__
	    hal_gpio_write(mtk->wakeup_pin, 0);
	    gnss_nmea_send_cmd(ctx, "PMTK225,0");
	    gnss_nmea_send_cmd(ctx, "PMTK225,4");
#else
	    return -1;
#endif
	} else {
	    gnss_nmea_send_cmd(ctx, "PMTK161,0");
	}
	break;

    default:
	return -1;
    }

    mtk->standby_level = level;
    return 0;
#else
    return -1;
#endif
}

static int
gnss_mediatek_wakeup(gnss_t *ctx)
{
#if MYNEWT_VAL(GNSS_RX_ONLY) == 0
    struct gnss_mediatek *mtk = ctx->driver.conf;

    /* All the standby mode, even the one using the wakeup_pin, */
    /* Need to be able to send a PMTK command to enter standby  */
    if (ctx->transport.send == NULL) {
	return -1;
    }
    
    switch(mtk->standby_level) {
    case GNSS_STANDBY_LIGHT:
    case GNSS_STANDBY_DEEP:
    case GNSS_STANDBY_FULL:
	if (mtk->wakeup_pin >= 0) {
#if __TO_BE_TESTED__
	    hal_gpio_write(g->wakeup_pin, 1);
#else
	    return -1;
#endif
	} else {
	    ctx->transport.send(ctx, (uint8_t *)"\r\n", 2);
	}
	break;

    case GNSS_STANDBY_NONE:
    default:
	return -1;
    }
    
    return 0;
#else
    return -1;
#endif
}

static int
gnss_mediatek_reset(gnss_t *ctx, int type)
{
    struct gnss_mediatek *mtk = ctx->driver.conf;
    
    switch(type) {
#if MYNEWT_VAL(GNSS_RX_ONLY) == 0
    case GNSS_RESET_HOT:  /* Hot restart */
	gnss_nmea_send_cmd(ctx, "PMTK101");
	break;

    case GNSS_RESET_WARM: /* Warm restart */
	gnss_nmea_send_cmd(ctx, "PMTK102");
	break;

    case GNSS_RESET_COLD: /* Cold restart */
	gnss_nmea_send_cmd(ctx, "PMTK103"); 
	break;

    case GNSS_RESET_HARD: /* Hard reset */
	if (mtk->reset_pin >= 0) {
#if __TO_BE_TESTED__
	    hal_gpio_write(mtk->reset_pin, 0);
	    os_time_delay(1);
	    hal_gpio_write(mtk->reset_pin, 1);
	    break;
#endif
	}
	/* FALL THROUGH */
	
    case GNSS_RESET_FULL: /* Full cold restart */
	gnss_nmea_send_cmd(ctx, "PMTK104"); 
	break;
#else
    case GNSS_RESET_HOT:  /* Hot restart */
    case GNSS_RESET_WARM: /* Warm restart */
    case GNSS_RESET_COLD: /* Cold restart */
    case GNSS_RESET_FULL: /* Full cold restart */
    case GNSS_RESET_HARD: /* Hard reset */
	if (mtk->reset_pin >= 0) {
#if __TO_BE_TESTED__
	    hal_gpio_write(mtk->reset_pin, 0);
	    os_time_delay(1);
	    hal_gpio_write(mtk->reset_pin, 1);
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

static int
gnss_mediatek_on_data_ready(gnss_t *ctx, gnss_data_ready_callback_t cb)
{
    struct gnss_mediatek *mtk = ctx->driver.conf;

    if (mtk->data_ready_pin < 0) {
	return -1;
    }

    if (cb != NULL) {
	hal_gpio_irq_init(mtk->data_ready_pin, cb, ctx,
			  HAL_GPIO_TRIG_LOW, HAL_GPIO_PULL_NONE);
	hal_gpio_irq_enable(mtk->data_ready_pin);
    } else {
	hal_gpio_irq_disable(mtk->data_ready_pin);
	hal_gpio_irq_release(mtk->data_ready_pin);
    }

    return 0;
}

static int
gnss_mediatek_is_data_ready(gnss_t *ctx)
{
    struct gnss_mediatek *mtk = ctx->driver.conf;

    if (mtk->data_ready_pin < 0) {
	return -1;
    }
    
    return hal_gpio_read(mtk->data_ready_pin) ? 0 : 1;
}

int
gnss_mediatek_init(gnss_t *ctx, struct gnss_mediatek *mtk) {
    ctx->driver.conf    = mtk;

    ctx->driver.standby       = gnss_mediatek_standby;
    ctx->driver.wakeup        = gnss_mediatek_wakeup;
    ctx->driver.reset         = gnss_mediatek_reset;
    ctx->driver.on_data_ready = gnss_mediatek_on_data_ready;
    ctx->driver.is_data_ready = gnss_mediatek_is_data_ready;

    mtk->standby_level  = GNSS_STANDBY_NONE;



#ifdef __TO_BE_TESTED__
    if (mtk->reset_pin >= 0) {
	hal_gpio_init_out(mtk->reset_pin, 1);
    }
    if (mtk->wakeup_pin >= 0) {
	hal_gpio_init_out(mtk->wakeup_pin, 1);
    }
#endif
    
    return 0;
}


int
gnss_mediatek_set_bauds(gnss_t *ctx, uint32_t bauds) {
    char cmd[12]; /* PGCMD,232,x */
    int b;
    
    /* Convert bauds */
    switch (bauds) {
    case   4800: b = 0; break;
    case   9600: b = 1; break;
    case  14400: b = 2; break;
    case  19200: b = 3; break;
    case  38400: b = 4; break;
    case  57600: b = 5; break;
    case 115200: b = 6; break;
    default: return -1;
    }

    /* Switch to SDK mode */
    gnss_nmea_send_cmd(ctx, "PGCMD,380,7");
    /* Enable GNSS constellation */
    snprintf(cmd, sizeof(cmd), "PGCMD,232,%d", b);
    gnss_nmea_send_cmd(ctx, cmd);
    /* Perform full cold start */
    gnss_nmea_send_cmd(ctx, "PMTK104");

    /* Wait for the reboot */
    os_time_delay(100);

    return 1;
}

int
gnss_mediatek_nmea_rate(gnss_t *ctx, struct gnss_nmea_rate *rates) {
    static char *cmd  = "PMTK314,1,1,1,1,1,5,0,0,0,0,0,0,0,0,0,0,0,0,0";
    uint16_t rate;
    int idx = 0;
    
    if (rates == NULL) {
	gnss_nmea_send_cmd(ctx, "PMTK314,-1");
	return 1;
    }

    /* Reset command string */
    for (idx = 0; idx < 19 ; idx++) {
	cmd[idx * 2] = '0';
    }
	
    for ( ; rates->sentence != GNSS_NMEA_SENTENCE_NONE ; rates++) {
	idx       = -1;
	rate = rates->rate;
	if (rate > 5) {
	    rate = 5;
	}
	
	switch(rates->sentence) {
	case GNSS_NMEA_SENTENCE_GLL:  idx =  0; break;
	case GNSS_NMEA_SENTENCE_RMC:  idx =  1; break;
	case GNSS_NMEA_SENTENCE_VTG:  idx =  2; break;
	case GNSS_NMEA_SENTENCE_GGA:  idx =  3; break;
	case GNSS_NMEA_SENTENCE_GSA:  idx =  4; break;
	case GNSS_NMEA_SENTENCE_GSV:  idx =  5; break;
	case GNSS_NMEA_SENTENCE_ZDA:  idx = 17; break;
	case GNSS_NMEA_SENTENCE_MCHN: idx = 18; break;
	}

	if (idx > 0) {
	    cmd[8 + idx * 2] = '0' + rate;
	}
    }
    
    gnss_nmea_send_cmd(ctx, cmd);

    return 1;
}

int
gnss_mediatek_gnss(gnss_t *ctx, uint32_t gnss_mask) {
    char cmd[18]; /* PGCMD,229,x,x,x,x
		     PMTK353,x,x,x,0,x */
    uint8_t gps     = 0;
    uint8_t glonass = 0;
    uint8_t galileo = 0;
    uint8_t beidou  = 0;
    uint8_t sbas    = 0;
    uint8_t qzss    = 0;
    
    if (gnss_mask & (1 << GNSS_GPS    )) { gps     = 1; }
    if (gnss_mask & (1 << GNSS_GLONASS)) { glonass = 1; }
    if (gnss_mask & (1 << GNSS_GALILEO)) { galileo = 1; }
    if (gnss_mask & (1 << GNSS_BEIDOU )) { beidou  = 1; }
    if (gnss_mask & (1 << GNSS_SBAS   )) { sbas    = 1; }
    if (gnss_mask & (1 << GNSS_QZSS   )) { qzss    = 1; }
    
    /* Switch to SDK mode */
    gnss_nmea_send_cmd(ctx, "PGCMD,380,7");
    /* Enable GNSS constellation */
    snprintf(cmd, sizeof(cmd), "PGCMD,229,%d,%d,%d,%d",
	     gps, glonass, beidou, galileo);
    gnss_nmea_send_cmd(ctx, cmd);
    /* Perform full cold start */
    gnss_nmea_send_cmd(ctx, "PMTK104");

    /* Wait for the reboot */
    os_time_delay(100);

    /* Search mode (not available on MT3339) */
    snprintf(cmd, sizeof(cmd), "PMTK353,%d,%d,%d,0,%d",
	     gps, glonass, beidou, galileo);
    gnss_nmea_send_cmd(ctx, cmd);

    /* SBAS */
    snprintf(cmd, sizeof(cmd), "PMTK513,%d", sbas);
    gnss_nmea_send_cmd(ctx, cmd);

    /* QZSS */
    snprintf(cmd, sizeof(cmd), "PMTK352,%d", qzss);
    gnss_nmea_send_cmd(ctx, cmd);
    
    return 1;
}	 
// PMTK225
