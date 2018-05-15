/*!
 * \file      LoRaMac.c
 *
 * \brief     LoRa MAC layer implementation
 *
 * \copyright Revised BSD License, see section \ref LICENSE.
 *
 * \code
 *                ______                              _
 *               / _____)             _              | |
 *              ( (____  _____ ____ _| |_ _____  ____| |__
 *               \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 *               _____) ) ____| | | || |_| ____( (___| | | |
 *              (______/|_____)_|_|_| \__)_____)\____)_| |_|
 *              (C)2013-2017 Semtech
 *
 *               ___ _____ _   ___ _  _____ ___  ___  ___ ___
 *              / __|_   _/_\ / __| |/ / __/ _ \| _ \/ __| __|
 *              \__ \ | |/ _ \ (__| ' <| _| (_) |   / (__| _|
 *              |___/ |_/_/ \_\___|_|\_\_| \___/|_|_\\___|___|
 *              embedded.connectivity.solutions===============
 *
 * \endcode
 *
 * \author    Miguel Luis ( Semtech )
 *
 * \author    Gregory Cristian ( Semtech )
 *
 * \author    Daniel Jaeckle ( STACKFORCE )
 */

#include <string.h>
#include <assert.h>
#include "os/mynewt.h"
#include "node/lora.h"
#include "node/utilities.h"
#include "node/mac/LoRaMacCrypto.h"
#include "node/mac/LoRaMac.h"
#include "node/mac/LoRaMacTest.h"
#include "hal/hal_timer.h"
#include "node/lora_priv.h"

#if MYNEWT_VAL(LORA_MAC_TIMER_NUM) == -1
#error "Must define a Lora MAC timer number"
#else
#define LORA_MAC_TIMER_NUM    MYNEWT_VAL(LORA_MAC_TIMER_NUM)
#endif

/* The lora mac timer counts in 1 usec increments */
#define LORA_MAC_TIMER_FREQ     1000000

/*!
 * Maximum PHY layer payload size
 */
#define LORAMAC_PHY_MAXPAYLOAD                      255

/*!
 * Maximum MAC commands buffer size
 */
#define LORA_MAC_CMD_BUF_LEN                        128

/*!
 * Maximum length of the fOpts field
 */
#define LORA_MAC_COMMAND_MAX_FOPTS_LENGTH           15

/*!
 * LoRaMac region.
 */
static LoRaMacRegion_t LoRaMacRegion;

/*!
 * Device IEEE EUI
 */
static uint8_t *LoRaMacDevEui;

/*!
 * Application IEEE EUI
 */
static uint8_t *LoRaMacAppEui;

/*!
 * AES encryption/decryption cipher application key
 */
static uint8_t *LoRaMacAppKey;

/*!
 * AES encryption/decryption cipher network session key
 */
static uint8_t LoRaMacNwkSKey[16];

/*!
 * AES encryption/decryption cipher application session key
 */
static uint8_t LoRaMacAppSKey[16];

/*!
 * Multicast channels linked list
 */
static MulticastParams_t *MulticastChannels = NULL;

/*!
 * Actual device class
 */
static DeviceClass_t LoRaMacDeviceClass;

/*!
 * Buffer containing the data to be sent or received.
 */
static uint8_t LoRaMacBuffer[LORAMAC_PHY_MAXPAYLOAD];

/*!
 * Length of packet in LoRaMacBuffer
 */
static uint16_t LoRaMacBufferPktLen;

/*!
 * Buffer containing the upper layer data.
 */
static uint8_t LoRaMacRxPayload[LORAMAC_PHY_MAXPAYLOAD];

/*!
 * IsPacketCounterFixed enables the MIC field tests by fixing the
 * UpLinkCounter value
 */
static bool IsUpLinkCounterFixed = false;

/*!
 * LoRaMac ADR control status
 */
static bool AdrCtrlOn = false;

/*!
 * Contains the current MacCommandsBuffer index
 */
static uint8_t MacCommandsBufferIndex;

/*!
 * Contains the current MacCommandsBuffer index for MAC commands to repeat
 */
static uint8_t MacCommandsBufferToRepeatIndex;

/*!
 * Buffer containing the MAC layer commands
 */
static uint8_t MacCommandsBuffer[LORA_MAC_CMD_BUF_LEN];

/*!
 * Buffer containing the MAC layer commands which must be repeated
 */
static uint8_t MacCommandsBufferToRepeat[LORA_MAC_CMD_BUF_LEN];

/*!
 * LoRaMac parameters
 */
LoRaMacParams_t LoRaMacParams;

/*!
 * LoRaMac default parameters
 */
LoRaMacParams_t LoRaMacParamsDefaults;

/*!
 * Enables/Disables duty cycle management (Test only)
 */
static bool DutyCycleOn;

/*!
 * LoRaMac internal states
 */
enum eLoRaMacState
{
    LORAMAC_IDLE          = 0x00000000,
    LORAMAC_TX_RUNNING    = 0x00000001,
    LORAMAC_RX            = 0x00000002,
    LORAMAC_ACK_REQ       = 0x00000004,
    LORAMAC_ACK_RETRY     = 0x00000008,
    LORAMAC_TX_DELAYED    = 0x00000010,
    LORAMAC_TX_CONFIG     = 0x00000020,
    LORAMAC_RX_ABORT      = 0x00000040,
};

/*!
 * LoRaMac internal state
 */
uint32_t LoRaMacState;

/*!
 * LoRaMac upper layer callback functions
 */
static LoRaMacCallback_t *LoRaMacCallbacks;

/*!
 * Radio events function pointer
 */
static RadioEvents_t RadioEvents;

/*!
 * LoRaMac duty cycle delayed Tx timer
 */
static struct hal_timer TxDelayedTimer;

/*!
 * LoRaMac reception windows timers
 */
static struct hal_timer RxWindowTimer1;
static struct hal_timer RxWindowTimer2;

/*!
 * LoRaMac Rx windows configuration
 */
static RxConfigParams_t RxWindow1Config;
static RxConfigParams_t RxWindow2Config;

/* Lengths of MAC commands */
static const uint8_t
g_lora_mac_cmd_lens[LORA_MAC_MAX_MAC_CMD_CID + 1] = {0, 0, 1, 2, 1, 2, 3, 2, 1};

/* Radio events */
struct os_event g_lora_mac_radio_tx_timeout_event;
struct os_event g_lora_mac_radio_tx_event;
struct os_event g_lora_mac_radio_rx_event;
struct os_event g_lora_mac_radio_rx_err_event;
struct os_event g_lora_mac_radio_rx_timeout_event;
struct os_event g_lora_mac_rtx_timeout_event;
struct os_event g_lora_mac_rx_win1_event;
struct os_event g_lora_mac_rx_win2_event;
struct os_event g_lora_mac_tx_delay_timeout_event;

static void lora_mac_rx_on_window2(void);
static uint8_t lora_mac_extract_mac_cmds(uint8_t max_cmd_bytes, uint8_t *buf);

/**
 * lora mac rtx timer stop
 *
 * Stops the retransmission timer. Removes any enqueued event (if on queue).
 */
static void
lora_mac_rtx_timer_stop(void)
{
    hal_timer_stop(&g_lora_mac_data.rtx_timer);
    os_eventq_remove(lora_node_mac_evq_get(), &g_lora_mac_rtx_timeout_event);
}

/**
 * lora mac rx win2 stop
 *
 * Stops the receive window timer. Removes any enqueued event (if on queue)
 *
 */
static void
lora_mac_rx_win2_stop(void)
{
    if (LoRaMacDeviceClass == CLASS_A) {
        hal_timer_stop(&RxWindowTimer2);
        os_eventq_remove(lora_node_mac_evq_get(), &g_lora_mac_rx_win2_event);
    }
}

/*!
 * \brief Function to be executed on Radio Tx Done event
 */
static void
OnRadioTxDone(void)
{
    os_eventq_put(lora_node_mac_evq_get(), &g_lora_mac_radio_tx_event);
}

/**
 * Processes radio received done interrupt
 *
 * Posts received packet event to MAC task for processing.
 *
 * Context: ISR
 *
 */
static void
OnRadioRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr)
{
    os_eventq_put(lora_node_mac_evq_get(), &g_lora_mac_radio_rx_event);

    /*
      * TODO: for class C devices we may need to handle this differently as
      * the device is continuously listening I believe and it may be possible
      * to get a receive done event before the previous one has been handled.
      */
    /* The ISR fills out the payload pointer, size, rssi and snr of rx pdu */
    g_lora_mac_data.rxpkt.rxdinfo.rssi = rssi;
    g_lora_mac_data.rxpkt.rxdinfo.snr = snr;
    g_lora_mac_data.rxbuf = payload;
    g_lora_mac_data.rxbufsize = size;
}

/**
 * Radio transmit timeout event
 *
 * Context: ISR
 */
static void
OnRadioTxTimeout(void)
{
    os_eventq_put(lora_node_mac_evq_get(), &g_lora_mac_radio_tx_timeout_event);
}

/*!
 * \brief Function executed on Radio Rx error event
 */
static void
OnRadioRxError(void)
{
    os_eventq_put(lora_node_mac_evq_get(), &g_lora_mac_radio_rx_err_event);
}

/*!
 * \brief Function executed on Radio Rx Timeout event
 */
static void
OnRadioRxTimeout(void)
{
    os_eventq_put(lora_node_mac_evq_get(), &g_lora_mac_radio_rx_timeout_event);
}

/*!
 * \brief Function executed on duty cycle delayed Tx  timer event
 */
static void
OnTxDelayedTimerEvent(void *unused)
{
    os_eventq_put(lora_node_mac_evq_get(), &g_lora_mac_tx_delay_timeout_event);
}

/*!
 * \brief Function executed on first Rx window timer event
 */
static void
OnRxWindow1TimerEvent(void *unused)
{
    os_eventq_put(lora_node_mac_evq_get(), &g_lora_mac_rx_win1_event);
}

/*!
 * \brief Function executed on second Rx window timer event
 */
static void
OnRxWindow2TimerEvent(void *unused)
{
    os_eventq_put(lora_node_mac_evq_get(), &g_lora_mac_rx_win2_event);
}

/*!
 * \brief Function executed when rtx timer expires. The rtx timer is used
 * for packet re-transmissions.
 *
 * Context: Interrupt and MAC
 */
static void
lora_mac_rtx_timer_cb(void *unused)
{
    os_eventq_put(lora_node_mac_evq_get(), &g_lora_mac_rtx_timeout_event);
}

/*!
 * \brief Initializes and opens the reception window
 *
 * \param [IN] rxContinuous Set to true, if the RX is in continuous mode
 * \param [IN] maxRxWindow Maximum RX window timeout
 */
static void RxWindowSetup(bool rxContinuous, uint32_t maxRxWindow);

/*!
 * \brief Adds a new MAC command to be sent.
 *
 * \Remark MAC layer internal function
 *
 * \param [in] cmd MAC command to be added
 *                 [MOTE_MAC_LINK_CHECK_REQ,
 *                  MOTE_MAC_LINK_ADR_ANS,
 *                  MOTE_MAC_DUTY_CYCLE_ANS,
 *                  MOTE_MAC_RX2_PARAM_SET_ANS,
 *                  MOTE_MAC_DEV_STATUS_ANS
 *                  MOTE_MAC_NEW_CHANNEL_ANS]
 * \param [in] p1  1st parameter ( optional depends on the command )
 * \param [in] p2  2nd parameter ( optional depends on the command )
 *
 * \retval status  Function status [0: OK, 1: Unknown command, 2: Buffer full]
 */
static LoRaMacStatus_t AddMacCommand( uint8_t cmd, uint8_t p1, uint8_t p2 );

/*!
 * \brief Parses the MAC commands which must be repeated.
 *
 * \Remark MAC layer internal function
 *
 * \param [IN] cmdBufIn  Buffer which stores the MAC commands to send
 * \param [IN] length  Length of the input buffer to parse
 * \param [OUT] cmdBufOut  Buffer which stores the MAC commands which must be
 *                         repeated.
 *
 * \retval Size of the MAC commands to repeat.
 */
static uint8_t ParseMacCommandsToRepeat( uint8_t* cmdBufIn, uint8_t length, uint8_t* cmdBufOut );

/*!
 * \brief Validates if the payload fits into the frame, taking the datarate
 *        into account.
 *
 * \details Refer to chapter 4.3.2 of the LoRaWAN specification, v1.0
 *
 * \param lenN Length of the application payload. The length depends on the
 *             datarate and is region specific
 *
 * \param datarate Current datarate
 *
 * \param fOptsLen Length of the fOpts field
 *
 * \retval [false: payload does not fit into the frame, true: payload fits into
 *          the frame]
 */
static bool ValidatePayloadLength(uint8_t lenN, int8_t datarate, uint8_t fOptsLen);

/*!
 * \brief Decodes MAC commands in the fOpts field and in the payload
 */
static void ProcessMacCommands(uint8_t *payload, uint8_t macIndex, uint8_t commandsSize, uint8_t snr);

/*!
 * \brief LoRaMAC layer generic send frame
 *
 * \param [IN] macHdr      MAC header field
 * \param [IN] fPort       MAC payload port
 * \param [IN] m           mbuf containing MAC payload
 * \retval status          Status of the operation.
 */

LoRaMacStatus_t Send(LoRaMacHeader_t *macHdr, uint8_t fPort, struct os_mbuf *m);

/*!
 * \brief LoRaMAC layer frame buffer initialization
 *
 * \param [IN] macHdr      MAC header field
 * \param [IN] fCtrl       MAC frame control field
 * \param [IN] fOpts       MAC commands buffer
 * \param [IN] fPort       MAC payload port
 * \param [IN] om          mbuf with MAC payload
 * \retval status          Status of the operation.
 */
LoRaMacStatus_t PrepareFrame(LoRaMacHeader_t *macHdr, LoRaMacFrameCtrl_t *fCtrl,
                             uint8_t fPort, struct os_mbuf *om);

/*
 * \brief Schedules the frame according to the duty cycle
 *
 * \retval Status of the operation
 */
static LoRaMacStatus_t ScheduleTx(void);

/*
 * \brief Calculates the back-off time for the band of a channel.
 *
 * \param [IN] channel     The last Tx channel index
 */
static void CalculateBackOff( uint8_t channel );

/*!
 * \brief LoRaMAC layer prepared frame buffer transmission with channel specification
 *
 * \remark PrepareFrame must be called at least once before calling this
 *         function.
 *
 * \param [IN] channel     Channel to transmit on
 * \retval status          Status of the operation.
 */
LoRaMacStatus_t SendFrameOnChannel( uint8_t channel );

/*!
 * \brief Sets the radio in continuous transmission mode
 *
 * \remark Uses the radio parameters set on the previous transmission.
 *
 * \param [IN] timeout     Time in seconds while the radio is kept in continuous wave mode
 * \retval status          Status of the operation.
 */
LoRaMacStatus_t SetTxContinuousWave(uint16_t timeout);

/*!
 * \brief Sets the radio in continuous transmission mode
 *
 * \remark Uses the radio parameters set on the previous transmission.
 *
 * \param [IN] timeout     Time in seconds while the radio is kept in continuous wave mode
 * \param [IN] frequency   RF frequency to be set.
 * \param [IN] power       RF output power to be set.
 * \retval status          Status of the operation.
 */
LoRaMacStatus_t SetTxContinuousWave1( uint16_t timeout, uint32_t frequency, uint8_t power );

/*
 * XXX: TODO
 *
 * Need to understand how to handle mac commands that are waiting to be
 * transmitted. We do not want to constantly transmit them but if we dont
 * get a downlink packet for commands that we need to repeat until we hear
 * a downlink.
 */

/**
 * Called to send MCPS confirmations
 */
static void
lora_mac_send_mcps_confirm(LoRaMacEventInfoStatus_t status)
{
    /* We are no longer running a tx service */
    LoRaMacState &= ~LORAMAC_TX_RUNNING;

    if (LM_F_IS_MCPS_REQ()) {
        assert(g_lora_mac_data.curtx != NULL);
        g_lora_mac_data.curtx->status = status;
        if (g_lora_mac_data.cur_tx_mbuf) {
            lora_app_mcps_confirm(g_lora_mac_data.cur_tx_mbuf);
        }
        LM_F_IS_MCPS_REQ() = 0;
    }
}

/**
 * Called to send join confirmations
 */
static void
lora_mac_send_join_confirm(LoRaMacEventInfoStatus_t status, uint8_t attempts)
{
    LoRaMacState &= ~LORAMAC_TX_RUNNING;
    lora_app_join_confirm(status, attempts);
    LM_F_IS_JOINING() = 0;
}

static void
lora_mac_join_accept_rxd(uint8_t *payload, uint16_t size)
{
    uint32_t temp;
    uint32_t mic;
    uint32_t micRx;
    ApplyCFListParams_t apply_cf_list;

    STATS_INC(lora_mac_stats, join_accept_rx);

    if (LM_F_IS_JOINED()) {
        STATS_INC(lora_mac_stats, already_joined);
        return;
    }

    /*
     * XXX: This is odd, but if we receive a join accept and we are not
     * joined but have not started the join process not sure what to
     * do. Guess we will just ignore this packet.
     */
    if (!LM_F_IS_JOINING()) {
        return;
    }

    /* XXX: check for too small frame! */

    LoRaMacJoinDecrypt(payload + 1, size - 1, LoRaMacAppKey,
                       LoRaMacRxPayload + 1);

    LoRaMacRxPayload[0] = payload[0];

    LoRaMacJoinComputeMic(LoRaMacRxPayload, size - LORAMAC_MFR_LEN,
                          LoRaMacAppKey, &mic);

    micRx = ( uint32_t )LoRaMacRxPayload[size - LORAMAC_MFR_LEN];
    micRx |= ( ( uint32_t )LoRaMacRxPayload[size - LORAMAC_MFR_LEN + 1] << 8 );
    micRx |= ( ( uint32_t )LoRaMacRxPayload[size - LORAMAC_MFR_LEN + 2] << 16 );
    micRx |= ( ( uint32_t )LoRaMacRxPayload[size - LORAMAC_MFR_LEN + 3] << 24 );

    if (micRx == mic) {
        LoRaMacJoinComputeSKeys(LoRaMacAppKey, LoRaMacRxPayload + 1,
                                g_lora_mac_data.dev_nonce, LoRaMacNwkSKey,
                                LoRaMacAppSKey);

        temp = ( uint32_t )LoRaMacRxPayload[4];
        temp |= ( ( uint32_t )LoRaMacRxPayload[5] << 8 );
        temp |= ( ( uint32_t )LoRaMacRxPayload[6] << 16 );
        g_lora_mac_data.netid = temp;

        temp = ( uint32_t )LoRaMacRxPayload[7];
        temp |= ( ( uint32_t )LoRaMacRxPayload[8] << 8 );
        temp |= ( ( uint32_t )LoRaMacRxPayload[9] << 16 );
        temp |= ( ( uint32_t )LoRaMacRxPayload[10] << 24 );
        g_lora_mac_data.dev_addr = temp;

        // DLSettings
        LoRaMacParams.Rx1DrOffset = ( LoRaMacRxPayload[11] >> 4 ) & 0x07;
        LoRaMacParams.Rx2Channel.Datarate = LoRaMacRxPayload[11] & 0x0F;

        // RxDelay
        LoRaMacParams.ReceiveDelay1 = ( LoRaMacRxPayload[12] & 0x0F );
        if (LoRaMacParams.ReceiveDelay1 == 0) {
            LoRaMacParams.ReceiveDelay1 = 1;
        }
        LoRaMacParams.ReceiveDelay1 *= 1000;
        LoRaMacParams.ReceiveDelay2 = LoRaMacParams.ReceiveDelay1 + 1000;

        // Apply CF list
        apply_cf_list.Payload = &LoRaMacRxPayload[13];

        // Size of the regular payload is 12. Plus 1 byte MHDR and 4 bytes MIC
        apply_cf_list.Size = size - 17;

        RegionApplyCFList(LoRaMacRegion, &apply_cf_list);

        /* We are now joined */
        STATS_INC(lora_mac_stats, joins);

        /* Stop window 2 if class A device */
        lora_mac_rx_win2_stop();

        LM_F_IS_JOINED() = 1;
        g_lora_mac_data.uplink_cntr = 0;
        g_lora_mac_data.nb_rep_cntr = 0;

        /* XXX: why not increment this when sending it? Now
           it is done on both success and fail */
        ++g_lora_mac_data.cur_join_attempt;
        lora_mac_send_join_confirm(LORAMAC_EVENT_INFO_STATUS_OK,
                                   g_lora_mac_data.cur_join_attempt);
    } else {
        STATS_INC(lora_mac_stats, rx_mic_failures);
    }
}

static void
lora_mac_confirmed_tx_fail(struct lora_pkt_info *txi)
{
    GetPhyParams_t getPhy;
    PhyParam_t phyParam;
    LoRaMacStatus_t rc;
    LoRaMacEventInfoStatus_t status;

    STATS_INC(lora_mac_stats, confirmed_tx_fail);

    /* XXX: how does this work in conjunction with ADR? Will the server
     * tell me to use a higher data rate if we fall back erroneously?
     * Need to understand. This is a bad retry mechanism
     */
    if (g_lora_mac_data.ack_timeout_retries_cntr < g_lora_mac_data.ack_timeout_retries) {
        g_lora_mac_data.ack_timeout_retries_cntr++;
        if ((g_lora_mac_data.ack_timeout_retries_cntr % 2) == 1) {
            getPhy.Attribute = PHY_NEXT_LOWER_TX_DR;
            getPhy.UplinkDwellTime = LoRaMacParams.UplinkDwellTime;
            getPhy.Datarate = LoRaMacParams.ChannelsDatarate;
            phyParam = RegionGetPhyParam( LoRaMacRegion, &getPhy );
            LoRaMacParams.ChannelsDatarate = phyParam.Value;
        }

        /* Attempt to transmit */
        rc = ScheduleTx();
        if (rc != LORAMAC_STATUS_OK) {
            // The DR is not applicable for the payload size
            LM_F_NODE_ACK_REQ() = 0;
            txi->txdinfo.retries = g_lora_mac_data.ack_timeout_retries_cntr;
            txi->txdinfo.datarate = LoRaMacParams.ChannelsDatarate;
            g_lora_mac_data.uplink_cntr++;
            if (rc == LORAMAC_STATUS_LENGTH_ERROR) {
                status = LORAMAC_EVENT_INFO_STATUS_TX_DR_PAYLOAD_SIZE_ERROR;
            } else {
                status = LORAMAC_EVENT_INFO_STATUS_ERROR;
            }
            lora_mac_send_mcps_confirm(status);
        }
    } else {
        /* XXX: This seems to be a bit suspect. Why is this done after a
           confirmed transmission failure? */
        RegionInitDefaults(LoRaMacRegion, INIT_TYPE_RESTORE);
        g_lora_mac_data.uplink_cntr++;
        LM_F_NODE_ACK_REQ() = 0;
        txi->txdinfo.retries = g_lora_mac_data.ack_timeout_retries_cntr;
        lora_mac_send_mcps_confirm(LORAMAC_EVENT_INFO_STATUS_TX_RETRIES_EXCEEDED);
    }
}

static void
lora_mac_confirmed_tx_success(struct lora_pkt_info *txi)
{
    STATS_INC(lora_mac_stats, confirmed_tx_good);
    g_lora_mac_data.uplink_cntr++;
    LM_F_NODE_ACK_REQ() = 0;
    txi->txdinfo.ack_rxd = true;
    txi->txdinfo.retries = g_lora_mac_data.ack_timeout_retries_cntr;
    lora_mac_send_mcps_confirm(LORAMAC_EVENT_INFO_STATUS_OK);
}

static void
lora_mac_join_req_tx_fail(void)
{
    /* Add to Join Request trials if not joined */
    ++g_lora_mac_data.cur_join_attempt;

    /* Have we exceeded the number of join request attempts */
    if (g_lora_mac_data.cur_join_attempt >= g_lora_mac_data.max_join_attempt) {
        /* Join was a failure */
        STATS_INC(lora_mac_stats, join_failures);
        lora_mac_send_join_confirm(LORAMAC_EVENT_INFO_STATUS_JOIN_FAIL,
                                   g_lora_mac_data.cur_join_attempt);
    } else {
        /* XXX: see if we want to do this. Not sure it is needed. I added
           this but probably should be modified */
        /* Add some transmit delay between join request transmissions */
        LoRaMacState |= LORAMAC_TX_DELAYED;
        hal_timer_stop(&TxDelayedTimer);
        hal_timer_start(&TxDelayedTimer,
            randr(0, MYNEWT_VAL(LORA_JOIN_REQ_RAND_DELAY) * 1000));
    }
}

static void
lora_mac_unconfirmed_tx_done(struct lora_pkt_info *txi, int stop_tx)
{
    STATS_INC(lora_mac_stats, unconfirmed_tx);

    /*
     * XXX: this might be wrong, but if we cannot schedule the transmission
     * we will just hand up the frame even though we have not attempted
     * it the requisite number of times
     */

    /* Unconfirmed frames get repeated N times. */
    if (stop_tx ||
        (g_lora_mac_data.nb_rep_cntr >= LoRaMacParams.ChannelsNbRep) ||
        (ScheduleTx() != LORAMAC_STATUS_OK)) {
        txi->txdinfo.retries = g_lora_mac_data.nb_rep_cntr;
        g_lora_mac_data.nb_rep_cntr = 0;
        g_lora_mac_data.adr_ack_cntr++;
        g_lora_mac_data.uplink_cntr++;
        lora_mac_send_mcps_confirm(LORAMAC_EVENT_INFO_STATUS_OK);
    }
}

/**
 * lora mac tx service done
 *
 * The term "tx service" is a made-up term (not in the specification). A tx
 * service starts when a frame is transmitted and ends when another frame can
 * be sent (not including duty-cycle limitations).
 *
 * @param rxd_confirmation A boolean flag denoting the following:
 *  -> A confirmed frame was sent and an acknowledgement was received
 *  -> An unconfirmed frame was sent and a frame was received in either window
 *  if the device is a class A device or a frame was received in window 1 if
 *  class C.
 */
static void
lora_mac_tx_service_done(int rxd_confirmation)
{
    struct lora_pkt_info *txi;

    /* A sanity check to make sure things are in the proper state */
    if (!LM_F_IS_JOINING() && !LM_F_IS_MCPS_REQ()) {
        assert((LoRaMacState & LORAMAC_TX_RUNNING) == 0);
        goto chk_txq;
    }

    if (LM_F_IS_JOINING()) {
        lora_mac_join_req_tx_fail();
    } else {
        txi = g_lora_mac_data.curtx;
        assert(txi != NULL);
        if (LM_F_NODE_ACK_REQ()) {
            if (rxd_confirmation) {
                lora_mac_confirmed_tx_success(txi);
            } else {
                lora_mac_confirmed_tx_fail(txi);
            }
        } else {
            lora_mac_unconfirmed_tx_done(txi, rxd_confirmation);
        }
    }

    /* For now, always post an event to check the transmit queue for activity */
chk_txq:
    lora_node_chk_txq();
}

static void
lora_mac_process_radio_tx(struct os_event *ev)
{
    uint32_t timeout;
    GetPhyParams_t getPhy;
    PhyParam_t phyParam;
    SetBandTxDoneParams_t txDone;

    /* XXX: We need to time this more accurately */
    uint32_t curTime = hal_timer_read(LORA_MAC_TIMER_NUM);

    if (LoRaMacDeviceClass != CLASS_C) {
        Radio.Sleep( );
    } else {
        lora_mac_rx_on_window2();
    }

    /* Always start receive window 1 */
    hal_timer_start_at(&RxWindowTimer1,
                       curTime + (g_lora_mac_data.rx_win1_delay * 1000));

    /* Only start receive window 2 if not a class C device */
    if (LoRaMacDeviceClass != CLASS_C) {
        hal_timer_start_at(&RxWindowTimer2,
                           curTime + (g_lora_mac_data.rx_win2_delay * 1000));
    }

    /* Set flag if tx is a join request and increment tx request stats. */
    if (LM_F_IS_JOINING()) {
        STATS_INC(lora_mac_stats, join_req_tx);
        LM_F_LAST_TX_IS_JOIN_REQ() = 1;
    } else {
        LM_F_LAST_TX_IS_JOIN_REQ() = 0;
    }

    if (LM_F_NODE_ACK_REQ()) {
        getPhy.Attribute = PHY_ACK_TIMEOUT;
        phyParam = RegionGetPhyParam(LoRaMacRegion, &getPhy);
        hal_timer_start_at(&g_lora_mac_data.rtx_timer, curTime +
                           ((g_lora_mac_data.rx_win2_delay + phyParam.Value) * 1000));
    } else {
        /*
         * For unconfirmed transmisson for class C devices,
         * we want to make sure we listen for the second rx
         * window before moving on to another transmission.
         */
        if (LoRaMacDeviceClass == CLASS_C) {
            timeout = (g_lora_mac_data.rx_win2_delay * 1000);
            /* XXX: work-around for now. Must insure that we give enough
             * time to hear a complete join accept in window 2 so we add
             * 2 seconds here
             */
            if (LM_F_LAST_TX_IS_JOIN_REQ()) {
                timeout += (2000 * 1000);
            } else {
                timeout += (uint32_t)((RxWindow2Config.tsymbol * 1000) *
                    RxWindow2Config.WindowTimeout);
            }
            hal_timer_start_at(&g_lora_mac_data.rtx_timer, curTime + timeout);
        }
    }

    // Store last Tx channel
    g_lora_mac_data.last_tx_chan = g_lora_mac_data.cur_chan;

    // Update last tx done time for the current channel
    txDone.Channel = g_lora_mac_data.cur_chan;
    txDone.Joined = LM_F_IS_JOINED();
    txDone.LastTxDoneTime = curTime;
    RegionSetBandTxDone(LoRaMacRegion, &txDone);

    // Update Aggregated last tx done time
    g_lora_mac_data.aggr_last_tx_done_time = curTime;

    if (!LM_F_NODE_ACK_REQ()) {
        g_lora_mac_data.nb_rep_cntr++;
    }

    lora_node_log(LORA_NODE_LOG_TX_DONE, g_lora_mac_data.cur_chan,
                  LoRaMacBufferPktLen, curTime);
}

/**
 * Process the radio receive event.
 *
 * Context: MAC task
 *
 * @param ev
 */
static void
lora_mac_process_radio_rx(struct os_event *ev)
{
    LoRaMacHeader_t macHdr;
    LoRaMacFrameCtrl_t fCtrl;
    LoRaMacRxSlot_t entry_rx_slot;
    struct lora_pkt_info *rxi;
    bool skipIndication = false;
    bool send_indicate = false;
    GetPhyParams_t getPhy;
    PhyParam_t phyParam;
    uint8_t *payload;
    uint16_t size;
    int8_t snr;
    uint8_t hdrlen;
    uint32_t address = 0;
    uint8_t appPayloadStartIndex = 0;
    uint8_t port = 0xFF;
    uint8_t frameLen;
    uint32_t mic = 0;
    uint32_t micRx;

    uint16_t sequenceCounter = 0;
    uint16_t sequenceCounterPrev = 0;
    uint16_t sequenceCounterDiff = 0;
    uint32_t downLinkCounter = 0;

    MulticastParams_t *curMulticastParams = NULL;
    uint8_t *nwkSKey = LoRaMacNwkSKey;
    uint8_t *appSKey = LoRaMacAppSKey;

    uint8_t multicast = 0;

    /*
     * XXX: what if window 2 timeout event already enqueued? If we
     * receive a frame with a valid MIC we are supposed to not receive
     * on window 2. We stop the timer below but what if event has been
     * processed?
     */

    /* Put radio to sleep if not class C */
    if (LoRaMacDeviceClass != CLASS_C) {
        Radio.Sleep( );
    }

    STATS_INC(lora_mac_stats, rx_frames);

    /* Payload, size and snr are filled in by radio rx ISR */
    payload = g_lora_mac_data.rxbuf;
    size = g_lora_mac_data.rxbufsize;

    rxi = &g_lora_mac_data.rxpkt;
    snr = rxi->rxdinfo.snr;

    /* Reset rest of global indication element */
    rxi->port = 0;
    entry_rx_slot = g_lora_mac_data.rx_slot;
    rxi->rxdinfo.rxslot = entry_rx_slot;
    rxi->rxdinfo.multicast = 0;
    rxi->rxdinfo.frame_pending = 0;
    rxi->rxdinfo.rxdata = false;
    rxi->rxdinfo.ack_rxd = false;
    rxi->rxdinfo.downlink_cntr = 0;

    /* Get the MHDR from the received frame */
    macHdr.Value = payload[0];
    hdrlen = 1;

    lora_node_log(LORA_NODE_LOG_RX_DONE, g_lora_mac_data.cur_chan, size,
                  (entry_rx_slot << 8) | macHdr.Value);

    switch (macHdr.Bits.MType) {
        case FRAME_TYPE_JOIN_ACCEPT:
            lora_mac_join_accept_rxd(payload, size);
            break;
        case FRAME_TYPE_DATA_CONFIRMED_DOWN:
        case FRAME_TYPE_DATA_UNCONFIRMED_DOWN:
            /* If not joined I do not know why we would accept a frame */
            if (!LM_F_IS_JOINED()) {
                goto process_rx_done;
            }

            address = payload[hdrlen++];
            address |= ((uint32_t)payload[hdrlen++] << 8);
            address |= ((uint32_t)payload[hdrlen++] << 16);
            address |= ((uint32_t)payload[hdrlen++] << 24) ;

            if (address != g_lora_mac_data.dev_addr) {
                curMulticastParams = MulticastChannels;
                while (curMulticastParams != NULL) {
                    if (address == curMulticastParams->Address) {
                        multicast = 1;
                        nwkSKey = curMulticastParams->NwkSKey;
                        appSKey = curMulticastParams->AppSKey;
                        downLinkCounter = curMulticastParams->DownLinkCounter;
                        break;
                    }
                    curMulticastParams = curMulticastParams->Next;
                }

                if (multicast == 0) {
                    /* XXX: stat */
                    /* We are not the destination of this frame. */
                    goto process_rx_done;
                }
            } else {
                multicast = 0;
                nwkSKey = LoRaMacNwkSKey;
                appSKey = LoRaMacAppSKey;
                downLinkCounter = g_lora_mac_data.downlink_cntr;
            }

            lora_node_qual_sample(rxi->rxdinfo.rssi, snr);
            fCtrl.Value = payload[hdrlen++];

            sequenceCounter = ( uint16_t )payload[hdrlen++];
            sequenceCounter |= ( uint16_t )payload[hdrlen++] << 8;

            appPayloadStartIndex = 8 + fCtrl.Bits.FOptsLen;

            micRx = ( uint32_t )payload[size - LORAMAC_MFR_LEN];
            micRx |= ( ( uint32_t )payload[size - LORAMAC_MFR_LEN + 1] << 8 );
            micRx |= ( ( uint32_t )payload[size - LORAMAC_MFR_LEN + 2] << 16 );
            micRx |= ( ( uint32_t )payload[size - LORAMAC_MFR_LEN + 3] << 24 );

            sequenceCounterPrev = (uint16_t)downLinkCounter;
            sequenceCounterDiff = (sequenceCounter - sequenceCounterPrev);

            /* Check for correct MIC */
            downLinkCounter += sequenceCounterDiff;
            LoRaMacComputeMic(payload, size - LORAMAC_MFR_LEN, nwkSKey, address,
                              DOWN_LINK, downLinkCounter, &mic);
            if (micRx == mic) {
                /* XXX: inc mic good stat */
                rxi->status = LORAMAC_EVENT_INFO_STATUS_OK;
                rxi->rxdinfo.multicast = multicast;
                rxi->rxdinfo.frame_pending = fCtrl.Bits.FPending;
                rxi->rxdinfo.downlink_cntr = downLinkCounter;

                /*
                 * Per spec v1.1: a device MUST not open the second receive if
                 * it receives a frame destined to it and it passes the MIC in
                 * the first receive window.
                 *
                 * NOTE: only class A devices use the window 2 timer. If the
                 * frame was receiving during the second window stopping the
                 * timer has no effect so that is why we do not check for
                 * receiving this in window 1
                 */
                lora_mac_rx_win2_stop();
            } else {
                STATS_INC(lora_mac_stats, rx_mic_failures);
                goto process_rx_done;
            }

            /* Check for a the maximum allowed counter difference */
            getPhy.Attribute = PHY_MAX_FCNT_GAP;
            phyParam = RegionGetPhyParam(LoRaMacRegion, &getPhy);
            if (sequenceCounterDiff >= phyParam.Value) {
                /*
                 * XXX: For now we will hand up the indication with this error
                 * status and let the application handle re-join. Note this in
                 * the documentation!
                 *
                 * XXX: this should be handled differently. Flush txq and
                 * inform upper layer this happened. Indicate is not great.
                 *
                 * NOTE: we wont process an ACK, so confirmed frames would get
                 * re-transmitted here.
                 */
                rxi->status = LORAMAC_EVENT_INFO_STATUS_DOWNLINK_TOO_MANY_FRAMES_LOSS;
                send_indicate = true;
                goto process_rx_done;
            }

            g_lora_mac_data.adr_ack_cntr = 0;
            MacCommandsBufferToRepeatIndex = 0;

            /*
             * XXX: Look into confirmed frames and multicast and the
             * "gw_ack_req" bit. I think the bit should be reset after
             * every valid reception (except possibly multicast case).
             * Need to see if there is a frame already created and if the
             * ACK bit is set in it. If it is and we just received a
             * non-confirmd downlink or multicast the ACK bit going up
             * should be cleared.
             */

            // Update 32 bits downlink counter
            if (multicast == 1) {
                rxi->pkt_type = MCPS_MULTICAST;

                if((curMulticastParams->DownLinkCounter == downLinkCounter ) &&
                    (curMulticastParams->DownLinkCounter != 0)) {
                    /* XXX: I need to understand multicast. Are these sent
                     * in normal rx windows and act the same as normal
                       transmissions? */

                    /* XXX: count stat */
                    rxi->status = LORAMAC_EVENT_INFO_STATUS_DOWNLINK_REPEATED;
                    send_indicate = true;
                    goto process_rx_done;
                }
                curMulticastParams->DownLinkCounter = downLinkCounter;
            } else {
                if (macHdr.Bits.MType == FRAME_TYPE_DATA_CONFIRMED_DOWN) {
                    LM_F_GW_ACK_REQ() = 1;
                    rxi->pkt_type = MCPS_CONFIRMED;

                    /* XXX: why not initialize the DownLinkCounter to
                       all 0xf instead of check for 0 */
                    if ((g_lora_mac_data.downlink_cntr == downLinkCounter) &&
                        (g_lora_mac_data.downlink_cntr != 0) ) {
                        /* Duplicated confirmed downlink. Skip indication.*/
                        STATS_INC(lora_mac_stats, rx_dups);
                        skipIndication = true;
                    }
                } else {
                    LM_F_GW_ACK_REQ() = 0;
                    rxi->pkt_type = MCPS_UNCONFIRMED;

                    /*
                     * XXX: this should be handled differently. Something
                     * akin to what happens if we get a frame counter gap
                     * that is too big.
                     */
                    if ((g_lora_mac_data.downlink_cntr == downLinkCounter ) &&
                        (g_lora_mac_data.downlink_cntr != 0)) {
                        rxi->status = LORAMAC_EVENT_INFO_STATUS_DOWNLINK_REPEATED;

                        /* Duplicated unconfirmed downlink. */
                        STATS_INC(lora_mac_stats, rx_dups);

                        send_indicate = true;
                        goto process_rx_done;
                    }
                }
                g_lora_mac_data.downlink_cntr = downLinkCounter;
            }

            /* XXX: Make sure MacCommands are handled properly */
            // This must be done before parsing the payload and the MAC commands.
            // We need to reset the MacCommandsBufferIndex here, since we need
            // to take retransmissions and repititions into account.
            if ((g_lora_mac_data.curtx != NULL) &&
                (g_lora_mac_data.curtx->pkt_type == MCPS_CONFIRMED)) {
                if (fCtrl.Bits.Ack == 1) {
                    // Reset MacCommandsBufferIndex when we have received an ACK.
                    MacCommandsBufferIndex = 0;
                }
            } else {
                // Reset the variable if we have received any valid frame.
                MacCommandsBufferIndex = 0;
            }

            if (((size - 4) - appPayloadStartIndex) > 0) {
                port = payload[appPayloadStartIndex++];
                frameLen = (size - 4) - appPayloadStartIndex;

                rxi->port = port;

                lora_node_log(LORA_NODE_LOG_RX_PORT, port, frameLen,
                              downLinkCounter);

                /* If port is 0 it means frame has MAC commands in payload */
                if (port == 0) {
                    STATS_INC(lora_mac_stats, rx_mlme);
                    if (fCtrl.Bits.FOptsLen == 0) {
                        LoRaMacPayloadDecrypt( payload + appPayloadStartIndex,
                                               frameLen,
                                               nwkSKey,
                                               address,
                                               DOWN_LINK,
                                               downLinkCounter,
                                               LoRaMacRxPayload );

                        // Decode frame payload MAC commands
                        ProcessMacCommands(LoRaMacRxPayload, 0, frameLen, snr);
                    } else {
                        /* This is an invalid frame. Ignore it */
                        STATS_INC(lora_mac_stats, rx_invalid);
                        goto process_rx_done;
                    }
                } else {
                    STATS_INC(lora_mac_stats, rx_mcps);

                    if (fCtrl.Bits.FOptsLen > 0) {
                        // Decode Options field MAC commands. Omit the fPort.
                        ProcessMacCommands(payload, 8, appPayloadStartIndex - 1,
                                           snr);
                    }

                    LoRaMacPayloadDecrypt( payload + appPayloadStartIndex,
                                           frameLen,
                                           appSKey,
                                           address,
                                           DOWN_LINK,
                                           downLinkCounter,
                                           LoRaMacRxPayload );

                    if (skipIndication == false ) {
                        g_lora_mac_data.rxbuf = LoRaMacRxPayload;
                        g_lora_mac_data.rxbufsize = frameLen;
                        rxi->rxdinfo.rxdata = true;
                        send_indicate = true;
                    }
                }
            } else {
                /*
                 * No application payload. Process any mac commands in the
                 * fopts field. No need to send indicate
                 */
                if (fCtrl.Bits.FOptsLen > 0) {
                    // Decode Options field MAC commands
                    ProcessMacCommands( payload, 8, appPayloadStartIndex, snr );
                }
            }

            /* We received a valid frame. */
            if (LM_F_NODE_ACK_REQ()) {
                if (skipIndication == false) {
                    // Check if the frame is an acknowledgement
                    if (fCtrl.Bits.Ack == 1) {
                        rxi->rxdinfo.ack_rxd = true;

                        /*
                         * We received an ACK. Stop the retransmit timer
                         * and confirm successful transmission.
                         */
                        lora_mac_rtx_timer_stop();
                        lora_mac_tx_service_done(1);
                    }
                }
            } else {
                /*
                 * The specification is not the greatest here. It states
                 * that a frame cannot be retransmitted unless a valid
                 * frame was received in either window 1 or window 2. This
                 * makes sense for Class A but what about class C? They
                 * mean the actual receive window 2 and not receiving on
                 * window 2 parameters (if you know what I mean). Thus,
                 * the check below is if the device is class A or this
                 * was receive window 1 we end the transmission service.
                 */
                if ((LoRaMacDeviceClass == CLASS_A) ||
                    (entry_rx_slot == RX_SLOT_WIN_1)) {

                    /* Stop the retransmit timer as we can now transmit */
                    lora_mac_rtx_timer_stop();

                    /*
                     * The spec is clear in this case: an unconfirmed
                     * frame should not be retransmitted NbTrans # of times
                     * if a valid frame was received in either window 1
                     * or window 2 for class A or in receive window 1 for
                     * class C. Calling the tx service done function with
                     * 1 will make sure no more retransmissions occur.
                     */
                    lora_mac_tx_service_done(1);

                    /*
                     * This check is so that we do not attempt to end
                     * the transmit service in the process rx done case.
                     */
                    if ((LoRaMacDeviceClass == CLASS_A) &&
                        (entry_rx_slot == RX_SLOT_WIN_2)) {
                        goto chk_send_indicate;
                    }
                }
            }
            break;
        case FRAME_TYPE_PROPRIETARY:
            break;
        default:
            break;
    }

process_rx_done:
    if (LoRaMacDeviceClass == CLASS_C) {
        /* If we are not receiving, start receiving on window 2 parameters */
        if (Radio.GetStatus() == RF_IDLE) {
            lora_mac_rx_on_window2();
        }
    } else {
        /*
         * If second receive window and a transmit service is running end it.
         * If a confirmed frame the retransmission timer will handle ending
         * the transmit service. Note that we put the retransmit timeout
         * event on the queue as opposed to calling tx service done so that
         * we do not attempt to retransmit the frame in this call.
         */
        if ((!LM_F_NODE_ACK_REQ() && (entry_rx_slot == RX_SLOT_WIN_2) &&
             (LoRaMacState & LORAMAC_TX_RUNNING))) {
            os_eventq_put(lora_node_mac_evq_get(), &g_lora_mac_rtx_timeout_event);
        }
    }

    /* Send MCPS indication if flag set */
chk_send_indicate:
    if (send_indicate) {
        lora_node_mac_mcps_indicate();
    }

    /*
     * If gw wants an ack, make sure we check on transmit queue to send it if
     * no other frames enqueued
     */
    if (LM_F_GW_ACK_REQ()) {
        lora_node_chk_txq();
    }
}

/**
 * Process a transmit timeout event
 *
 * NOTE: no window timers should have been started at this point.
 *
 * Context: lora  mac task
 *
 * @param ev
 */
static void
lora_mac_process_radio_tx_timeout(struct os_event *ev)
{
    if (LoRaMacDeviceClass != CLASS_C) {
        Radio.Sleep( );
    } else {
        lora_mac_rx_on_window2();
    }
    STATS_INC(lora_mac_stats, tx_timeouts);

    /* XXX: what happens if ack requested? What should we do? */
    /* XXX: Have we set up any window timers or ACK timers? I dont think so */
    lora_mac_tx_service_done(0);
}

static void
lora_mac_process_radio_rx_err(struct os_event *ev)
{
    STATS_INC(lora_mac_stats, rx_errors);

    if (LoRaMacDeviceClass != CLASS_C) {
        Radio.Sleep( );
        if (g_lora_mac_data.rx_slot == RX_SLOT_WIN_2) {
            if (!LM_F_NODE_ACK_REQ()) {
                lora_mac_tx_service_done(0);
            }
        }
    } else {
        /* If we are not receiving start receiving on window 2 */
        if (Radio.GetStatus() == RF_IDLE) {
            lora_mac_rx_on_window2();
        }
    }
}

static void
lora_mac_process_radio_rx_timeout(struct os_event *ev)
{
    lora_node_log(LORA_NODE_LOG_RX_TIMEOUT, g_lora_mac_data.cur_chan,
                  g_lora_mac_data.rx_slot, 0);

    if (LoRaMacDeviceClass != CLASS_C) {
        Radio.Sleep( );
        if (g_lora_mac_data.rx_slot == RX_SLOT_WIN_2) {
            /* Let the ACK retry timer handle confirmed transmissions */
            if (!LM_F_NODE_ACK_REQ()) {
                lora_mac_tx_service_done(0);
            }
        }
    } else {
        /* Rx timeout for class C devices should only occur in rx window 1 */
        assert(g_lora_mac_data.rx_slot == RX_SLOT_WIN_1);
        lora_mac_rx_on_window2();
    }
}

static void
lora_mac_process_tx_delay_timeout(struct os_event *ev)
{
    LoRaMacHeader_t macHdr;
    LoRaMacFrameCtrl_t fCtrl;
    LoRaMacStatus_t rc;

    /*
     * XXX: not sure if we should keep this code the way it is. I am thinking
     * probably better to not call ScheduleTx() here and remove all this
     * code that is basically duplicated for the join process.
     *
     * I guess the main question is this: should the process transmit queue
     * function only be taking a packet off the queue and then starting
     * the whole transmit process (the transmit service I will call it) or
     * should that code also notice that a transmit process is currently
     * happening and deal with that?.
     */
    LoRaMacState &= ~LORAMAC_TX_DELAYED;

    /*
     * It is possible that the delay timer is running but we finished
     * the transmit service. If that is the case, just return
     */
    if (!LM_F_IS_JOINING() && !LM_F_IS_MCPS_REQ()) {
        lora_node_chk_txq();
        return;
    }

    if (LM_F_IS_JOINING()) {
        /* Since this is a join we may need to change datarate */
        LoRaMacParams.ChannelsDatarate =
            RegionAlternateDr(LoRaMacRegion, LoRaMacParams.ChannelsDatarate);

        macHdr.Value = 0;
        macHdr.Bits.MType = FRAME_TYPE_JOIN_REQ;

        fCtrl.Value = 0;
        fCtrl.Bits.Adr = AdrCtrlOn;

        /* In case of join request retransmissions, the stack must prepare
         * the frame again, because the network server keeps track of the random
         * LoRaMacDevNonce values to prevent reply attacks. */
        PrepareFrame(&macHdr, &fCtrl, 0, NULL);
    }

    rc = ScheduleTx();
    if (rc != LORAMAC_STATUS_OK) {
        lora_mac_tx_service_done(0);
    }
}

static void
lora_mac_process_rx_win1_timeout(struct os_event *ev)
{
    g_lora_mac_data.rx_slot = RX_SLOT_WIN_1;

    RxWindow1Config.Channel = g_lora_mac_data.cur_chan;
    RxWindow1Config.DrOffset = LoRaMacParams.Rx1DrOffset;
    RxWindow1Config.DownlinkDwellTime = LoRaMacParams.DownlinkDwellTime;
    RxWindow1Config.RepeaterSupport = LM_F_REPEATER_SUPP();
    RxWindow1Config.RxContinuous = false;
    RxWindow1Config.RxSlot = RX_SLOT_WIN_1;

    if (LoRaMacDeviceClass == CLASS_C) {
        Radio.Standby( );
    }

    lora_node_log(LORA_NODE_LOG_RX_WIN1_SETUP, RxWindow1Config.Datarate,
                  g_lora_mac_data.cur_chan, RxWindow1Config.WindowTimeout);

    /* XXX: check error codes! */
    RegionRxConfig(LoRaMacRegion, &RxWindow1Config,
                   (int8_t *)&g_lora_mac_data.rxpkt.rxdinfo.rxdatarate);

    RxWindowSetup(RxWindow1Config.RxContinuous, LoRaMacParams.MaxRxWindow);
}

/**
 * lora mac rx on window 2
 *
 * Called to receive on Rx Window 2 parameters. When this function is called
 * it is expected that the radio is not currently receiving on window 2
 * parameters
 */
void
lora_mac_rx_on_window2(void)
{
    bool rc;
    bool rx_continuous;
    uint8_t rx_slot;

    if (LoRaMacDeviceClass == CLASS_C) {
        rx_slot = RX_SLOT_WIN_CLASS_C;
        rx_continuous = 1;
    } else {
        rx_slot = RX_SLOT_WIN_2;
        rx_continuous = 0;
    }

    lora_node_log(LORA_NODE_LOG_RX_WIN2, rx_slot, 0,
                  LoRaMacParams.Rx2Channel.Frequency);

    g_lora_mac_data.rx_slot = rx_slot;

    RxWindow2Config.Channel = g_lora_mac_data.cur_chan;
    RxWindow2Config.Frequency = LoRaMacParams.Rx2Channel.Frequency;
    RxWindow2Config.DownlinkDwellTime = LoRaMacParams.DownlinkDwellTime;
    RxWindow2Config.RepeaterSupport = LM_F_REPEATER_SUPP();
    RxWindow2Config.RxSlot = RX_SLOT_WIN_2;
    RxWindow2Config.RxContinuous = rx_continuous;

    /*
     * NOTE: this should never fail since we made sure radio was idle and
     * this is the only way this should fail!
     */
    rc = RegionRxConfig(LoRaMacRegion, &RxWindow2Config, (int8_t *)
                        &g_lora_mac_data.rxpkt.rxdinfo.rxdatarate);
    assert(rc);

    RxWindowSetup(RxWindow2Config.RxContinuous, LoRaMacParams.MaxRxWindow);
}

/**
 * Receive Window 2 timeout
 *
 * Called when the receive window 2 timer expires. This timer is only started
 * for class A devices.
 *
 * Context: MAC task
 *
 * XXX: I think we may want to modify the code such that window 2 timer only
 * starts if there is a timeout for window 1. Would make things much easier
 * I think (simpler).
 *
 * @param unused
 */
static void
lora_mac_process_rx_win2_timeout(struct os_event *ev)
{
    /*
     * There are two cases here. Either the radio is still receiving in which
     * case the radio status is not idle, or the radio finished receiving
     * a frame and posted an event but there was a race condition and this
     * timer fired off just before the packet was finished. In the latter case,
     * a receive done event will be enqueued but not processed.
     */
    if ((Radio.GetStatus() == RF_IDLE) &&
        (g_lora_mac_radio_rx_event.ev_queued == 0)) {
        lora_mac_rx_on_window2();
    }
}

/**
 * lora mac process rtx timeout
 *
 * Called when the retransmit timer expires. This timer is set under the
 * following conditions:
 *  1) The previous frame sent was a confirmed frame.
 *  2) The device is class C and an unconfirmed frame was sent.
 *  3) The device is class A and the second receive window ended and the frame
 *  is unconfirmed.
 *
 *  If this timer does actually expire it means either no ACK was received or
 *  a class C device is now allowed to transmit another frame.
 *
 * @param ev    Pointer to event.
 */
static void
lora_mac_process_rtx_timeout(struct os_event *ev)
{
    lora_node_log(LORA_NODE_LOG_RTX_TIMEOUT, g_lora_mac_data.lmflags.lmf_all, 0,
                  0);
    lora_mac_tx_service_done(0);
}

static void
RxWindowSetup(bool rxContinuous, uint32_t maxRxWindow)
{
    if (rxContinuous == false) {
        Radio.Rx(maxRxWindow);
    } else {
        Radio.Rx(0);
    }
}

static bool
ValidatePayloadLength(uint8_t lenN, int8_t datarate, uint8_t fOptsLen)
{
    GetPhyParams_t getPhy;
    PhyParam_t phyParam;
    uint16_t maxN = 0;
    uint16_t payloadSize = 0;

    // Setup PHY request
    getPhy.UplinkDwellTime = LoRaMacParams.UplinkDwellTime;
    getPhy.Datarate = datarate;
    getPhy.Attribute = PHY_MAX_PAYLOAD;

    // Get the maximum payload length
    if (LM_F_REPEATER_SUPP()) {
        getPhy.Attribute = PHY_MAX_PAYLOAD_REPEATER;
    }
    phyParam = RegionGetPhyParam(LoRaMacRegion, &getPhy);
    maxN = phyParam.Value;

    // Calculate the resulting payload size
    payloadSize = (lenN + fOptsLen);

    // Validation of the application payload size
    if ((payloadSize <= maxN) && (payloadSize <= LORAMAC_PHY_MAXPAYLOAD)) {
        return true;
    }
    return false;
}

/**
 * This is called to make sure we schedule an uplink frame ASAP.
 *
 * XXX: Not sure if we need to inform the application about any of this. Look
 * into providing an upper layer callback for the MAC commands that cause this
 * to happen
 */
void
SetMlmeScheduleUplinkIndication( void )
{
    if (lora_node_txq_empty()) {
        lora_node_chk_txq();
    }
}

static LoRaMacStatus_t
AddMacCommand(uint8_t cmd, uint8_t p1, uint8_t p2)
{
    LoRaMacStatus_t status = LORAMAC_STATUS_BUSY;

    // The maximum buffer length must take MAC commands to re-send into account.
    uint8_t bufLen = LORA_MAC_CMD_BUF_LEN - MacCommandsBufferToRepeatIndex;

    switch(cmd) {
        case MOTE_MAC_LINK_CHECK_REQ:
            if (MacCommandsBufferIndex < bufLen) {
                MacCommandsBuffer[MacCommandsBufferIndex++] = cmd;
                // No payload for this command
                status = LORAMAC_STATUS_OK;
            }
            break;
        case MOTE_MAC_LINK_ADR_ANS:
            if (MacCommandsBufferIndex < (bufLen - 1)) {
                MacCommandsBuffer[MacCommandsBufferIndex++] = cmd;
                // Margin
                MacCommandsBuffer[MacCommandsBufferIndex++] = p1;
                status = LORAMAC_STATUS_OK;
            }
            break;
        case MOTE_MAC_DUTY_CYCLE_ANS:
            if (MacCommandsBufferIndex < bufLen) {
                MacCommandsBuffer[MacCommandsBufferIndex++] = cmd;
                // No payload for this answer
                status = LORAMAC_STATUS_OK;
            }
            break;
        case MOTE_MAC_RX_PARAM_SETUP_ANS:
            if (MacCommandsBufferIndex < (bufLen - 1)) {
                MacCommandsBuffer[MacCommandsBufferIndex++] = cmd;
                // Status: Datarate ACK, Channel ACK
                MacCommandsBuffer[MacCommandsBufferIndex++] = p1;
                // This is a sticky MAC command answer. Setup indication
                SetMlmeScheduleUplinkIndication( );
                status = LORAMAC_STATUS_OK;
            }
            break;
        case MOTE_MAC_DEV_STATUS_ANS:
            if (MacCommandsBufferIndex < (bufLen - 2)) {
                MacCommandsBuffer[MacCommandsBufferIndex++] = cmd;
                // 1st byte Battery
                // 2nd byte Margin
                MacCommandsBuffer[MacCommandsBufferIndex++] = p1;
                MacCommandsBuffer[MacCommandsBufferIndex++] = p2;
                status = LORAMAC_STATUS_OK;
            }
            break;
        case MOTE_MAC_NEW_CHANNEL_ANS:
            if (MacCommandsBufferIndex < (bufLen - 1)) {
                MacCommandsBuffer[MacCommandsBufferIndex++] = cmd;
                // Status: Datarate range OK, Channel frequency OK
                MacCommandsBuffer[MacCommandsBufferIndex++] = p1;
                status = LORAMAC_STATUS_OK;
            }
            break;
        case MOTE_MAC_RX_TIMING_SETUP_ANS:
            if (MacCommandsBufferIndex < bufLen) {
                MacCommandsBuffer[MacCommandsBufferIndex++] = cmd;
                // No payload for this answer
                // This is a sticky MAC command answer. Setup indication
                SetMlmeScheduleUplinkIndication( );
                status = LORAMAC_STATUS_OK;
            }
            break;
        case MOTE_MAC_TX_PARAM_SETUP_ANS:
            if (MacCommandsBufferIndex < bufLen) {
                MacCommandsBuffer[MacCommandsBufferIndex++] = cmd;
                // No payload for this answer
                status = LORAMAC_STATUS_OK;
            }
            break;
        case MOTE_MAC_DL_CHANNEL_ANS:
            if (MacCommandsBufferIndex < bufLen) {
                MacCommandsBuffer[MacCommandsBufferIndex++] = cmd;
                // Status: Uplink frequency exists, Channel frequency OK
                MacCommandsBuffer[MacCommandsBufferIndex++] = p1;

                // This is a sticky MAC command answer. Setup indication
                SetMlmeScheduleUplinkIndication( );

                status = LORAMAC_STATUS_OK;
            }
            break;
        default:
            status = LORAMAC_STATUS_SERVICE_UNKNOWN;
            break;
    }

    return status;
}

static uint8_t ParseMacCommandsToRepeat( uint8_t* cmdBufIn, uint8_t length, uint8_t* cmdBufOut )
{
    uint8_t i = 0;
    uint8_t cmdCount = 0;

    if ((cmdBufIn == NULL) || (cmdBufOut == NULL)) {
        return 0;
    }

    for (i = 0; i < length; i++) {
        switch( cmdBufIn[i] ) {
            // STICKY
            case MOTE_MAC_DL_CHANNEL_ANS:
            case MOTE_MAC_RX_PARAM_SETUP_ANS:
                cmdBufOut[cmdCount++] = cmdBufIn[i++];
                cmdBufOut[cmdCount++] = cmdBufIn[i];
                break;
            case MOTE_MAC_RX_TIMING_SETUP_ANS:
                cmdBufOut[cmdCount++] = cmdBufIn[i];
                break;
            // NON-STICKY
            case MOTE_MAC_DEV_STATUS_ANS:
                // 2 bytes payload
                i += 2;
                break;
            case MOTE_MAC_LINK_ADR_ANS:
            case MOTE_MAC_NEW_CHANNEL_ANS:
                // 1 byte payload
                i++;
                break;
            case MOTE_MAC_TX_PARAM_SETUP_ANS:
            case MOTE_MAC_DUTY_CYCLE_ANS:
            case MOTE_MAC_LINK_CHECK_REQ:
                // 0 byte payload
                break;
            default:
                break;
        }
    }

    return cmdCount;
}

static void
ProcessMacCommands(uint8_t *payload, uint8_t macIndex, uint8_t commandsSize,
                   uint8_t snr)
{
    uint8_t status;
    uint8_t i;
    uint8_t delay;
    uint8_t demod_margin;
    uint8_t gateways;
    uint8_t batteryLevel;
    uint8_t eirpDwellTime;
    LinkAdrReqParams_t linkAdrReq;
    int8_t linkAdrDatarate = DR_0;
    int8_t linkAdrTxPower = TX_POWER_0;
    uint8_t linkAdrNbRep = 0;
    uint8_t linkAdrNbBytesParsed = 0;
    NewChannelReqParams_t newChannelReq;
    ChannelParams_t chParam;
    RxParamSetupReqParams_t rxParamSetupReq;
    TxParamSetupReqParams_t txParamSetupReq;
    DlChannelReqParams_t dlChannelReq;

    lora_node_log(LORA_NODE_LOG_PROC_MAC_CMD, macIndex, snr, commandsSize);

    while (macIndex < commandsSize) {
        // Decode Frame MAC commands
        switch(payload[macIndex++]) {
        case SRV_MAC_LINK_CHECK_ANS:
            demod_margin = payload[macIndex++];
            gateways = payload[macIndex++];
            STATS_INC(lora_mac_stats, link_chk_ans_rxd);
            lora_app_link_chk_confirm(LORAMAC_EVENT_INFO_STATUS_OK,
                                      gateways, demod_margin);
            break;
        case SRV_MAC_LINK_ADR_REQ:
            // Fill parameter structure
            linkAdrReq.Payload = &payload[macIndex - 1];
            linkAdrReq.PayloadSize = commandsSize - ( macIndex - 1 );
            linkAdrReq.AdrEnabled = AdrCtrlOn;
            linkAdrReq.UplinkDwellTime = LoRaMacParams.UplinkDwellTime;
            linkAdrReq.CurrentDatarate = LoRaMacParams.ChannelsDatarate;
            linkAdrReq.CurrentTxPower = LoRaMacParams.ChannelsTxPower;
            linkAdrReq.CurrentNbRep = LoRaMacParams.ChannelsNbRep;

            // Process the ADR requests
            status = RegionLinkAdrReq(LoRaMacRegion, &linkAdrReq, &linkAdrDatarate,
                                       &linkAdrTxPower, &linkAdrNbRep, &linkAdrNbBytesParsed );

            if ((status & 0x07) == 0x07) {
                LoRaMacParams.ChannelsDatarate = linkAdrDatarate;
                LoRaMacParams.ChannelsTxPower = linkAdrTxPower;
                LoRaMacParams.ChannelsNbRep = linkAdrNbRep;
            }

            // Add the answers to the buffer
            for (i = 0; i < ( linkAdrNbBytesParsed / 5 ); i++) {
                AddMacCommand( MOTE_MAC_LINK_ADR_ANS, status, 0 );
            }
            // Update MAC index
            macIndex += linkAdrNbBytesParsed - 1;

            /* XXX: This does not show the raw data sent. It shows the
               processed output. Would be nice to see each piece */
            lora_node_log(LORA_NODE_LOG_RX_ADR_REQ, linkAdrDatarate,
                          linkAdrTxPower,linkAdrNbRep);
            break;
        case SRV_MAC_DUTY_CYCLE_REQ:
            g_lora_mac_data.max_dc = payload[macIndex++];
            g_lora_mac_data.aggr_dc = 1 << g_lora_mac_data.max_dc;
            AddMacCommand(MOTE_MAC_DUTY_CYCLE_ANS, 0, 0);
            break;
        case SRV_MAC_RX_PARAM_SETUP_REQ:
            status = 0x07;

            rxParamSetupReq.DrOffset = ( payload[macIndex] >> 4 ) & 0x07;
            rxParamSetupReq.Datarate = payload[macIndex] & 0x0F;
            macIndex++;

            rxParamSetupReq.Frequency =  ( uint32_t )payload[macIndex++];
            rxParamSetupReq.Frequency |= ( uint32_t )payload[macIndex++] << 8;
            rxParamSetupReq.Frequency |= ( uint32_t )payload[macIndex++] << 16;
            rxParamSetupReq.Frequency *= 100;

            // Perform request on region
            status = RegionRxParamSetupReq( LoRaMacRegion, &rxParamSetupReq );

            if ((status & 0x07) == 0x07) {
                LoRaMacParams.Rx2Channel.Datarate = rxParamSetupReq.Datarate;
                LoRaMacParams.Rx2Channel.Frequency = rxParamSetupReq.Frequency;
                LoRaMacParams.Rx1DrOffset = rxParamSetupReq.DrOffset;
            }
            AddMacCommand(MOTE_MAC_RX_PARAM_SETUP_ANS, status, 0);
            break;
        case SRV_MAC_DEV_STATUS_REQ:
            batteryLevel = BAT_LEVEL_NO_MEASURE;
            if ((LoRaMacCallbacks != NULL) && (LoRaMacCallbacks->GetBatteryLevel != NULL)) {
                batteryLevel = LoRaMacCallbacks->GetBatteryLevel( );
            }
            AddMacCommand(MOTE_MAC_DEV_STATUS_ANS, batteryLevel, snr & 0x3F);
            break;
        case SRV_MAC_NEW_CHANNEL_REQ:
            status = 0x03;

            newChannelReq.ChannelId = payload[macIndex++];
            newChannelReq.NewChannel = &chParam;

            chParam.Frequency = ( uint32_t )payload[macIndex++];
            chParam.Frequency |= ( uint32_t )payload[macIndex++] << 8;
            chParam.Frequency |= ( uint32_t )payload[macIndex++] << 16;
            chParam.Frequency *= 100;
            chParam.Rx1Frequency = 0;
            chParam.DrRange.Value = payload[macIndex++];

            status = RegionNewChannelReq( LoRaMacRegion, &newChannelReq );

            AddMacCommand( MOTE_MAC_NEW_CHANNEL_ANS, status, 0 );
            break;
        case SRV_MAC_RX_TIMING_SETUP_REQ:
            delay = payload[macIndex++] & 0x0F;
            if (delay == 0) {
                delay++;
            }
            LoRaMacParams.ReceiveDelay1 = delay * 1000;
            LoRaMacParams.ReceiveDelay2 = LoRaMacParams.ReceiveDelay1 + 1000;
            AddMacCommand(MOTE_MAC_RX_TIMING_SETUP_ANS, 0, 0);
            break;
        case SRV_MAC_TX_PARAM_SETUP_REQ:
            eirpDwellTime = payload[macIndex++];
            txParamSetupReq.UplinkDwellTime = 0;
            txParamSetupReq.DownlinkDwellTime = 0;

            if ((eirpDwellTime & 0x20) == 0x20) {
                txParamSetupReq.DownlinkDwellTime = 1;
            }
            if ((eirpDwellTime & 0x10) == 0x10) {
                txParamSetupReq.UplinkDwellTime = 1;
            }
            txParamSetupReq.MaxEirp = eirpDwellTime & 0x0F;

            // Check the status for correctness
            if (RegionTxParamSetupReq(LoRaMacRegion, &txParamSetupReq) != -1) {
                // Accept command
                LoRaMacParams.UplinkDwellTime = txParamSetupReq.UplinkDwellTime;
                LoRaMacParams.DownlinkDwellTime = txParamSetupReq.DownlinkDwellTime;
                LoRaMacParams.MaxEirp = LoRaMacMaxEirpTable[txParamSetupReq.MaxEirp];
                // Add command response
                AddMacCommand( MOTE_MAC_TX_PARAM_SETUP_ANS, 0, 0 );
            }
            break;
        case SRV_MAC_DL_CHANNEL_REQ:
            status = 0x03;

            dlChannelReq.ChannelId = payload[macIndex++];
            dlChannelReq.Rx1Frequency = ( uint32_t )payload[macIndex++];
            dlChannelReq.Rx1Frequency |= ( uint32_t )payload[macIndex++] << 8;
            dlChannelReq.Rx1Frequency |= ( uint32_t )payload[macIndex++] << 16;
            dlChannelReq.Rx1Frequency *= 100;

            status = RegionDlChannelReq(LoRaMacRegion, &dlChannelReq);

            AddMacCommand(MOTE_MAC_DL_CHANNEL_ANS, status, 0);
            break;
        default:
            // Unknown command. ABORT MAC commands processing
            return;
        }
    }
}

LoRaMacStatus_t
Send(LoRaMacHeader_t *macHdr, uint8_t fPort, struct os_mbuf *om)
{
    LoRaMacFrameCtrl_t fCtrl;
    LoRaMacStatus_t status = LORAMAC_STATUS_PARAMETER_INVALID;

    fCtrl.Value = 0;
    fCtrl.Bits.FOptsLen      = 0;
    fCtrl.Bits.FPending      = 0;
    fCtrl.Bits.Ack           = false;
    fCtrl.Bits.AdrAckReq     = false;
    fCtrl.Bits.Adr           = AdrCtrlOn;

    // Prepare the frame
    status = PrepareFrame(macHdr, &fCtrl, fPort, om);

    // Validate status
    if (status == LORAMAC_STATUS_OK) {
        status = ScheduleTx();
    }
    return status;
}

/**
 * ScheduleTx
 *
 * Attempt to send a frame.
 *
 * @return LoRaMacStatus_t This function can return the following status
 *  LORAMAC_STATUS_OK:              The transmission attempt was successful.
 *  LORAMAC_STATUS_LENGTH_ERROR:    The transmission cannot be attempted as the
 *                                  payload is too large
 *  LORAMAC_STATUS_DEVICE_OFF:      The transmission cannot be attempte as the
 *                                  device is off.
 *  LORAMAC_STATUS_NO_CHANNEL_FOUND Transmission cannot be attempted due to no
 *                                  channel available.
 *
 */
static LoRaMacStatus_t
ScheduleTx(void)
{
    uint32_t duty_cycle_time_off = 0;
    LoRaMacStatus_t status;
    NextChanParams_t nextChan;

    // Check if the device is off
    if (g_lora_mac_data.max_dc == 255) {
        return LORAMAC_STATUS_DEVICE_OFF;
    }

    if (g_lora_mac_data.max_dc == 0) {
        g_lora_mac_data.aggr_time_off = 0;
    }

    // Update Backoff
    CalculateBackOff(g_lora_mac_data.last_tx_chan);

    // Select channel
    nextChan.AggrTimeOff = g_lora_mac_data.aggr_time_off;
    nextChan.Datarate = LoRaMacParams.ChannelsDatarate;
    nextChan.DutyCycleEnabled = DutyCycleOn;
    nextChan.Joined = LM_F_IS_JOINED();
    nextChan.LastAggrTx = g_lora_mac_data.aggr_last_tx_done_time;

    status = RegionNextChannel(LoRaMacRegion, &nextChan,
                               &g_lora_mac_data.cur_chan, &duty_cycle_time_off,
                               &g_lora_mac_data.aggr_time_off);

    if (status != LORAMAC_STATUS_OK) {
        if (status == LORAMAC_STATUS_DUTYCYCLE_RESTRICTED) {
            assert(duty_cycle_time_off != 0);

            // Send later - prepare timer
            LoRaMacState |= LORAMAC_TX_DELAYED;
            hal_timer_stop(&TxDelayedTimer);
            hal_timer_start(&TxDelayedTimer, duty_cycle_time_off);

            lora_node_log(LORA_NODE_LOG_TX_DELAY, g_lora_mac_data.max_dc, 0,
                          duty_cycle_time_off);

            return LORAMAC_STATUS_OK;
        } else {
            // State where the MAC cannot send a frame
            return status;
        }
    }

    // Compute Rx1 windows parameters
    RegionComputeRxWindowParameters( LoRaMacRegion,
                                     RegionApplyDrOffset( LoRaMacRegion, LoRaMacParams.DownlinkDwellTime, LoRaMacParams.ChannelsDatarate, LoRaMacParams.Rx1DrOffset ),
                                     LoRaMacParams.MinRxSymbols,
                                     LoRaMacParams.SystemMaxRxError,
                                     &RxWindow1Config );

    // Compute Rx2 windows parameters
    RegionComputeRxWindowParameters( LoRaMacRegion,
                                     LoRaMacParams.Rx2Channel.Datarate,
                                     LoRaMacParams.MinRxSymbols,
                                     LoRaMacParams.SystemMaxRxError,
                                     &RxWindow2Config );

    /* XXX: Thi should be based on whether or not the transmission is a
       join request! */
    if (!LM_F_IS_JOINED()) {
        g_lora_mac_data.rx_win1_delay = LoRaMacParams.JoinAcceptDelay1 +
            RxWindow1Config.WindowOffset;
        g_lora_mac_data.rx_win2_delay = LoRaMacParams.JoinAcceptDelay2 +
            RxWindow2Config.WindowOffset;
    } else {
        if (!ValidatePayloadLength(g_lora_mac_data.cur_tx_pyld,
                                   LoRaMacParams.ChannelsDatarate, 0)) {
            return LORAMAC_STATUS_LENGTH_ERROR;
        }
        g_lora_mac_data.rx_win1_delay = LoRaMacParams.ReceiveDelay1 +
            RxWindow1Config.WindowOffset;
        g_lora_mac_data.rx_win2_delay = LoRaMacParams.ReceiveDelay2 +
            RxWindow2Config.WindowOffset;
    }

    // Try to send now
    return SendFrameOnChannel(g_lora_mac_data.cur_chan);
}

static void
CalculateBackOff(uint8_t channel)
{
    uint32_t elapsed_msecs;
    uint32_t tx_ticks;
    CalcBackOffParams_t calc_backoff;

    /* Convert the tx time on air (which is in msecs) to lora mac timer ticks */
    tx_ticks = g_lora_mac_data.tx_time_on_air * 1000;

    /*
     * Calculcate the elasped time (in msecs) from when we started the
     * join process. We do not ever expect this to wrap.
     */
    elapsed_msecs = (uint32_t)(os_time_get() - g_lora_mac_data.init_time);
    if (os_time_ticks_to_ms(elapsed_msecs, &elapsed_msecs)) {
        elapsed_msecs = 0xFFFFFFFF;
    }

    calc_backoff.Joined = LM_F_IS_JOINED();
    calc_backoff.DutyCycleEnabled = DutyCycleOn;
    calc_backoff.Channel = channel;
    calc_backoff.ElapsedTime =  elapsed_msecs;
    calc_backoff.TxTimeOnAir = tx_ticks;
    calc_backoff.LastTxIsJoinRequest = (bool)LM_F_LAST_TX_IS_JOIN_REQ();

    // Update regional back-off
    RegionCalcBackOff(LoRaMacRegion, &calc_backoff);

    // Update aggregated time-off. This must be an assignment and no incremental
    // update as we do only calculate the time-off based on the last transmission
    g_lora_mac_data.aggr_time_off = ((tx_ticks * g_lora_mac_data.aggr_dc) - tx_ticks);
}

/*!
 * \brief Resets MAC specific parameters to default
 */
static void
ResetMacParameters(void)
{
    uint8_t pub_nwk;
    uint8_t repeater;

    // Store the current initialization time
    g_lora_mac_data.init_time = os_time_get();

    // Counters
    g_lora_mac_data.uplink_cntr = 0;
    g_lora_mac_data.downlink_cntr = 0;
    g_lora_mac_data.adr_ack_cntr = 0;

    g_lora_mac_data.nb_rep_cntr = 0;

    g_lora_mac_data.ack_timeout_retries = 1;
    g_lora_mac_data.ack_timeout_retries_cntr = 1;

    g_lora_mac_data.max_dc = 0;
    g_lora_mac_data.aggr_dc = 1;

    MacCommandsBufferIndex = 0;
    MacCommandsBufferToRepeatIndex = 0;

    LoRaMacParams.ChannelsTxPower = LoRaMacParamsDefaults.ChannelsTxPower;
    LoRaMacParams.ChannelsDatarate = LoRaMacParamsDefaults.ChannelsDatarate;
    LoRaMacParams.Rx1DrOffset = LoRaMacParamsDefaults.Rx1DrOffset;
    LoRaMacParams.Rx2Channel = LoRaMacParamsDefaults.Rx2Channel;
    LoRaMacParams.UplinkDwellTime = LoRaMacParamsDefaults.UplinkDwellTime;
    LoRaMacParams.DownlinkDwellTime = LoRaMacParamsDefaults.DownlinkDwellTime;
    LoRaMacParams.MaxEirp = LoRaMacParamsDefaults.MaxEirp;
    LoRaMacParams.AntennaGain = LoRaMacParamsDefaults.AntennaGain;

    // Reset to application defaults
    RegionInitDefaults( LoRaMacRegion, INIT_TYPE_APP_DEFAULTS );

    /* Reset mac flags but retain those that need it */
    pub_nwk = LM_F_IS_PUBLIC_NWK();
    repeater = LM_F_REPEATER_SUPP();
    g_lora_mac_data.lmflags.lmf_all = 0;
    LM_F_IS_PUBLIC_NWK() = pub_nwk;
    LM_F_REPEATER_SUPP() = repeater;

    // Reset Multicast downlink counters
    MulticastParams_t *cur = MulticastChannels;
    while( cur != NULL )
    {
        cur->DownLinkCounter = 0;
        cur = cur->Next;
    }

    // Initialize channel index.
    g_lora_mac_data.cur_chan = 0;
    g_lora_mac_data.last_tx_chan = 0;
}

LoRaMacStatus_t
PrepareFrame(LoRaMacHeader_t *macHdr, LoRaMacFrameCtrl_t *fCtrl, uint8_t fPort,
             struct os_mbuf *om)
{
    AdrNextParams_t adrNext;
    GetPhyParams_t getPhy;
    PhyParam_t phyParam;
    uint8_t hdrlen = 0;
    uint16_t bufsize;
    uint32_t mic = 0;
    uint8_t port = fPort;
    const uint8_t *key;
    uint8_t maxbytes;
    uint8_t max_cmd_bytes;
    uint8_t cmd_bytes_txd;
    uint8_t pyld_len;
    uint16_t dev_nonce;
    uint32_t dev_addr;

    LM_F_NODE_ACK_REQ() = 0;

    /* Get the mac payload size from the mbuf. A NULL mbuf is valid */
    if (om == NULL) {
        bufsize = 0;
    } else {
        bufsize = OS_MBUF_PKTLEN(om);
        assert(bufsize > 0);
    }

    LoRaMacBufferPktLen = 0;
    pyld_len = bufsize;
    LoRaMacBuffer[hdrlen++] = macHdr->Value;

    switch(macHdr->Bits.MType) {
        case FRAME_TYPE_JOIN_REQ:
            LoRaMacBufferPktLen = hdrlen;

            swap_buf(LoRaMacBuffer + LoRaMacBufferPktLen, LoRaMacAppEui, 8);
            LoRaMacBufferPktLen += 8;
            swap_buf(LoRaMacBuffer + LoRaMacBufferPktLen, LoRaMacDevEui, 8);
            LoRaMacBufferPktLen += 8;

            dev_nonce = Radio.Random( );
            LoRaMacBuffer[LoRaMacBufferPktLen++] = dev_nonce & 0xFF;
            LoRaMacBuffer[LoRaMacBufferPktLen++] = (dev_nonce >> 8) & 0xFF;
            g_lora_mac_data.dev_nonce = dev_nonce;

            LoRaMacJoinComputeMic(LoRaMacBuffer, LoRaMacBufferPktLen & 0xFF,
                                  LoRaMacAppKey, &mic);

            LoRaMacBuffer[LoRaMacBufferPktLen++] = mic & 0xFF;
            LoRaMacBuffer[LoRaMacBufferPktLen++] = (mic >> 8) & 0xFF;
            LoRaMacBuffer[LoRaMacBufferPktLen++] = (mic >> 16) & 0xFF;
            LoRaMacBuffer[LoRaMacBufferPktLen++] = (mic >> 24) & 0xFF;
            break;
        case FRAME_TYPE_DATA_CONFIRMED_UP:
            LM_F_NODE_ACK_REQ() = 1;
            //Intentional fallthrough
        case FRAME_TYPE_DATA_UNCONFIRMED_UP:
            // Adr next request
            adrNext.UpdateChanMask = true;
            adrNext.AdrEnabled = fCtrl->Bits.Adr;
            adrNext.AdrAckCounter = g_lora_mac_data.adr_ack_cntr;
            adrNext.Datarate = LoRaMacParams.ChannelsDatarate;
            adrNext.TxPower = LoRaMacParams.ChannelsTxPower;
            adrNext.UplinkDwellTime = LoRaMacParams.UplinkDwellTime;

            fCtrl->Bits.AdrAckReq = RegionAdrNext(LoRaMacRegion, &adrNext,
                                                  &LoRaMacParams.ChannelsDatarate,
                                                  &LoRaMacParams.ChannelsTxPower,
                                                  &g_lora_mac_data.adr_ack_cntr);

            /* Can we send payload now that ADR changed? */
            if (!ValidatePayloadLength(pyld_len,LoRaMacParams.ChannelsDatarate, 0)) {
                return LORAMAC_STATUS_LENGTH_ERROR;
            }

            // Setup PHY request
            getPhy.UplinkDwellTime = LoRaMacParams.UplinkDwellTime;
            getPhy.Datarate = LoRaMacParams.ChannelsDatarate;
            getPhy.Attribute = PHY_MAX_PAYLOAD;

            // Get the maximum payload length
            if (LM_F_REPEATER_SUPP()) {
                getPhy.Attribute = PHY_MAX_PAYLOAD_REPEATER;
            }
            phyParam = RegionGetPhyParam(LoRaMacRegion, &getPhy);
            maxbytes = (uint8_t)phyParam.Value;

            if (LM_F_GW_ACK_REQ()) {
                LM_F_GW_ACK_REQ() = 0;
                fCtrl->Bits.Ack = 1;
            }

            dev_addr = g_lora_mac_data.dev_addr;
            LoRaMacBuffer[hdrlen++] = (uint8_t)(dev_addr & 0xFF);
            LoRaMacBuffer[hdrlen++] = (dev_addr >> 8)  & 0xFF;
            LoRaMacBuffer[hdrlen++] = (dev_addr >> 16) & 0xFF;
            LoRaMacBuffer[hdrlen++] = (dev_addr >> 24) & 0xFF;
            LoRaMacBuffer[hdrlen++] = fCtrl->Value;
            LoRaMacBuffer[hdrlen++] = g_lora_mac_data.uplink_cntr & 0xFF;
            LoRaMacBuffer[hdrlen++] = ( g_lora_mac_data.uplink_cntr >> 8 ) & 0xFF;

            /* Determine maximum MAC command bytes we can allow */
            max_cmd_bytes = maxbytes - pyld_len;

            // Copy MAC commands which must be re-sent into MAC command buffer
            memcpy(&MacCommandsBuffer[MacCommandsBufferIndex],
                   MacCommandsBufferToRepeat, MacCommandsBufferToRepeatIndex);
            MacCommandsBufferIndex += MacCommandsBufferToRepeatIndex;

            cmd_bytes_txd = 0;
            if (om != NULL) {
                if ((MacCommandsBufferIndex != 0) && (max_cmd_bytes != 0)) {
                    /* fopts cannot exceed 15 bytes */
                    if (max_cmd_bytes > LORA_MAC_COMMAND_MAX_FOPTS_LENGTH) {
                        max_cmd_bytes = LORA_MAC_COMMAND_MAX_FOPTS_LENGTH;
                    }
                    /* Extract only the mac commands that will fit */
                    cmd_bytes_txd = lora_mac_extract_mac_cmds(max_cmd_bytes,
                                                              &LoRaMacBuffer[hdrlen]);
                    if (cmd_bytes_txd) {
                        /* Update FCtrl field with new value of OptionsLength */
                        fCtrl->Bits.FOptsLen += cmd_bytes_txd;
                        LoRaMacBuffer[0x05] = fCtrl->Value;
                        hdrlen += cmd_bytes_txd;
                    }
                }

                /* Set the port and copy in the application payload */
                LoRaMacBuffer[hdrlen++] = port;
                os_mbuf_copydata(om, 0, pyld_len, LoRaMacBuffer + hdrlen);
            } else {
                if (MacCommandsBufferIndex > 0) {
                    port = 0;
                    LoRaMacBuffer[hdrlen++] = port;
                    cmd_bytes_txd = lora_mac_extract_mac_cmds(max_cmd_bytes,
                                                              &LoRaMacBuffer[hdrlen]);

                    pyld_len = cmd_bytes_txd;
                    assert(cmd_bytes_txd != 0);
                }
            }

            /*
             * At this point, we know the amount of payload we are going to
             * send. We need to remember this so we can validate the frame
             * length if we have to re-transmit the frame at a lower data rate.
             * Note that payload length we are storing in basically N (from
             * the lorawan specification.
             */
            g_lora_mac_data.cur_tx_pyld = cmd_bytes_txd + pyld_len;

            /*
             * Store MAC commands which must be re-sent in case the device does
             * not receive a downlink anymore.
             */
            if (cmd_bytes_txd) {
                MacCommandsBufferToRepeatIndex =
                    ParseMacCommandsToRepeat(MacCommandsBuffer, cmd_bytes_txd,
                                             MacCommandsBufferToRepeat);

                if (cmd_bytes_txd < MacCommandsBufferIndex) {
                    MacCommandsBufferIndex -= cmd_bytes_txd;
                    memmove(MacCommandsBuffer, &MacCommandsBuffer[cmd_bytes_txd],
                            MacCommandsBufferIndex);

                } else {
                    MacCommandsBufferIndex = 0;
                }
            }

            if (pyld_len > 0) {
                /* Encrypt the MAC payload using appropriate key */
                if (port == 0) {
                    key = LoRaMacNwkSKey;
                } else {
                    key = LoRaMacAppSKey;
                }
                LoRaMacPayloadEncrypt(LoRaMacBuffer + hdrlen,
                                      pyld_len,
                                      key,
                                      g_lora_mac_data.dev_addr,
                                      UP_LINK,
                                      g_lora_mac_data.uplink_cntr,
                                      LoRaMacBuffer + hdrlen);
            }
            LoRaMacBufferPktLen = hdrlen + pyld_len;
            LoRaMacComputeMic(LoRaMacBuffer,
                              LoRaMacBufferPktLen,
                              LoRaMacNwkSKey,
                              g_lora_mac_data.dev_addr,
                              UP_LINK,
                              g_lora_mac_data.uplink_cntr,
                              &mic);

            LoRaMacBuffer[LoRaMacBufferPktLen + 0] = mic & 0xFF;
            LoRaMacBuffer[LoRaMacBufferPktLen + 1] = ( mic >> 8 ) & 0xFF;
            LoRaMacBuffer[LoRaMacBufferPktLen + 2] = ( mic >> 16 ) & 0xFF;
            LoRaMacBuffer[LoRaMacBufferPktLen + 3] = ( mic >> 24 ) & 0xFF;

            LoRaMacBufferPktLen += LORAMAC_MFR_LEN;

            lora_node_log(LORA_NODE_LOG_TX_PREP_FRAME, cmd_bytes_txd,
                          (uint16_t)g_lora_mac_data.uplink_cntr,
                          macHdr->Value);
            break;
        case FRAME_TYPE_PROPRIETARY:
            if ((om != NULL) && (pyld_len > 0)) {
                os_mbuf_copydata(om, 0, pyld_len, LoRaMacBuffer + hdrlen);
                LoRaMacBufferPktLen = hdrlen + pyld_len;
            }
            break;
        default:
            return LORAMAC_STATUS_SERVICE_UNKNOWN;
    }

    return LORAMAC_STATUS_OK;
}

LoRaMacStatus_t
SendFrameOnChannel(uint8_t channel)
{
    TxConfigParams_t txConfig;
    struct lora_pkt_info *txi;
    int8_t txPower = 0;

    txConfig.Channel = channel;
    txConfig.Datarate = LoRaMacParams.ChannelsDatarate;
    txConfig.TxPower = LoRaMacParams.ChannelsTxPower;
    txConfig.MaxEirp = LoRaMacParams.MaxEirp;
    txConfig.AntennaGain = LoRaMacParams.AntennaGain;
    txConfig.PktLen = LoRaMacBufferPktLen;

    RegionTxConfig(LoRaMacRegion, &txConfig, &txPower,
                   &g_lora_mac_data.tx_time_on_air);

    /* Set MCPS confirm information */
    txi = g_lora_mac_data.curtx;
    txi->txdinfo.datarate = LoRaMacParams.ChannelsDatarate;
    txi->txdinfo.txpower = txPower;
    txi->txdinfo.uplink_chan = channel;
    txi->txdinfo.tx_time_on_air = g_lora_mac_data.tx_time_on_air;

    // Send now
    Radio.Send(LoRaMacBuffer, LoRaMacBufferPktLen);

    LoRaMacState |= LORAMAC_TX_RUNNING;

    lora_node_log(LORA_NODE_LOG_TX_START, txPower,
                  ((uint16_t)LoRaMacParams.ChannelsDatarate << 8) | channel,
                  g_lora_mac_data.tx_time_on_air);

    return LORAMAC_STATUS_OK;
}

LoRaMacStatus_t
SetTxContinuousWave(uint16_t timeout)
{
    return LORAMAC_STATUS_SERVICE_UNKNOWN;
}

LoRaMacStatus_t
SetTxContinuousWave1(uint16_t timeout, uint32_t frequency, uint8_t power)
{
    return LORAMAC_STATUS_SERVICE_UNKNOWN;
}

LoRaMacStatus_t
LoRaMacInitialization(LoRaMacCallback_t *callbacks, LoRaMacRegion_t region)
{
    GetPhyParams_t getPhy;
    PhyParam_t phyParam;

    /* XXX: note that we used to use the LORA_NODE_DEFAULT_DATARATE here
     * See if we should still use it or change that mynewt_val. The code
     * below just uses the PHY default data upon init. I think this needs
     * more thought. We could always just change the default rate with a
     * MIB call I suspect (after initialization).
     */

    // Verify if the region is supported
    if (RegionIsActive(region) == false) {
        return LORAMAC_STATUS_REGION_NOT_SUPPORTED;
    }

    LoRaMacCallbacks = callbacks;
    LoRaMacRegion = region;

    LoRaMacDeviceClass = CLASS_A;
    LoRaMacState = LORAMAC_IDLE;

    // Reset duty cycle times
    g_lora_mac_data.aggr_last_tx_done_time = 0;
    g_lora_mac_data.aggr_time_off = 0;

    // Reset to defaults
    getPhy.Attribute = PHY_DUTY_CYCLE;
    phyParam = RegionGetPhyParam( LoRaMacRegion, &getPhy );
    DutyCycleOn = ( bool ) phyParam.Value;

    getPhy.Attribute = PHY_DEF_TX_POWER;
    phyParam = RegionGetPhyParam( LoRaMacRegion, &getPhy );
    LoRaMacParamsDefaults.ChannelsTxPower = phyParam.Value;

    getPhy.Attribute = PHY_DEF_TX_DR;
    phyParam = RegionGetPhyParam( LoRaMacRegion, &getPhy );
    LoRaMacParamsDefaults.ChannelsDatarate = phyParam.Value;

    getPhy.Attribute = PHY_MAX_RX_WINDOW;
    phyParam = RegionGetPhyParam( LoRaMacRegion, &getPhy );
    LoRaMacParamsDefaults.MaxRxWindow = phyParam.Value;

    getPhy.Attribute = PHY_RECEIVE_DELAY1;
    phyParam = RegionGetPhyParam( LoRaMacRegion, &getPhy );
    LoRaMacParamsDefaults.ReceiveDelay1 = phyParam.Value;

    getPhy.Attribute = PHY_RECEIVE_DELAY2;
    phyParam = RegionGetPhyParam( LoRaMacRegion, &getPhy );
    LoRaMacParamsDefaults.ReceiveDelay2 = phyParam.Value;

    getPhy.Attribute = PHY_JOIN_ACCEPT_DELAY1;
    phyParam = RegionGetPhyParam( LoRaMacRegion, &getPhy );
    LoRaMacParamsDefaults.JoinAcceptDelay1 = phyParam.Value;

    getPhy.Attribute = PHY_JOIN_ACCEPT_DELAY2;
    phyParam = RegionGetPhyParam( LoRaMacRegion, &getPhy );
    LoRaMacParamsDefaults.JoinAcceptDelay2 = phyParam.Value;

    getPhy.Attribute = PHY_DEF_DR1_OFFSET;
    phyParam = RegionGetPhyParam( LoRaMacRegion, &getPhy );
    LoRaMacParamsDefaults.Rx1DrOffset = phyParam.Value;

    getPhy.Attribute = PHY_DEF_RX2_FREQUENCY;
    phyParam = RegionGetPhyParam( LoRaMacRegion, &getPhy );
    LoRaMacParamsDefaults.Rx2Channel.Frequency = phyParam.Value;

    getPhy.Attribute = PHY_DEF_RX2_DR;
    phyParam = RegionGetPhyParam( LoRaMacRegion, &getPhy );
    LoRaMacParamsDefaults.Rx2Channel.Datarate = phyParam.Value;

    getPhy.Attribute = PHY_DEF_UPLINK_DWELL_TIME;
    phyParam = RegionGetPhyParam( LoRaMacRegion, &getPhy );
    LoRaMacParamsDefaults.UplinkDwellTime = phyParam.Value;

    getPhy.Attribute = PHY_DEF_DOWNLINK_DWELL_TIME;
    phyParam = RegionGetPhyParam( LoRaMacRegion, &getPhy );
    LoRaMacParamsDefaults.DownlinkDwellTime = phyParam.Value;

    getPhy.Attribute = PHY_DEF_MAX_EIRP;
    phyParam = RegionGetPhyParam( LoRaMacRegion, &getPhy );
    LoRaMacParamsDefaults.MaxEirp = phyParam.fValue;

    getPhy.Attribute = PHY_DEF_ANTENNA_GAIN;
    phyParam = RegionGetPhyParam( LoRaMacRegion, &getPhy );
    LoRaMacParamsDefaults.AntennaGain = phyParam.fValue;

    RegionInitDefaults( LoRaMacRegion, INIT_TYPE_INIT );

    // Init parameters which are not set in function ResetMacParameters
    LoRaMacParamsDefaults.ChannelsNbRep = 1;
    LoRaMacParamsDefaults.SystemMaxRxError = 10;
    LoRaMacParamsDefaults.MinRxSymbols = 6;

    LoRaMacParams.SystemMaxRxError = LoRaMacParamsDefaults.SystemMaxRxError;
    LoRaMacParams.MinRxSymbols = LoRaMacParamsDefaults.MinRxSymbols;
    LoRaMacParams.MaxRxWindow = LoRaMacParamsDefaults.MaxRxWindow;
    LoRaMacParams.ReceiveDelay1 = LoRaMacParamsDefaults.ReceiveDelay1;
    LoRaMacParams.ReceiveDelay2 = LoRaMacParamsDefaults.ReceiveDelay2;
    LoRaMacParams.JoinAcceptDelay1 = LoRaMacParamsDefaults.JoinAcceptDelay1;
    LoRaMacParams.JoinAcceptDelay2 = LoRaMacParamsDefaults.JoinAcceptDelay2;
    LoRaMacParams.ChannelsNbRep = LoRaMacParamsDefaults.ChannelsNbRep;

    ResetMacParameters( );

    /* XXX: determine which of these should be os callouts */
    hal_timer_config(LORA_MAC_TIMER_NUM, LORA_MAC_TIMER_FREQ);
    hal_timer_set_cb(LORA_MAC_TIMER_NUM, &TxDelayedTimer, OnTxDelayedTimerEvent,
                     NULL);
    hal_timer_set_cb(LORA_MAC_TIMER_NUM, &RxWindowTimer1, OnRxWindow1TimerEvent,
                     NULL);
    hal_timer_set_cb(LORA_MAC_TIMER_NUM, &RxWindowTimer2, OnRxWindow2TimerEvent,
                     NULL);
    hal_timer_set_cb(LORA_MAC_TIMER_NUM, &g_lora_mac_data.rtx_timer,
                     lora_mac_rtx_timer_cb, NULL);

    /* Init MAC radio events */
    g_lora_mac_radio_tx_timeout_event.ev_cb = lora_mac_process_radio_tx_timeout;
    g_lora_mac_radio_tx_event.ev_cb = lora_mac_process_radio_tx;
    g_lora_mac_radio_rx_event.ev_cb = lora_mac_process_radio_rx;
    g_lora_mac_radio_rx_timeout_event.ev_cb = lora_mac_process_radio_rx_timeout;
    g_lora_mac_radio_rx_err_event.ev_cb = lora_mac_process_radio_rx_err;
    g_lora_mac_rtx_timeout_event.ev_cb = lora_mac_process_rtx_timeout;
    g_lora_mac_rx_win1_event.ev_cb = lora_mac_process_rx_win1_timeout;
    g_lora_mac_rx_win2_event.ev_cb = lora_mac_process_rx_win2_timeout;
    g_lora_mac_tx_delay_timeout_event.ev_cb = lora_mac_process_tx_delay_timeout;

    // Initialize Radio driver
    RadioEvents.TxDone = OnRadioTxDone;
    RadioEvents.RxDone = OnRadioRxDone;
    RadioEvents.RxError = OnRadioRxError;
    RadioEvents.TxTimeout = OnRadioTxTimeout;
    RadioEvents.RxTimeout = OnRadioRxTimeout;
    Radio.Init( &RadioEvents );

    // Random seed initialization
    //srand1( Radio.Random( ) );

#if MYNEWT_VAL(LORA_NODE_PUBLIC_NWK)
    LM_F_IS_PUBLIC_NWK() = 1;
    Radio.SetPublicNetwork(true);
#else
    LM_F_IS_PUBLIC_NWK() = 0;
    Radio.SetPublicNetwork(false);
#endif
    Radio.Sleep( );

    return LORAMAC_STATUS_OK;
}

LoRaMacStatus_t
LoRaMacQueryTxPossible(uint8_t size, LoRaMacTxInfo_t* txInfo)
{
    AdrNextParams_t adrNext;
    GetPhyParams_t getPhy;
    PhyParam_t phyParam;
    int8_t datarate = LoRaMacParamsDefaults.ChannelsDatarate;
    int8_t txPower = LoRaMacParamsDefaults.ChannelsTxPower;
    uint8_t fOptLen = MacCommandsBufferIndex + MacCommandsBufferToRepeatIndex;

    assert(txInfo != NULL);

    // Setup ADR request
    adrNext.UpdateChanMask = false;
    adrNext.AdrEnabled = AdrCtrlOn;
    adrNext.AdrAckCounter = g_lora_mac_data.adr_ack_cntr;
    adrNext.Datarate = LoRaMacParams.ChannelsDatarate;
    adrNext.TxPower = LoRaMacParams.ChannelsTxPower;
    adrNext.UplinkDwellTime = LoRaMacParams.UplinkDwellTime;

    // We call the function for information purposes only. We don't want to
    // apply the datarate, the tx power and the ADR ack counter.
    RegionAdrNext(LoRaMacRegion, &adrNext, &datarate, &txPower,
                  &g_lora_mac_data.adr_ack_cntr);

    // Setup PHY request
    getPhy.UplinkDwellTime = LoRaMacParams.UplinkDwellTime;
    getPhy.Datarate = datarate;
    getPhy.Attribute = PHY_MAX_PAYLOAD;

    // Change request in case repeater is supported
    if (LM_F_REPEATER_SUPP() == true) {
        getPhy.Attribute = PHY_MAX_PAYLOAD_REPEATER;
    }
    phyParam = RegionGetPhyParam( LoRaMacRegion, &getPhy );
    txInfo->CurrentPayloadSize = phyParam.Value;

    if (txInfo->CurrentPayloadSize >= fOptLen) {
        txInfo->MaxPossiblePayload = txInfo->CurrentPayloadSize - fOptLen;
    } else {
        return LORAMAC_STATUS_MAC_CMD_LENGTH_ERROR;
    }

    if (!ValidatePayloadLength(size, datarate, 0)) {
        return LORAMAC_STATUS_LENGTH_ERROR;
    }

    if (!ValidatePayloadLength(size, datarate, fOptLen)) {
        return LORAMAC_STATUS_MAC_CMD_LENGTH_ERROR;
    }

    return LORAMAC_STATUS_OK;
}

LoRaMacStatus_t LoRaMacMibGetRequestConfirm( MibRequestConfirm_t *mibGet )
{
    LoRaMacStatus_t status = LORAMAC_STATUS_OK;
    GetPhyParams_t getPhy;
    PhyParam_t phyParam;

    if (mibGet == NULL) {
        return LORAMAC_STATUS_PARAMETER_INVALID;
    }

    switch (mibGet->Type) {
        case MIB_DEVICE_CLASS:
            mibGet->Param.Class = LoRaMacDeviceClass;
            break;
        case MIB_NETWORK_JOINED:
            mibGet->Param.IsNetworkJoined = LM_F_IS_JOINED();
            break;
        case MIB_ADR:
            mibGet->Param.AdrEnable = AdrCtrlOn;
            break;
        case MIB_NET_ID:
            mibGet->Param.NetID = g_lora_mac_data.netid;
            break;
        case MIB_DEV_ADDR:
            mibGet->Param.DevAddr = g_lora_mac_data.dev_addr;
            break;
        case MIB_NWK_SKEY:
            mibGet->Param.NwkSKey = LoRaMacNwkSKey;
            break;
        case MIB_APP_SKEY:
            mibGet->Param.AppSKey = LoRaMacAppSKey;
            break;
        case MIB_PUBLIC_NETWORK:
            mibGet->Param.EnablePublicNetwork = (bool)LM_F_IS_PUBLIC_NWK();
            break;
        case MIB_REPEATER_SUPPORT:
            mibGet->Param.EnableRepeaterSupport = LM_F_REPEATER_SUPP();
            break;
        case MIB_CHANNELS:
            getPhy.Attribute = PHY_CHANNELS;
            phyParam = RegionGetPhyParam( LoRaMacRegion, &getPhy );

            mibGet->Param.ChannelList = phyParam.Channels;
            break;
        case MIB_RX2_CHANNEL:
            mibGet->Param.Rx2Channel = LoRaMacParams.Rx2Channel;
            break;
        case MIB_RX2_DEFAULT_CHANNEL:
            mibGet->Param.Rx2Channel = LoRaMacParamsDefaults.Rx2Channel;
            break;
        case MIB_CHANNELS_DEFAULT_MASK:
            getPhy.Attribute = PHY_CHANNELS_DEFAULT_MASK;
            phyParam = RegionGetPhyParam( LoRaMacRegion, &getPhy );

            mibGet->Param.ChannelsDefaultMask = phyParam.ChannelsMask;
            break;
        case MIB_CHANNELS_MASK:
            getPhy.Attribute = PHY_CHANNELS_MASK;
            phyParam = RegionGetPhyParam( LoRaMacRegion, &getPhy );

            mibGet->Param.ChannelsMask = phyParam.ChannelsMask;
            break;
        case MIB_CHANNELS_NB_REP:
            mibGet->Param.ChannelNbRep = LoRaMacParams.ChannelsNbRep;
            break;
        case MIB_MAX_RX_WINDOW_DURATION:
            mibGet->Param.MaxRxWindow = LoRaMacParams.MaxRxWindow;
            break;
        case MIB_RECEIVE_DELAY_1:
            mibGet->Param.ReceiveDelay1 = LoRaMacParams.ReceiveDelay1;
            break;
        case MIB_RECEIVE_DELAY_2:
            mibGet->Param.ReceiveDelay2 = LoRaMacParams.ReceiveDelay2;
            break;
        case MIB_JOIN_ACCEPT_DELAY_1:
            mibGet->Param.JoinAcceptDelay1 = LoRaMacParams.JoinAcceptDelay1;
            break;
        case MIB_JOIN_ACCEPT_DELAY_2:
            mibGet->Param.JoinAcceptDelay2 = LoRaMacParams.JoinAcceptDelay2;
            break;
        case MIB_CHANNELS_DEFAULT_DATARATE:
            mibGet->Param.ChannelsDefaultDatarate = LoRaMacParamsDefaults.ChannelsDatarate;
            break;
        case MIB_CHANNELS_DATARATE:
            mibGet->Param.ChannelsDatarate = LoRaMacParams.ChannelsDatarate;
            break;
        case MIB_CHANNELS_DEFAULT_TX_POWER:
            mibGet->Param.ChannelsDefaultTxPower = LoRaMacParamsDefaults.ChannelsTxPower;
            break;
        case MIB_CHANNELS_TX_POWER:
            mibGet->Param.ChannelsTxPower = LoRaMacParams.ChannelsTxPower;
            break;
        case MIB_UPLINK_COUNTER:
            mibGet->Param.UpLinkCounter = g_lora_mac_data.uplink_cntr;
            break;
        case MIB_DOWNLINK_COUNTER:
            mibGet->Param.DownLinkCounter = g_lora_mac_data.downlink_cntr;
            break;
        case MIB_MULTICAST_CHANNEL:
            mibGet->Param.MulticastList = MulticastChannels;
            break;
        case MIB_SYSTEM_MAX_RX_ERROR:
            mibGet->Param.SystemMaxRxError = LoRaMacParams.SystemMaxRxError;
            break;
        case MIB_MIN_RX_SYMBOLS:
            mibGet->Param.MinRxSymbols = LoRaMacParams.MinRxSymbols;
            break;
        case MIB_ANTENNA_GAIN:
            mibGet->Param.AntennaGain = LoRaMacParams.AntennaGain;
            break;
        case MIB_DEFAULT_ANTENNA_GAIN:
            mibGet->Param.DefaultAntennaGain = LoRaMacParamsDefaults.AntennaGain;
            break;
        default:
            status = LORAMAC_STATUS_SERVICE_UNKNOWN;
            break;
    }

    return status;
}

LoRaMacStatus_t
LoRaMacMibSetRequestConfirm(MibRequestConfirm_t *mibSet)
{
    LoRaMacStatus_t status = LORAMAC_STATUS_OK;
    ChanMaskSetParams_t chanMaskSet;
    VerifyParams_t verify;

    assert(mibSet != NULL);

    if ((LoRaMacState & LORAMAC_TX_RUNNING ) == LORAMAC_TX_RUNNING) {
        return LORAMAC_STATUS_BUSY;
    }

    switch (mibSet->Type) {
    case MIB_DEVICE_CLASS:
            /* Do not allow device class to be set if tx delayed either! */
            if (lora_mac_tx_state() == LORAMAC_STATUS_BUSY) {
                return LORAMAC_STATUS_BUSY;
            }

            LoRaMacDeviceClass = mibSet->Param.Class;
            switch( LoRaMacDeviceClass )
            {
                case CLASS_A:
                    // Set the radio into sleep to setup a defined state
                    Radio.Sleep( );
                    break;
                case CLASS_B:
                    break;
                case CLASS_C:
                    Radio.Sleep();

                    // Compute Rx2 windows parameters in case the RX2 datarate has changed
                    RegionComputeRxWindowParameters( LoRaMacRegion,
                                                     LoRaMacParams.Rx2Channel.Datarate,
                                                     LoRaMacParams.MinRxSymbols,
                                                     LoRaMacParams.SystemMaxRxError,
                                                     &RxWindow2Config );

                    lora_mac_rx_on_window2();
                    break;
            }
            break;
        case MIB_NETWORK_JOINED:
            LM_F_IS_JOINED() = mibSet->Param.IsNetworkJoined;
            break;
        case MIB_ADR:
            AdrCtrlOn = mibSet->Param.AdrEnable;
            break;
        case MIB_NET_ID:
            g_lora_mac_data.netid = mibSet->Param.NetID;
            break;
        case MIB_DEV_ADDR:
            g_lora_mac_data.dev_addr = mibSet->Param.DevAddr;
            break;
        case MIB_NWK_SKEY:
            if (mibSet->Param.NwkSKey != NULL) {
                memcpy( LoRaMacNwkSKey, mibSet->Param.NwkSKey,
                               sizeof( LoRaMacNwkSKey ) );
            } else {
                status = LORAMAC_STATUS_PARAMETER_INVALID;
            }
            break;
        case MIB_APP_SKEY:
            if (mibSet->Param.AppSKey != NULL) {
                memcpy( LoRaMacAppSKey, mibSet->Param.AppSKey,
                               sizeof( LoRaMacAppSKey ) );
            } else {
                status = LORAMAC_STATUS_PARAMETER_INVALID;
            }
            break;
        case MIB_PUBLIC_NETWORK:
            LM_F_IS_PUBLIC_NWK() = mibSet->Param.EnablePublicNetwork;
            Radio.SetPublicNetwork((bool)LM_F_IS_PUBLIC_NWK());
            break;
        case MIB_REPEATER_SUPPORT:
             LM_F_REPEATER_SUPP() = mibSet->Param.EnableRepeaterSupport;
            break;
        case MIB_RX2_CHANNEL:
            verify.DatarateParams.Datarate = mibSet->Param.Rx2Channel.Datarate;
            verify.DatarateParams.DownlinkDwellTime = LoRaMacParams.DownlinkDwellTime;

            if (RegionVerify( LoRaMacRegion, &verify, PHY_RX_DR) == true) {
                LoRaMacParams.Rx2Channel = mibSet->Param.Rx2Channel;

                if ((LoRaMacDeviceClass == CLASS_C ) && LM_F_IS_JOINED()) {
                    // We can only compute the RX window parameters directly, if we are already
                    // in class c mode and joined. We cannot setup an RX window in case of any other
                    // class type.
                    // Set the radio into sleep mode in case we are still in RX mode
                    Radio.Sleep( );

                    // Compute Rx2 windows parameters
                    RegionComputeRxWindowParameters(LoRaMacRegion,
                                                    LoRaMacParams.Rx2Channel.Datarate,
                                                    LoRaMacParams.MinRxSymbols,
                                                    LoRaMacParams.SystemMaxRxError,
                                                    &RxWindow2Config);
                    /* Use our function */
                    lora_mac_rx_on_window2();
                }
            } else {
                status = LORAMAC_STATUS_PARAMETER_INVALID;
            }
            break;
        case MIB_RX2_DEFAULT_CHANNEL:
            verify.DatarateParams.Datarate = mibSet->Param.Rx2Channel.Datarate;
            verify.DatarateParams.DownlinkDwellTime = LoRaMacParams.DownlinkDwellTime;

            if (RegionVerify( LoRaMacRegion, &verify, PHY_RX_DR ) == true) {
                LoRaMacParamsDefaults.Rx2Channel = mibSet->Param.Rx2DefaultChannel;
            } else {
                status = LORAMAC_STATUS_PARAMETER_INVALID;
            }
            break;
        case MIB_CHANNELS_DEFAULT_MASK:
            chanMaskSet.ChannelsMaskIn = mibSet->Param.ChannelsMask;
            chanMaskSet.ChannelsMaskType = CHANNELS_DEFAULT_MASK;

            if (RegionChanMaskSet( LoRaMacRegion, &chanMaskSet) == false) {
                status = LORAMAC_STATUS_PARAMETER_INVALID;
            }
            break;
        case MIB_CHANNELS_MASK:
            chanMaskSet.ChannelsMaskIn = mibSet->Param.ChannelsMask;
            chanMaskSet.ChannelsMaskType = CHANNELS_MASK;

            if (RegionChanMaskSet( LoRaMacRegion, &chanMaskSet) == false){
                status = LORAMAC_STATUS_PARAMETER_INVALID;
            }
            break;
        case MIB_CHANNELS_NB_REP:
            if ((mibSet->Param.ChannelNbRep >= 1 ) &&
                (mibSet->Param.ChannelNbRep <= 15 )) {
                LoRaMacParams.ChannelsNbRep = mibSet->Param.ChannelNbRep;
            } else {
                status = LORAMAC_STATUS_PARAMETER_INVALID;
            }
            break;
        case MIB_MAX_RX_WINDOW_DURATION:
            LoRaMacParams.MaxRxWindow = mibSet->Param.MaxRxWindow;
            break;
        case MIB_RECEIVE_DELAY_1:
            LoRaMacParams.ReceiveDelay1 = mibSet->Param.ReceiveDelay1;
            break;
        case MIB_RECEIVE_DELAY_2:
            LoRaMacParams.ReceiveDelay2 = mibSet->Param.ReceiveDelay2;
            break;
        case MIB_JOIN_ACCEPT_DELAY_1:
            LoRaMacParams.JoinAcceptDelay1 = mibSet->Param.JoinAcceptDelay1;
            break;
        case MIB_JOIN_ACCEPT_DELAY_2:
            LoRaMacParams.JoinAcceptDelay2 = mibSet->Param.JoinAcceptDelay2;
            break;
        case MIB_CHANNELS_DEFAULT_DATARATE:
            verify.DatarateParams.Datarate = mibSet->Param.ChannelsDefaultDatarate;

            if (RegionVerify( LoRaMacRegion, &verify, PHY_DEF_TX_DR ) == true) {
                LoRaMacParamsDefaults.ChannelsDatarate = verify.DatarateParams.Datarate;
            } else {
                status = LORAMAC_STATUS_PARAMETER_INVALID;
            }
            break;
        case MIB_CHANNELS_DATARATE:
            verify.DatarateParams.Datarate = mibSet->Param.ChannelsDatarate;
            verify.DatarateParams.UplinkDwellTime = LoRaMacParams.UplinkDwellTime;

            if (RegionVerify( LoRaMacRegion, &verify, PHY_TX_DR ) == true) {
                LoRaMacParams.ChannelsDatarate = verify.DatarateParams.Datarate;
            } else {
                status = LORAMAC_STATUS_PARAMETER_INVALID;
            }
            break;
        case MIB_CHANNELS_DEFAULT_TX_POWER:
            verify.TxPower = mibSet->Param.ChannelsDefaultTxPower;

            if (RegionVerify(LoRaMacRegion, &verify, PHY_DEF_TX_POWER) == true) {
                LoRaMacParamsDefaults.ChannelsTxPower = verify.TxPower;
            } else {
                status = LORAMAC_STATUS_PARAMETER_INVALID;
            }
            break;
        case MIB_CHANNELS_TX_POWER:
            verify.TxPower = mibSet->Param.ChannelsTxPower;

            if (RegionVerify( LoRaMacRegion, &verify, PHY_TX_POWER ) == true) {
                LoRaMacParams.ChannelsTxPower = verify.TxPower;
            } else {
                status = LORAMAC_STATUS_PARAMETER_INVALID;
            }
            break;
        case MIB_UPLINK_COUNTER:
            g_lora_mac_data.uplink_cntr = mibSet->Param.UpLinkCounter;
            break;
        case MIB_DOWNLINK_COUNTER:
            g_lora_mac_data.downlink_cntr = mibSet->Param.DownLinkCounter;
            break;
        case MIB_SYSTEM_MAX_RX_ERROR:
            LoRaMacParams.SystemMaxRxError = LoRaMacParamsDefaults.SystemMaxRxError = mibSet->Param.SystemMaxRxError;
            break;
        case MIB_MIN_RX_SYMBOLS:
            LoRaMacParams.MinRxSymbols = LoRaMacParamsDefaults.MinRxSymbols = mibSet->Param.MinRxSymbols;
            break;
        case MIB_ANTENNA_GAIN:
            LoRaMacParams.AntennaGain = mibSet->Param.AntennaGain;
            break;
        case MIB_DEFAULT_ANTENNA_GAIN:
            LoRaMacParamsDefaults.AntennaGain = mibSet->Param.DefaultAntennaGain;
            break;
        default:
            status = LORAMAC_STATUS_SERVICE_UNKNOWN;
            break;
    }

    return status;
}

LoRaMacStatus_t
LoRaMacChannelAdd(uint8_t id, ChannelParams_t params)
{
    ChannelAddParams_t channelAdd;

    // Validate if the MAC is in a correct state
    if ((LoRaMacState & LORAMAC_TX_RUNNING) == LORAMAC_TX_RUNNING) {
        if ((LoRaMacState & LORAMAC_TX_CONFIG) != LORAMAC_TX_CONFIG) {
            return LORAMAC_STATUS_BUSY;
        }
    }

    channelAdd.NewChannel = &params;
    channelAdd.ChannelId = id;

    return RegionChannelAdd(LoRaMacRegion, &channelAdd);
}

LoRaMacStatus_t LoRaMacChannelRemove( uint8_t id )
{
    ChannelRemoveParams_t channelRemove;

    if ((LoRaMacState & LORAMAC_TX_RUNNING) == LORAMAC_TX_RUNNING) {
        if ((LoRaMacState & LORAMAC_TX_CONFIG) != LORAMAC_TX_CONFIG) {
            return LORAMAC_STATUS_BUSY;
        }
    }

    channelRemove.ChannelId = id;

    if (RegionChannelsRemove( LoRaMacRegion, &channelRemove) == false) {
        return LORAMAC_STATUS_PARAMETER_INVALID;
    }
    return LORAMAC_STATUS_OK;
}

LoRaMacStatus_t
LoRaMacMulticastChannelLink( MulticastParams_t *channelParam )
{
    if (channelParam == NULL) {
        return LORAMAC_STATUS_PARAMETER_INVALID;
    }

    if ((LoRaMacState & LORAMAC_TX_RUNNING) == LORAMAC_TX_RUNNING) {
        return LORAMAC_STATUS_BUSY;
    }

    // Reset downlink counter
    channelParam->DownLinkCounter = 0;

    if (MulticastChannels == NULL) {
        // New node is the fist element
        MulticastChannels = channelParam;
    } else {
        MulticastParams_t *cur = MulticastChannels;

        // Search the last node in the list
        while( cur->Next != NULL) {
            cur = cur->Next;
        }
        // This function always finds the last node
        cur->Next = channelParam;
    }

    return LORAMAC_STATUS_OK;
}

LoRaMacStatus_t
LoRaMacMulticastChannelUnlink(MulticastParams_t *channelParam)
{
    if (channelParam == NULL) {
        return LORAMAC_STATUS_PARAMETER_INVALID;
    }

    if ((LoRaMacState & LORAMAC_TX_RUNNING) == LORAMAC_TX_RUNNING) {
        return LORAMAC_STATUS_BUSY;
    }

    if (MulticastChannels != NULL) {
        if (MulticastChannels == channelParam) {
          // First element
          MulticastChannels = channelParam->Next;
        } else {
            MulticastParams_t *cur = MulticastChannels;

            // Search the node in the list
            while( cur->Next && cur->Next != channelParam ) {
                cur = cur->Next;
            }

            // If we found the node, remove it
            if( cur->Next ) {
                cur->Next = channelParam->Next;
            }
        }
        channelParam->Next = NULL;
    }

    return LORAMAC_STATUS_OK;
}

LoRaMacStatus_t
LoRaMacMlmeRequest(MlmeReq_t *mlmeRequest)
{
    LoRaMacStatus_t status = LORAMAC_STATUS_SERVICE_UNKNOWN;
    LoRaMacHeader_t macHdr;

    assert(mlmeRequest != NULL);

    /* If currently running return busy */
    if ((LoRaMacState & LORAMAC_TX_RUNNING) == LORAMAC_TX_RUNNING) {
        return LORAMAC_STATUS_BUSY;
    }

    /* If we are joining do not allow another MLME request */
    if (LM_F_IS_JOINING()) {
        return LORAMAC_STATUS_BUSY;
    }

    /* XXX: do these need to be set? */
    g_lora_mac_data.txpkt.port = 0;
    g_lora_mac_data.txpkt.status = LORAMAC_EVENT_INFO_STATUS_ERROR;
    g_lora_mac_data.txpkt.pkt_type = mlmeRequest->Type;

    switch (mlmeRequest->Type) {
        case MLME_JOIN:
            if ((mlmeRequest->Req.Join.DevEui == NULL) ||
                (mlmeRequest->Req.Join.AppEui == NULL) ||
                (mlmeRequest->Req.Join.AppKey == NULL) ||
                (mlmeRequest->Req.Join.NbTrials == 0)) {
                return LORAMAC_STATUS_PARAMETER_INVALID;
            }

            ResetMacParameters();

            g_lora_mac_data.cur_join_attempt = 0;
            g_lora_mac_data.max_join_attempt = mlmeRequest->Req.Join.NbTrials;

            LoRaMacDevEui = mlmeRequest->Req.Join.DevEui;
            LoRaMacAppEui = mlmeRequest->Req.Join.AppEui;
            LoRaMacAppKey = mlmeRequest->Req.Join.AppKey;
            LoRaMacParams.ChannelsDatarate =
                RegionAlternateDr(LoRaMacRegion,LoRaMacParams.ChannelsDatarate);

            /* Set flag to denote we are trying to join */
            LM_F_IS_JOINING() = 1;

            macHdr.Value = 0;
            macHdr.Bits.MType  = FRAME_TYPE_JOIN_REQ;
            status = Send(&macHdr, 0, NULL);
            if (status != LORAMAC_STATUS_OK) {
                LM_F_IS_JOINING() = 0;
            }
            break;
        case MLME_LINK_CHECK:
            // LoRaMac will send this command piggy-back.
            status = AddMacCommand(MOTE_MAC_LINK_CHECK_REQ, 0, 0);
            if (status == LORAMAC_STATUS_OK) {
                STATS_INC(lora_mac_stats, link_chk_tx);
            }
            break;
        case MLME_TXCW:
            /* XXX: make sure we can handle this. Set TX_RUNNING? */
            status = SetTxContinuousWave(mlmeRequest->Req.TxCw.Timeout);
            break;
        default:
            status = LORAMAC_STATUS_SERVICE_UNKNOWN;
            break;
    }

    return status;
}

LoRaMacStatus_t
LoRaMacMcpsRequest(struct os_mbuf *om, struct lora_pkt_info *txi)
{
    LoRaMacStatus_t status;
    GetPhyParams_t getPhy;
    PhyParam_t phyParam;
    LoRaMacHeader_t macHdr;
    VerifyParams_t verify;

    assert(txi != NULL);

    if (LM_F_IS_JOINING() || (lora_mac_tx_state() == LORAMAC_STATUS_BUSY)) {
        return LORAMAC_STATUS_BUSY;
    }

    macHdr.Value = 0;

    /*
     * ack timeout retries counter must be reset every time a new request
     * (unconfirmed or confirmed) is performed.
     */
    g_lora_mac_data.ack_timeout_retries_cntr = 1;

    switch (txi->pkt_type) {
        case MCPS_UNCONFIRMED:
            g_lora_mac_data.ack_timeout_retries = 1;
            macHdr.Bits.MType = FRAME_TYPE_DATA_UNCONFIRMED_UP;
            break;
        case MCPS_CONFIRMED:
            /* The retries field is seeded with trials, then cleared */
            g_lora_mac_data.ack_timeout_retries = txi->txdinfo.retries;
            if (g_lora_mac_data.ack_timeout_retries > MAX_ACK_RETRIES) {
                g_lora_mac_data.ack_timeout_retries = MAX_ACK_RETRIES;
            }

            macHdr.Bits.MType = FRAME_TYPE_DATA_CONFIRMED_UP;
            break;
        case MCPS_PROPRIETARY:
            g_lora_mac_data.ack_timeout_retries = 1;
            macHdr.Bits.MType = FRAME_TYPE_PROPRIETARY;
            break;
        default:
            assert(0);
            break;
    }

    /* Default packet status to error; will get set if ok later */
    txi->status = LORAMAC_EVENT_INFO_STATUS_ERROR;

    if (AdrCtrlOn == false) {
        /* Verify data rate valid */
        getPhy.Attribute = PHY_MIN_TX_DR;
        getPhy.UplinkDwellTime = LoRaMacParams.UplinkDwellTime;
        phyParam = RegionGetPhyParam(LoRaMacRegion, &getPhy);
        verify.DatarateParams.Datarate =
            MAX(LoRaMacParams.ChannelsDatarate, phyParam.Value);
        verify.DatarateParams.UplinkDwellTime = LoRaMacParams.UplinkDwellTime;

        if (RegionVerify(LoRaMacRegion, &verify, PHY_TX_DR) == true) {
            LoRaMacParams.ChannelsDatarate = verify.DatarateParams.Datarate;
        } else {
            return LORAMAC_STATUS_PARAMETER_INVALID;
        }
    }

    /* Clear out all transmitted information. */
    memset(&txi->txdinfo, 0, sizeof(struct lora_txd_info));
    txi->txdinfo.uplink_cntr = g_lora_mac_data.uplink_cntr;

    /* Set flag denoting we are sending a MCPS data packet */
    LM_F_IS_MCPS_REQ() = 1;

    /* Attempt to send. If failed clear flags */
    status = Send(&macHdr, txi->port, om);
    if (status != LORAMAC_STATUS_OK) {
        LM_F_NODE_ACK_REQ() = 0;
        LM_F_IS_MCPS_REQ() = 0;
    }

    return status;
}

void LoRaMacTestRxWindowsOn( bool enable )
{
}

void LoRaMacTestSetMic( uint16_t txPacketCounter )
{
    g_lora_mac_data.uplink_cntr = txPacketCounter;
    IsUpLinkCounterFixed = true;
}

void LoRaMacTestSetDutyCycleOn( bool enable )
{
    VerifyParams_t verify;

    verify.DutyCycle = enable;
    if (RegionVerify( LoRaMacRegion, &verify, PHY_DUTY_CYCLE ) == true) {
        DutyCycleOn = enable;
    }
}

void LoRaMacTestSetChannel( uint8_t channel )
{
    g_lora_mac_data.cur_chan = channel;
}

LoRaMacStatus_t
lora_mac_tx_state(void)
{
    LoRaMacStatus_t rc;

    if (((LoRaMacState & LORAMAC_TX_RUNNING) == LORAMAC_TX_RUNNING) ||
        ((LoRaMacState & LORAMAC_TX_DELAYED) == LORAMAC_TX_DELAYED)) {
        rc = LORAMAC_STATUS_BUSY;
    } else {
        rc = LORAMAC_STATUS_OK;
    }

    return rc;
}

bool
lora_mac_srv_ack_requested(void)
{
    return LM_F_GW_ACK_REQ() ? true : false;
}

/**
 * Returns the number of bytes of MAC commands to send.
 *
 * @return uint8_t Number of bytes of MAC commands that need to be sent
 */
uint8_t
lora_mac_cmd_buffer_len(void)
{
    return MacCommandsBufferIndex + MacCommandsBufferToRepeatIndex;
}

/* Extract only the mac commands that will fit */
static uint8_t
lora_mac_extract_mac_cmds(uint8_t max_cmd_bytes, uint8_t *buf)
{
    uint8_t cmd;
    uint8_t i;
    uint8_t bytes_added;
    uint8_t bytes_left;
    uint8_t cmd_len;

    i = 0;
    bytes_left = MacCommandsBufferIndex;
    bytes_added = 0;
    while (bytes_left != 0) {
        /*
         * Determine how many bytes are needed for the given MAC command. We
         * will always send them in order and if we cannot fit we stop.
         */
        cmd = MacCommandsBuffer[i];
        assert((cmd <= MOTE_MAC_RX_TIMING_SETUP_ANS) &&
               (cmd >= MOTE_MAC_LINK_CHECK_REQ));

        /* Get length of this command */
        cmd_len = g_lora_mac_cmd_lens[cmd];

        /* Make sure we can add this to the messgae */
        if ((cmd_len + bytes_added) > max_cmd_bytes) {
            break;
        }

        /*
         * There had better be enough room in the buffer! If not, just clear
         * the remaining bytes in the buffer
         */
        assert(cmd_len <= bytes_left);

        /* copy bytes into buffer */
        memcpy(buf, &MacCommandsBuffer[i], cmd_len);
        bytes_added += cmd_len;
        bytes_left -= cmd_len;
        buf += cmd_len;
        i += cmd_len;
    }

    return bytes_added;
}
