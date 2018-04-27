#include <string.h>
#include <stdio.h>

#include <gnss/gnss.h>
#include <gnss/nmea.h>
#include <gnss/ubx.h>
#include <uart/uart.h>
#include <gnss/uart.h>

#include <gnss/log.h>
#include <stats/stats.h>


/* Define the stats section and records */
STATS_SECT_START(gnss_stat_section)
    STATS_SECT_ENTRY(allocation_errors)
    STATS_SECT_ENTRY(errors)
STATS_SECT_END

/* Define stat names for querying */
STATS_NAME_START(gnss_stat_section)
    STATS_NAME(gnss_stat_section, allocation_errors)
    STATS_NAME(gnss_stat_section, errors)
STATS_NAME_END(gnss_stat_section)

/* Global variable used to hold stats data */
STATS_SECT_DECL(gnss_stat_section) g_gnss_stats;




struct log _gnss_log;


struct gnss_dummy_event {
    struct gnss_event event; /* Need to be first */
    uint8_t data[];
};

/* Structure used for computing minimal memory allocation for
 * supported protocols
 */
union gnss_event_memory {
    struct gnss_dummy_event a;
#if MYNEWT_VAL(GNSS_USE_NMEA_PROTOCOL) > 0
    struct gnss_nmea_event  b; 
#endif
#if MYNEWT_VAL(GNSS_USE_UBX_PROTOCOL) > 0
    struct gnss_ubx_event   c; 
#endif
};


#define GNSS_MESSAGE_EVENT_MAXSIZE sizeof(union gnss_event_memory)



static struct os_eventq *_gnss_evq = NULL;
struct os_eventq *_gnss_internal_evq = NULL;

static struct os_mempool _gnss_event_pool;
static os_membuf_t gnss_event_buffer[
	     OS_MEMPOOL_SIZE(MYNEWT_VAL(GNSS_EVENT_MAX),
			     GNSS_MESSAGE_EVENT_MAXSIZE)];




#define GNSS_MS_TO_TICKS(ms) (((ms) * OS_TICKS_PER_SEC + 999) / 1000)





static void
gnss_event_cb(struct os_event *ev)
{
    gnss_event_t *event = (gnss_event_t *) ev;
    gnss_t       *ctx   = (gnss_t *) ev->ev_arg;

    /* Perform logging */
#if MYNEWT_VAL(GNSS_LOG) > 0
    switch(event->type) {
#if MYNEWT_VAL(GNSS_NMEA_LOG) > 0
    case GNSS_EVENT_NMEA:
        gnss_nmea_log(&((struct gnss_nmea_event *)event)->nmea);
	break;
#endif
#if MYNEWT_VAL(GNSS_UBX_LOG) > 0
    case GNSS_EVENT_UBX:
        gnss_ubx_log(&((struct gnss_ubx_event *)event)->ubx);
	break;
#endif
    }
#endif

    /* Trigger user callback */
    if (ctx->callback) {
	ctx->callback(event->type, &((struct gnss_dummy_event*)event)->data);
    }

    /* Put event back to the memory block */
    os_memblock_put(&_gnss_event_pool, ev);
}

static void
gnss_error_event_cb(struct os_event *ev)
{
    gnss_t *ctx = (gnss_t *) ev->ev_arg;

    /* Trigger user callback */
    if (ctx->error_callback) {
	ctx->error_callback(ctx, ctx->error);
    }

    /* Clear error */
    ctx->error = GNSS_ERROR_NONE;
}


void
gnss_emit_error_event(gnss_t *ctx, unsigned int error)
{
    ctx->error = error;
    os_eventq_put(_gnss_evq, &ctx->error_event.os_event);
}

void
gnss_emit_event(gnss_t *ctx)
{
    if (ctx->event == NULL) {
	return;
    }
    
    os_eventq_put(_gnss_evq, &ctx->event->os_event);
    ctx->event = NULL;
}


gnss_event_t *
gnss_prepare_event(gnss_t *ctx, uint8_t type)
{
    /* Recycle event if already present */
    if (ctx->event != NULL) {
	ctx->event->type = type;

    /* Otherwise allocate and prepare new event */
    } else {
	ctx->event = os_memblock_get(&_gnss_event_pool);
	if (ctx->event) {
	    ctx->event->type     = type;
	    ctx->event->os_event = (struct os_event) {
		.ev_cb  = gnss_event_cb,
		.ev_arg = ctx,
	    };
	} else {
	    STATS_INC(g_gnss_stats, allocation_errors);
	}
    }

    return ctx->event;
}

void
gnss_eventq_set(struct os_eventq *evq)
{
    _gnss_evq = evq;
}

void
gnss_internal_eventq_set(struct os_eventq *evq)
{
    _gnss_internal_evq = evq;
}

void
gnss_pkg_init(void)
{
    int rc;
    
    /* Set default event queue */
    _gnss_evq          = os_eventq_dflt_get();
    _gnss_internal_evq = os_eventq_dflt_get();

    /* Register logger */
    rc = log_register("gnss", &_gnss_log,
		      &log_console_handler, NULL, LOG_LEVEL_DEBUG);
    assert(rc == 0);

    /* Initialise memory pool */
    rc = os_mempool_init(&_gnss_event_pool,
			 MYNEWT_VAL(GNSS_EVENT_MAX),
			 GNSS_MESSAGE_EVENT_MAXSIZE,
			 gnss_event_buffer,
			 "gnss_evt_pool");
    assert(rc == 0);

    /* Initialise the stats entry */
    rc = stats_init(STATS_HDR(g_gnss_stats),
		    STATS_SIZE_INIT_PARMS(g_gnss_stats, STATS_SIZE_32),
		    STATS_NAME_INIT_PARMS(gnss_stat_section));
    SYSINIT_PANIC_ASSERT(rc == 0);
    /* Register the entry with the stats registry */
    rc = stats_register("gnss", STATS_HDR(g_gnss_stats));
    SYSINIT_PANIC_ASSERT(rc == 0);

}

void
gnss_init(gnss_t *ctx,
	  gnss_callback_t callback,
	  gnss_error_callback_t error_callback)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->callback           = callback;
    ctx->error_callback     = error_callback;

    ctx->error_event.os_event.ev_cb  = gnss_error_event_cb;
    ctx->error_event.os_event.ev_arg = ctx;

}



#if MYNEWT_VAL(GNNS_CHECK_SCRAMBLED_TRANSPORT > 0)
int
gnss_check_scrambled_transport(gnss_t *ctx, int code) {
    switch(code) {

    /* Sucessfully decoded (but not necesarrily processed) */
    case GNSS_BYTE_DECODER_UNHANDLED:
    case GNSS_BYTE_DECODER_DECODED:
	ctx->decoder.error   = 0;
	ctx->decoder.syncing = 0;
	return code;
	
    /* One more decoding error */
    case GNSS_BYTE_DECODER_ERROR:
	ctx->decoder.error++;
	break;
	
    /* One more character skipped */
    case GNSS_BYTE_DECODER_SYNCING:
	ctx->decoder.syncing++;
	break;

    /* Othercase have no impact */
    default:
	return code;
    }

    /* Decide to emit an error according to threshold */
    if ((ctx->decoder.syncing > MYNEWT_VAL(GNNS_DECODER_SYNCING_THRESHOLD)) ||
	(ctx->decoder.error   > MYNEWT_VAL(GNNS_DECODER_ERROR_THRESHOLD  ))) {
	ctx->decoder.syncing = 0;
	ctx->decoder.error   = 0;
	gnss_emit_error_event(ctx, GNSS_ERROR_SCRAMBLED_TRANSPORT);
	return GNSS_BYTE_DECODER_ABORTED;
    }
    
    return code;
}
#endif
