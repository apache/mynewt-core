#include <string.h>
#include <gnss/gnss.h>
#include <gnss/ubx.h>
#include <gnss/log.h>

int
gnss_ubx_send_cmd(gnss_t *ctx, uint16_t msg, uint8_t *bytes, uint16_t size)
{
    unsigned int i;
    uint8_t hdr[6] = { 0xB5, 0x62 };
    uint8_t crc[2] = { 0x00, 0x00 };

    /* Set message (Class / Id) */
    hdr[2] = msg >> 8;   /* Class */
    hdr[3] = msg & 0xFF; /* Id    */

    /* Set Length */
    put_le16(&hdr[4], size);

    /* Compute CRC: class/id + length + payload */
    for (i = 2 ; i < 6    ; i++) {
        crc[0] += hdr[i];
        crc[1] += crc[0];
    }
    for (i = 0 ; i < size ; i++) {
        crc[0] += bytes[i];
        crc[1] += crc[0];
    }
    
    /* Sending command */
    GNSS_LOG_INFO("UBX Command: class/id=0x%04x, crc=0x%02x%02x, len=%d\n",
		  msg, crc[0], crc[1], size);

    ctx->transport.send(ctx, hdr, 6);
    ctx->transport.send(ctx, bytes, size);
    ctx->transport.send(ctx, crc, 2);

    /* Bytes really send on the wire */
    return 6 + size + 2;
}

/*
 * Decoding of UBX sentence
 *
 * @param ctx           GNSS context
 * @param gn            UBX decoder context
 * @param byte          byte to decode
 *
 * @return GNSS_BYTE_DECODER_*
 */
int
gnss_ubx_byte_decoder(gnss_t *ctx, struct gnss_ubx *gu, uint8_t byte)
{
    /* Compute CRC on the fly                   */
    /* (length is set to 0 until index reach 6) */
    if ((gu->idx >= 2) && (gu->idx < (6 + gu->len))) {
        gu->crc_a += byte;
        gu->crc_b += gu->crc_a;
    }
    
    switch(gu->idx) {
    /* Sync char */
    case 0: /* 0xB5 */
        if (byte != 0xB5) {
            return GNSS_BYTE_DECODER_SYNCING;
        } else {
            /* Ensure context is reseted */
            memset(gu, 0, sizeof(*gu));
        }
        break;
    case 1: /* 0x62 */
        if (byte != 0x62) {
            gu->idx = 0;
            return GNSS_BYTE_DECODER_SYNCING;
        } else {
            struct gnss_ubx_event *evt;
            /* Ensure we've got an event */
            evt = (struct gnss_ubx_event *)
                gnss_prepare_event(ctx, GNSS_EVENT_UBX);
            if (evt != NULL) {
		/* Set msg pointer, otherwise data will be discarded */
		gu->msg = &evt->ubx;
	    }
	}
        break;
        
    /* Class / Id */
    case 2: /* Class */
        gu->class = byte;
        break;
    case 3: /* ID */
        gu->id    = byte;
        break;
        
    /* Length (little endian) */
    case 4:
        gu->len   = ((uint16_t)byte);
        break;
    case 5:
        gu->len  |= ((uint16_t)byte) << 8;

        /* Ensure we've got enough storage for the payload */
        if (gu->len > sizeof(gu->msg->data)) {
	    gu->msg = NULL; /* Our fault :( */
        }
        break;
        
    /* Payload or CRC */
    default: {
        uint16_t pidx = gu->idx - 6;

        /* Payload */
        if (pidx < gu->len) {
	    if (gu->msg) {
		gu->msg->data[pidx] = byte;
	    }
            break; 
        }
        
        /* CRC */
        switch (pidx - gu->len) {
        case 0:
            gu->crc       = ((uint16_t)byte);
            break;
        case 1:
            gu->crc      |= ((uint16_t)byte) << 8;

            if (gu->crc != ((((uint16_t)gu->crc_a)      ) |
                            (((uint16_t)gu->crc_b) << 8))) {
                gu->idx = 0;
                return GNSS_BYTE_DECODER_ERROR;
            }

            /* Mark for reset */
	    gu->idx = 0;

	    /* Job's done */
            if (gu->msg != NULL) {
                gu->msg->len = gu->len;
                gu->msg->class_id = ((uint16_t)gu->class) << 8 |
		                    ((uint16_t)gu->id   ) << 0 ;
                gnss_emit_event(ctx);
		return GNSS_BYTE_DECODER_DECODED;
            } else {
		return GNSS_BYTE_DECODER_UNHANDLED;
	    }
        }
        break;
    }
    }
    
    gu->idx++;
    return GNSS_BYTE_DECODER_DECODING;
}
    
static int
gnss_ubx_decoder(gnss_t *ctx, uint8_t byte)
{
    int rc;
    struct gnss_ubx *gu = ctx->protocol.conf;
    
    rc = gnss_ubx_byte_decoder(ctx, gu, byte);

    return rc;
}

bool
gnss_ubx_init(gnss_t *ctx, struct gnss_ubx *ubx)
{
    ctx->protocol.conf    = ubx;
    ctx->protocol.decoder = gnss_ubx_decoder; /* The decoder */
    ubx->idx = 0;
    
    return true;
}

struct gnss_ubx_cfg_gnss {
    uint8_t msgVer;
    uint8_t numTrkChHw;
    uint8_t numTrkChUse;
    uint8_t numConfigBlocks;
    struct {
	uint8_t gnssId;
	uint8_t resTrkCh;
	uint8_t maxTrkCh;
	uint8_t _reserved;
	uint32_t flags;
    } block[];
};
     

void
gnss_ubx_log(struct gnss_ubx_message *ubx)
{    
    GNSS_LOG_INFO("UBX: class=0x%02x, id=0x%02x, len=%d\n",
		  ubx->class_id >> 8, ubx->class_id & 0xFF, ubx->len);

    switch(ubx->class_id) {
    case GNSS_UBX_MSG_MON_VER: {
	GNSS_LOG_INFO("UBX[MON_VER]: %s\n", (char*)ubx->data);
	GNSS_LOG_INFO("UBX[MON_VER]: %s\n", (char*)&ubx->data[30]);
	for (int i = 40 ; i < ubx->len ; i += 30) {
	    GNSS_LOG_INFO("UBX[MON_VER]: %s\n", (char*)&ubx->data[i]);
	}
	
	break;
    }

    case GNSS_UBX_MSG_CFG_GNSS: {
	struct gnss_ubx_cfg_gnss *d = (struct gnss_ubx_cfg_gnss *)ubx->data;
	GNSS_LOG_INFO("UBX[CFG_GNSS]: numTrkChHw=%d, numTrkChUse=%d, numConfigBlocks=%d\n", d->numTrkChHw, d->numTrkChUse, d->numConfigBlocks);
	for (int i = 0 ; i < d->numConfigBlocks ; i++) {
	    GNSS_LOG_INFO("UBX[CFG_GNSS]: GNSS=%d, resTrkCh=%d, maxTrkCh=%d, flags=0x%08x\n",

			  d->block[i].gnssId,
			  d->block[i].resTrkCh,
			  d->block[i].maxTrkCh,
			  d->block[i].flags);
	}
	break;
    }
    }
}
