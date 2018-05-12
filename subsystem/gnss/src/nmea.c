#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <gnss/gnss.h>
#include <gnss/nmea.h>
#include <gnss/log.h>

/* State for decoding parser
 */
#define GNSS_NMEA_STATE_VIRGIN          0x00
#define GNSS_NMEA_STATE_FLG_STARTED     0x01
#define GNSS_NMEA_STATE_FLG_CRC         0x02
#define GNSS_NMEA_STATE_FLG_CR          0x04
#define GNSS_NMEA_STATE_FLG_SKIP        0x08

/* Helper for getting NMEA specific decoder */
#define GNSS_NMEA_DECODER(x)                            \
    ((gnss_nmea_field_decoder_t)gnss_nmea_decoder_##x)

/*
 * Get field decoder function from the NMEA tag.
 * Also extract talker id and sentence id
 */
static gnss_nmea_field_decoder_t
gnss_nmea_get_field_decoder(const char *tag,
                            uint16_t *talker, uint16_t *sentence)
{
    gnss_nmea_field_decoder_t field_decoder = NULL;
    uint16_t                  t_id          = 0;
    uint16_t                  s_id          = 0;

    /* Common case: talker(2) + sentence(3) 
     */
    if (strlen(tag) == 5) {
        char talker_str[3] = { tag[0], tag[1], 0 };
        t_id = (uint16_t)strtoul(talker_str, NULL, 36);
        s_id = (uint16_t)strtoul(&tag[2],    NULL, 36);
        
        switch(s_id) {
#if MYNEWT_VAL(GNSS_NMEA_USE_GGA) > 0
        case GNSS_NMEA_SENTENCE_GGA:
            field_decoder = GNSS_NMEA_DECODER(gga);
            break;
#endif
#if MYNEWT_VAL(GNSS_NMEA_USE_GLL) > 0
        case GNSS_NMEA_SENTENCE_GLL:
            field_decoder = GNSS_NMEA_DECODER(gll);
            break;
#endif
#if MYNEWT_VAL(GNSS_NMEA_USE_GSA) > 0
        case GNSS_NMEA_SENTENCE_GSA:
            field_decoder = GNSS_NMEA_DECODER(gsa);
            break;
#endif
#if MYNEWT_VAL(GNSS_NMEA_USE_GST) > 0
        case GNSS_NMEA_SENTENCE_GST:
            field_decoder = GNSS_NMEA_DECODER(gst);
            break;
#endif
#if MYNEWT_VAL(GNSS_NMEA_USE_GSV) > 0
        case GNSS_NMEA_SENTENCE_GSV:
            field_decoder = GNSS_NMEA_DECODER(gsv);
            break;
#endif
#if MYNEWT_VAL(GNSS_NMEA_USE_RMC) > 0
        case GNSS_NMEA_SENTENCE_RMC:
            field_decoder = GNSS_NMEA_DECODER(rmc);
            break;
#endif
#if MYNEWT_VAL(GNSS_NMEA_USE_VTG) > 0
        case GNSS_NMEA_SENTENCE_VTG:
            field_decoder = GNSS_NMEA_DECODER(vtg);
            break;
#endif
        }
    }

    /* If no decoder found, look for proprietary tag
     */
    if (field_decoder == NULL) {
#if MYNEWT_VAL(GNSS_NMEA_USE_PGACK) > 0
        if (!strcmp(tag, "PGACK")) {
            t_id = GNSS_NMEA_TALKER_MTK;
            s_id = GNSS_NMEA_SENTENCE_PGACK;
            field_decoder = GNSS_NMEA_DECODER(pgack);
        }
#endif
#if MYNEWT_VAL(GNSS_NMEA_USE_PMTK) > 0
        if (!strncmp(tag, "PMTK", 4)) {
            t_id = GNSS_NMEA_TALKER_MTK;
            s_id = GNSS_NMEA_SENTENCE_PMTK;
            field_decoder = GNSS_NMEA_DECODER(pmtk);
        }
#endif
#if MYNEWT_VAL(GNSS_NMEA_USE_PUBX) > 0
        if (!strcmp(tag, "PUBX")) {
            t_id = GNSS_NMEA_TALKER_UBLOX;
            s_id = GNSS_NMEA_SENTENCE_PUBX;
            field_decoder = GNSS_NMEA_DECODER(pubx);
        }
#endif
    }

    /* If decoder found, save information about talker/sentence 
     */
    if (field_decoder != NULL) {
        *talker   = t_id;
        *sentence = s_id;
    }

    /* Return field decoder
     */
    return field_decoder;
}

/*
 * Decode an NMEA field using accumulated internal data
 */
static int
gnss_nmea_decode_field(struct gnss_nmea *gn)
{
    /* Check that we are in the main part 
     * (ie: started but no crc, no <cr>)
     */
    if (gn->state != GNSS_NMEA_STATE_FLG_STARTED) {
        return GNSS_BYTE_DECODER_ERROR;
    }

    /* Skipping? */
    if (gn->msg == NULL) {
        return GNSS_BYTE_DECODER_DECODING;
    }
    
    /* Make it a NUL terminated string,
     *  we kept 1 byte in the buffer for that purpose
     */
    gn->buffer[gn->bufcnt] = '\0';    

    /* Special handling for NMEA tag (field id == 0)
     *  we need to get the dedicated decoder
     */
    if (gn->fid == 0) {
        gn->field_decoder =
            gnss_nmea_get_field_decoder(gn->buffer,
                                        &gn->msg->talker, &gn->msg->sentence);
        /* If we don't have a field decoder, just skip everything */
        if (gn->field_decoder == NULL) {
            gn->msg = NULL;
            return GNSS_BYTE_DECODER_DECODING;
        }
    }

    /* Decoder is called
     *  - we don't need to check for field_decoder == NULL
     *    as we already have skipped in case of not decoding
     *  - we are calling decoder for field 0 (ie: tag), which seems
     *    unecessary but is required for proprietary tag (ex: PMTK001)
     */
    assert(gn->field_decoder != NULL);
    switch(gn->field_decoder(&gn->msg->data, gn->buffer, gn->fid)) {
    case -1: return GNSS_BYTE_DECODER_ERROR;
    case  0: return GNSS_BYTE_DECODER_FAILED;
    default: assert(0);
    case  1: return GNSS_BYTE_DECODER_DECODING;
    }
}

/*
 * Decoding of NMEA sentence
 *
 * @param ctx           GNSS context
 * @param gn            NMEA decoder context
 * @param byte          byte to decode
 *
 * @return GNSS_BYTE_DECODER_*
 */
int
gnss_nmea_byte_decoder(gnss_t *ctx, struct gnss_nmea *gn, uint8_t byte)
{
    int rc = GNSS_BYTE_DECODER_ERROR;
    
    /* Check if we are processing left over garbage */
    if ((gn->state == GNSS_NMEA_STATE_VIRGIN) && (byte != '$')) {
        return GNSS_BYTE_DECODER_SYNCING;
    }

    /* Decode NMEA one byte at a time */
    switch(byte) {
    /* Start of sentence marker */
    case '$': {
        struct gnss_nmea_event *evt;
        
        /* Ensure context is reseted */
        gn->state      = 0x01;
        gn->fid        = 0;
        gn->crc        = 0;
        gn->bufcnt     = 0;
        gn->msg        = NULL;
        
        /* Ensure we've got an event */
        evt = (struct gnss_nmea_event *)
            gnss_prepare_event(ctx, GNSS_EVENT_NMEA);
        if (evt != NULL) {
            gn->msg = &evt->nmea;
        }

        break;
    }
        
    /* Field separator */
    case ',':
        /* Decode current field (will also ensure state is legit) */
        rc = gnss_nmea_decode_field(gn);
        if (rc < GNSS_BYTE_DECODER_DECODING) {
            goto oops;
        }
        
        /* Mark field as processed */
        gn->crc     ^= byte; /* Compute CRC (',' is in main part) */
        gn->fid     += 1;    /* Moving to next field              */
        gn->bufcnt   = 0;    /* Reset field buffer                */
        break;

    /* CRC marker */
    case '*':
        /* Decode current field (will also ensure state is legit) */
        rc = gnss_nmea_decode_field(gn);
        if (rc < GNSS_BYTE_DECODER_DECODING) {
            goto oops;
        }
        
        /* Mark field as processed, and starting CRC decoding */
        gn->bufcnt = 0;                        /* Reset field buffer    */
        gn->state |= GNSS_NMEA_STATE_FLG_CRC;  /* Start of CRC decoding */
        break;

    /* <CR> marker */
    case '\r':
        /* Ensure that's the first <cr> */
        if (gn->state & GNSS_NMEA_STATE_FLG_CR) {
            gn->stats.parsing_error++;
            goto oops;
        }

        /* Either marking end of CRC or end of field */
        if (gn->state & GNSS_NMEA_STATE_FLG_CRC) {
            uint8_t crc;

            /* Make the buffer a NUL terminated string,       */
            /*  we kept 1 byte in the buffer for that purpose */
            gn->buffer[gn->bufcnt] = '\0';

            /* Decode CRC */
            if (!gnss_nmea_field_parse_crc(gn->buffer, &crc)) {
                gn->stats.parsing_error++;
                goto oops;
            }

            /* Validate CRC */
            if (crc != gn->crc) {
                gn->stats.crc_error++;
                goto oops;
            }
            
        } else {
            /* Decode current field (will also ensure state is legit) */
            rc = gnss_nmea_decode_field(gn);
            if (rc < GNSS_BYTE_DECODER_DECODING) {
                goto oops;
            }
        }

        /* Mark state as <CR> acknowledged */
        gn->state |= GNSS_NMEA_STATE_FLG_CR;
        break;

    /* <LF> marker: Sentence is finished */
    case '\n':
        /* Ensure we've got everything                   */
        /*  (at least started and <cr>, crc is optional) */
        if ((gn->state & (GNSS_NMEA_STATE_FLG_STARTED |
                          GNSS_NMEA_STATE_FLG_CR      )) !=
            (GNSS_NMEA_STATE_FLG_STARTED|GNSS_NMEA_STATE_FLG_CR)) {
            gn->stats.parsing_error++;
            goto oops;
        }

        /* Mark for reset */
        gn->state = GNSS_NMEA_STATE_VIRGIN;

        /* Job's done */
        if (gn->msg != NULL) {
            gnss_emit_event(ctx);
            return GNSS_BYTE_DECODER_DECODED;
        } else {
            return GNSS_BYTE_DECODER_UNHANDLED;
        }

    /* Other chars */
    default:
        /* Ensure that's a legal character */
        if (!isprint(byte)) {
            goto oops;
        }
        
        /* Check that buffer is not full (keeping 1 byte for NUL-char) */
        if (gn->bufcnt >= (MYNEWT_VAL(GNSS_NMEA_FIELD_BUFSIZE)-1)) {
            gn->stats.buffer_full++;
            gn->msg = NULL; /* Our fault, will skip data :( */
        }       
        /* If decoding main part, compute CRC */
        if (gn->state == GNSS_NMEA_STATE_FLG_STARTED) {
            gn->crc ^= byte;
        }
        /* Add byte to buffer */
        gn->buffer[gn->bufcnt++] = byte;
        break;
    }

    return GNSS_BYTE_DECODER_DECODING;

 oops:
    /* Reset state */
    gn->state = GNSS_NMEA_STATE_VIRGIN;
    return rc;
}

static int
gnss_nmea_decoder(gnss_t *ctx, uint8_t byte)
{
    int rc;

    rc = gnss_nmea_byte_decoder(ctx,
                (struct gnss_nmea *)ctx->protocol.conf, byte);

    rc = gnss_check_scrambled_transport(ctx, rc);
    
    return rc;
}

bool
gnss_nmea_init(gnss_t *ctx, struct gnss_nmea *nmea)
{
    ctx->protocol.conf    = nmea;
    ctx->protocol.decoder = gnss_nmea_decoder; /* The decoder */
    return true;
}

bool
gnss_nmea_send_cmd(gnss_t *ctx, char *cmd)
{
    char  crc[3];
    char *msg[5];
    int   i;    
    
    /* Generating crc string */
    snprintf(crc, sizeof(crc), "%02X", gnss_nmea_crc(cmd));

    /* Building message array */
    msg[0] = "$";
    msg[1] = cmd;
    msg[2] = "*";
    msg[3] = crc;
    msg[4] = "\r\n";

    /* Sending command */
    LOG_INFO(&_gnss_log, LOG_MODULE_DEFAULT, "Command: $%s*%s\n", cmd, crc);
    
    for (i = 0 ; i < sizeof(msg) / sizeof(*msg) ; i++) {
        gnss_send(ctx, (uint8_t*)msg[i], strlen(msg[i]));
    }

    /* Ensure a 10 ms delay */
    os_time_delay(GNSS_MS_TO_TICKS(10)); // XXX: don't hardcode

    return true;
}

void
gnss_nmea_log(struct gnss_nmea_message *nmea)
{    
    switch(nmea->talker) {
    case GNSS_NMEA_TALKER_MTK:
        switch(nmea->sentence) {
#if MYNEWT_VAL(GNSS_NMEA_USE_PGACK) > 0
        case GNSS_NMEA_SENTENCE_PGACK:
            gnss_nmea_log_pgack(&nmea->data.pgack);
            break;
#endif
#if MYNEWT_VAL(GNSS_NMEA_USE_PMTK) > 0
        case GNSS_NMEA_SENTENCE_PMTK:
            gnss_nmea_log_pmtk(&nmea->data.pmtk);
            break;
#endif
        }
        break;

    case GNSS_NMEA_TALKER_UBLOX:
        switch(nmea->sentence) {
#if MYNEWT_VAL(GNSS_NMEA_USE_PUBX) > 0
        case GNSS_NMEA_SENTENCE_PUBX:
            gnss_nmea_log_pubx(&nmea->data.pubx);
            break;
#endif
        }
        break;

    default:
        switch(nmea->sentence) {
#if MYNEWT_VAL(GNSS_NMEA_USE_GGA) > 0
        case GNSS_NMEA_SENTENCE_GGA:
            gnss_nmea_log_gga(&nmea->data.gga);
            break;
#endif
#if MYNEWT_VAL(GNSS_NMEA_USE_GLL) > 0
        case GNSS_NMEA_SENTENCE_GLL:
            gnss_nmea_log_gll(&nmea->data.gll);
            break;
#endif
#if MYNEWT_VAL(GNSS_NMEA_USE_GSA) > 0
        case GNSS_NMEA_SENTENCE_GSA:
            gnss_nmea_log_gsa(&nmea->data.gsa);
            break;
#endif
#if MYNEWT_VAL(GNSS_NMEA_USE_GST) > 0
        case GNSS_NMEA_SENTENCE_GST:
            gnss_nmea_log_gst(&nmea->data.gst);
            break;
#endif
#if MYNEWT_VAL(GNSS_NMEA_USE_GSV) > 0
        case GNSS_NMEA_SENTENCE_GSV:
            gnss_nmea_log_gsv(&nmea->data.gsv);
            break;
#endif
#if MYNEWT_VAL(GNSS_NMEA_USE_RMC) > 0
        case GNSS_NMEA_SENTENCE_RMC:
            gnss_nmea_log_rmc(&nmea->data.rmc);
            break;
#endif
#if MYNEWT_VAL(GNSS_NMEA_USE_VTG) > 0
        case GNSS_NMEA_SENTENCE_VTG:
            gnss_nmea_log_vtg(&nmea->data.vtg);
            break;
#endif
        }
        break;
    }
}
