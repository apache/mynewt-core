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



#define GNSS_MEDIATEK_GLL  0
#define GNSS_MEDIATEK_RMC  1
#define GNSS_MEDIATEK_VTG  2
#define GNSS_MEDIATEK_GGA  3
#define GNSS_MEDIATEK_GSA  4
#define GNSS_MEDIATEK_GDV  5
#define GNSS_MEDIATEK_ZDA  17
#define GNSS_MEDIATEK_MCHN 18




void
gnss_nmea_output(void) {
    /*
      0 GLL
      1 RMC
      2 VTG
      3 GGA
      4 GSA
      5 GDV
      17 ZDA
      18 MCHN
    (void)"PMTK314,1,1,1,1,1,5,0,0,0,0,0,0,0,0,0,0,0,0,0";
    */
}

		 
// PMTK225
