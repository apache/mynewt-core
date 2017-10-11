/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
    (C)2013 Semtech
 ___ _____ _   ___ _  _____ ___  ___  ___ ___
/ __|_   _/_\ / __| |/ / __/ _ \| _ \/ __| __|
\__ \ | |/ _ \ (__| ' <| _| (_) |   / (__| _|
|___/ |_/_/ \_\___|_|\_\_| \___/|_|_\\___|___|
embedded.connectivity.solutions===============

Description: LoRa MAC layer implementation

License: Revised BSD License, see LICENSE.TXT file include in the project

Maintainer: Miguel Luis ( Semtech ), Gregory Cristian ( Semtech ) and Daniel Jäckle ( STACKFORCE )
*/

#include <string.h>
#include <assert.h>
#include "node/radio.h"
#include "node/lora.h"
#include "radio/radio.h"
#include "node/utilities.h"
#include "node/mac/LoRaMacCrypto.h"
#include "node/mac/LoRaMac.h"
#include "node/mac/LoRaMacTest.h"
#include "os/os.h"
#include "hal/hal_timer.h"
#include "node/lora_priv.h"

#if MYNEWT_VAL(LORA_MAC_TIMER_NUM) == -1
#error "Must define a Lora MAC timer number"
#else
#define LORA_MAC_TIMER_NUM    MYNEWT_VAL(LORA_MAC_TIMER_NUM)
#endif

/* The lora mac timer counts in 1 usec increments */
#define LORA_MAC_TIMER_FREQ     1000000

/* Convert mac state timeout to os ticks */
#define MAC_STATE_CHECK_OS_TICKS        \
    ((MAC_STATE_CHECK_TIMEOUT * OS_TICKS_PER_SEC) / 1000)

/*!
 * Maximum PHY layer payload size
 */
#define LORAMAC_PHY_MAXPAYLOAD                      255

/*!
 * Maximum MAC commands buffer size
 */
#define LORA_MAC_COMMAND_MAX_LENGTH                 15

/*!
 * FRMPayload overhead to be used when setting the Radio.SetMaxPayloadLength
 * in RxWindowSetup function.
 * Maximum PHYPayload = MaxPayloadOfDatarate/MaxPayloadOfDatarateRepeater + LORA_MAC_FRMPAYLOAD_OVERHEAD
 */
#define LORA_MAC_FRMPAYLOAD_OVERHEAD                13 // MHDR(1) + FHDR(7) + Port(1) + MIC(4)

/*!
 * LoRaMac duty cycle for the back-off procedure during the first hour.
 */
#define BACKOFF_DC_1_HOUR                           100

/*!
 * LoRaMac duty cycle for the back-off procedure during the next 10 hours.
 */
#define BACKOFF_DC_10_HOURS                         1000

/*!
 * LoRaMac duty cycle for the back-off procedure during the next 24 hours.
 */
#define BACKOFF_DC_24_HOURS                         10000

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
 * Device nonce is a random value extracted by issuing a sequence of RSSI
 * measurements
 */
static uint16_t LoRaMacDevNonce;

/*!
 * Network ID ( 3 bytes )
 */
static uint32_t LoRaMacNetID;

/*!
 * Mote Address
 */
static uint32_t LoRaMacDevAddr;

/*!
 * Multicast channels linked list
 */
static MulticastParams_t *MulticastChannels = NULL;

/*!
 * Actual device class
 */
static DeviceClass_t LoRaMacDeviceClass;

/*!
 * Indicates if the node is connected to a private or public network
 */
static bool PublicNetwork;

/*!
 * Indicates if the node supports repeaters
 */
static bool RepeaterSupport;

/*!
 * Buffer containing the data to be sent or received.
 */
static uint8_t LoRaMacBuffer[LORAMAC_PHY_MAXPAYLOAD];

/*!
 * Length of packet in LoRaMacBuffer
 */
static uint16_t LoRaMacBufferPktLen;

/*!
 * Length of the payload in LoRaMacBuffer
 */
static uint8_t LoRaMacTxPayloadLen;

/*!
 * Buffer containing the upper layer data.
 */
static uint8_t LoRaMacPayload[LORAMAC_PHY_MAXPAYLOAD];
static uint8_t LoRaMacRxPayload[LORAMAC_PHY_MAXPAYLOAD];

/*!
 * LoRaMAC frame counter. Each time a packet is sent the counter is incremented.
 * Only the 16 LSB bits are sent
 */
static uint32_t UpLinkCounter;

/*!
 * LoRaMAC frame counter. Each time a packet is received the counter is incremented.
 * Only the 16 LSB bits are received
 */
static uint32_t DownLinkCounter;

/*!
 * IsPacketCounterFixed enables the MIC field tests by fixing the
 * UpLinkCounter value
 */
static bool IsUpLinkCounterFixed = false;

/*!
 * Indicates if the MAC layer has already joined a network.
 */
static bool IsLoRaMacNetworkJoined = false;

/*!
 * LoRaMac ADR control status
 */
static bool AdrCtrlOn = false;

/*!
 * Counts the number of missed ADR acknowledgements
 */
static uint32_t AdrAckCounter;

/*!
 * If the node has sent a FRAME_TYPE_DATA_CONFIRMED_UP this variable indicates
 * if the nodes needs to manage the server acknowledgement.
 */
static bool NodeAckRequested = false;

/*!
 * If the server has sent a FRAME_TYPE_DATA_CONFIRMED_DOWN this variable indicates
 * if the ACK bit must be set for the next transmission
 */
static bool SrvAckRequested = false;

/*!
 * Indicates if the MAC layer wants to send MAC commands
 */
static bool MacCommandsInNextTx = false;

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
static uint8_t MacCommandsBuffer[LORA_MAC_COMMAND_MAX_LENGTH];

/*!
 * Buffer containing the MAC layer commands which must be repeated
 */
static uint8_t MacCommandsBufferToRepeat[LORA_MAC_COMMAND_MAX_LENGTH];

#if defined( USE_BAND_433 )
/*!
 * Data rates table definition
 */
const uint8_t Datarates[]  = { 12, 11, 10,  9,  8,  7,  7, 50 };

/*!
 * Maximum payload with respect to the datarate index. Cannot operate with repeater.
 */
const uint8_t MaxPayloadOfDatarate[] = { 51, 51, 51, 115, 242, 242, 242, 242 };

/*!
 * Maximum payload with respect to the datarate index. Can operate with repeater.
 */
const uint8_t MaxPayloadOfDatarateRepeater[] = { 51, 51, 51, 115, 222, 222, 222, 222 };

/*!
 * Tx output powers table definition
 */
const int8_t TxPowers[]    = { 10, 7, 4, 1, -2, -5 };

/*!
 * LoRaMac bands
 */
static Band_t Bands[LORA_MAX_NB_BANDS] =
{
    BAND0,
};

/*!
 * LoRaMAC channels
 */
static ChannelParams_t Channels[LORA_MAX_NB_CHANNELS] =
{
    LC1,
    LC2,
    LC3,
};
#elif defined( USE_BAND_470 )

/*!
 * Data rates table definition
 */
const uint8_t Datarates[]  = { 12, 11, 10,  9,  8,  7 };

/*!
 * Maximum payload with respect to the datarate index. Cannot operate with repeater.
 */
const uint8_t MaxPayloadOfDatarate[] = { 51, 51, 51, 115, 222, 222 };

/*!
 * Maximum payload with respect to the datarate index. Can operate with repeater.
 */
const uint8_t MaxPayloadOfDatarateRepeater[] = { 51, 51, 51, 115, 222, 222 };

/*!
 * Tx output powers table definition
 */
const int8_t TxPowers[]    = { 17, 16, 14, 12, 10, 7, 5, 2 };

/*!
 * LoRaMac bands
 */
static Band_t Bands[LORA_MAX_NB_BANDS] =
{
    BAND0,
};

/*!
 * LoRaMAC channels
 */
static ChannelParams_t Channels[LORA_MAX_NB_CHANNELS];

/*!
 * Defines the first channel for RX window 1 for CN470 band
 */
#define LORAMAC_FIRST_RX1_CHANNEL           ( (uint32_t) 500.3e6 )

/*!
 * Defines the last channel for RX window 1 for CN470 band
 */
#define LORAMAC_LAST_RX1_CHANNEL            ( (uint32_t) 509.7e6 )

/*!
 * Defines the step width of the channels for RX window 1
 */
#define LORAMAC_STEPWIDTH_RX1_CHANNEL       ( (uint32_t) 200e3 )

#elif defined( USE_BAND_780 )
/*!
 * Data rates table definition
 */
const uint8_t Datarates[]  = { 12, 11, 10,  9,  8,  7,  7, 50 };

/*!
 * Maximum payload with respect to the datarate index. Cannot operate with repeater.
 */
const uint8_t MaxPayloadOfDatarate[] = { 51, 51, 51, 115, 242, 242, 242, 242 };

/*!
 * Maximum payload with respect to the datarate index. Can operate with repeater.
 */
const uint8_t MaxPayloadOfDatarateRepeater[] = { 51, 51, 51, 115, 222, 222, 222, 222 };

/*!
 * Tx output powers table definition
 */
const int8_t TxPowers[]    = { 10, 7, 4, 1, -2, -5 };

/*!
 * LoRaMac bands
 */
static Band_t Bands[LORA_MAX_NB_BANDS] =
{
    BAND0,
};

/*!
 * LoRaMAC channels
 */
static ChannelParams_t Channels[LORA_MAX_NB_CHANNELS] =
{
    LC1,
    LC2,
    LC3,
};
#elif defined( USE_BAND_868 )
/*!
 * Data rates table definition
 */
const uint8_t Datarates[]  = { 12, 11, 10,  9,  8,  7,  7, 50 };

/*!
 * Maximum payload with respect to the datarate index. Cannot operate with repeater.
 */
const uint8_t MaxPayloadOfDatarate[] = { 51, 51, 51, 115, 242, 242, 242, 242 };

/*!
 * Maximum payload with respect to the datarate index. Can operate with repeater.
 */
const uint8_t MaxPayloadOfDatarateRepeater[] = { 51, 51, 51, 115, 222, 222, 222, 222 };

/*!
 * Tx output powers table definition
 */
const int8_t TxPowers[]    = { 20, 14, 11,  8,  5,  2 };

/*!
 * LoRaMac bands
 */
static Band_t Bands[LORA_MAX_NB_BANDS] =
{
    BAND0,
    BAND1,
    BAND2,
    BAND3,
    BAND4,
};

/*!
 * LoRaMAC channels
 */
static ChannelParams_t Channels[LORA_MAX_NB_CHANNELS] =
{
    LC1,
    LC2,
    LC3,
};
#elif defined( USE_BAND_915 ) || defined( USE_BAND_915_HYBRID )
/*!
 * Data rates table definition
 */
const uint8_t Datarates[]  = { 10, 9, 8,  7,  8,  0,  0, 0, 12, 11, 10, 9, 8, 7, 0, 0 };

/*!
 * Up/Down link data rates offset definition
 */
const int8_t datarateOffsets[5][4] =
{
    { DR_10, DR_9 , DR_8 , DR_8  }, // DR_0
    { DR_11, DR_10, DR_9 , DR_8  }, // DR_1
    { DR_12, DR_11, DR_10, DR_9  }, // DR_2
    { DR_13, DR_12, DR_11, DR_10 }, // DR_3
    { DR_13, DR_13, DR_12, DR_11 }, // DR_4
};

/*!
 * Maximum payload with respect to the datarate index. Cannot operate with repeater.
 */
const uint8_t MaxPayloadOfDatarate[] = { 11, 53, 125, 242, 242, 0, 0, 0, 53, 129, 242, 242, 242, 242, 0, 0 };

/*!
 * Maximum payload with respect to the datarate index. Can operate with repeater.
 */
const uint8_t MaxPayloadOfDatarateRepeater[] = { 11, 53, 125, 242, 242, 0, 0, 0, 33, 109, 222, 222, 222, 222, 0, 0 };

#if (defined(USE_BAND_915) || defined(USE_BAND_915_HYBRID))
/* XXX: only looked at these for the US band */
/* (incoded) Symbol times, in usecs, for data rates */
const uint16_t g_lora_uncoded_symbol_len_usecs[] =
{
    8192, 4096, 2048, 1024, 512, 0, 0, 0, 8192, 4096, 2048, 1024, 512, 256, 0, 0
};
#endif

/*!
 * Tx output powers table definition
 */
const int8_t TxPowers[]    = { 30, 28, 26, 24, 22, 20, 18, 16, 14, 12, 10 };

/*!
 * LoRaMac bands
 */
static Band_t Bands[LORA_MAX_NB_BANDS] =
{
    BAND0,
};

/*!
 * LoRaMAC channels
 */
static ChannelParams_t Channels[LORA_MAX_NB_CHANNELS];

/*!
 * Contains the channels which remain to be applied.
 */
static uint16_t ChannelsMaskRemaining[6];

/*!
 * Defines the first channel for RX window 1 for US band
 */
#define LORAMAC_FIRST_RX1_CHANNEL           ( (uint32_t) 923.3e6 )

/*!
 * Defines the last channel for RX window 1 for US band
 */
#define LORAMAC_LAST_RX1_CHANNEL            ( (uint32_t) 927.5e6 )

/*!
 * Defines the step width of the channels for RX window 1
 */
#define LORAMAC_STEPWIDTH_RX1_CHANNEL       ( (uint32_t) 600e3 )

#else
    #error "Please define a frequency band in the compiler options."
#endif

/*!
 * LoRaMac parameters
 */
LoRaMacParams_t LoRaMacParams;

/*!
 * LoRaMac default parameters
 */
LoRaMacParams_t LoRaMacParamsDefaults;

/*!
 * Uplink messages repetitions counter
 */
static uint8_t ChannelsNbRepCounter = 0;

/*!
 * Maximum duty cycle
 * \remark Possibility to shutdown the device.
 */
static uint8_t MaxDCycle = 0;

/*!
 * Aggregated duty cycle management
 */
static uint16_t AggregatedDCycle;
static uint32_t AggregatedLastTxDoneTime;
static uint32_t AggregatedTimeOff;

/*!
 * Enables/Disables duty cycle management (Test only)
 */
static bool DutyCycleOn;

/*!
 * Current channel index
 */
static uint8_t Channel;

/*!
 * Stores the time at LoRaMac initialization.
 *
 * NOTE: units of this variable are in usecs.
 *
 * \remark Used for the BACKOFF_DC computation.
 */
static uint64_t LoRaMacInitializationTime;

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
};

/*!
 * LoRaMac internal state
 */
uint32_t LoRaMacState;

/*!
 * LoRaMac upper layer event functions
 */
static LoRaMacPrimitives_t *LoRaMacPrimitives;

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
 * LoRaMac reception windows delay
 * \remark normal frame: RxWindowXDelay = ReceiveDelayX - RADIO_WAKEUP_TIME
 *         join frame  : RxWindowXDelay = JoinAcceptDelayX - RADIO_WAKEUP_TIME
 */
static uint32_t RxWindow1Delay;
static uint32_t RxWindow2Delay;

/*!
 * Acknowledge timeout timer. Used for packet retransmissions.
 */
static struct hal_timer AckTimeoutTimer;

/*!
 * Number of trials to get a frame acknowledged
 */
static uint8_t AckTimeoutRetries = 1;

/*!
 * Number of trials to get a frame acknowledged
 */
static uint8_t AckTimeoutRetriesCounter = 1;

/*!
 * Last transmission time on air
 */
uint32_t TxTimeOnAir = 0;

/*!
 * Number of trials for the Join Request
 */
static uint8_t JoinRequestTrials;

/*!
 * Maximum number of trials for the Join Request
 */
static uint8_t MaxJoinRequestTrials;

/*!
 * Structure to hold an MCPS indication data.
 */
static McpsIndication_t McpsIndication;

/*!
 * Structure to hold MCPS confirm data.
 */
static McpsConfirm_t McpsConfirm;

/*!
 * Structure to hold MLME confirm data.
 */
static MlmeConfirm_t MlmeConfirm;

/*!
 * Holds the current rx window slot
 */
static uint8_t RxSlot;

/*!
 * LoRaMac tx/rx operation state
 */
LoRaMacFlags_t LoRaMacFlags;

/* Radio events */
struct os_event g_lora_mac_radio_tx_timeout_event;
struct os_event g_lora_mac_radio_tx_event;
struct os_event g_lora_mac_radio_rx_event;
struct os_event g_lora_mac_radio_rx_err_event;
struct os_event g_lora_mac_radio_rx_timeout_event;
struct os_event g_lora_mac_ack_timeout_event;
struct os_event g_lora_mac_rx_win1_event;
struct os_event g_lora_mac_rx_win2_event;
struct os_event g_lora_mac_tx_delay_timeout_event;

static void lora_mac_rx_on_window2(bool rx_continuous);

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
    lora_node_log(LORA_NODE_LOG_RX_DONE, Channel, size, 0);
    os_eventq_put(lora_node_mac_evq_get(), &g_lora_mac_radio_rx_event);

    /*
      * XXX: for class C devices we may need to handle this differently as
      * the device is continuously listening I believe and it may be possible
      * to get a receive done event before the previous one has been handled.
      */
    /* The ISR fills out the payload pointer, size, rssi and snr of rx pdu */
    McpsIndication.Rssi = rssi;
    McpsIndication.Snr = snr;
    McpsIndication.Buffer = payload;
    McpsIndication.BufferSize = size;
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
 * \brief Function executed on AckTimeout timer event
 */
static void
OnAckTimeoutTimerEvent(void *unused)
{
    os_eventq_put(lora_node_mac_evq_get(), &g_lora_mac_ack_timeout_event);
}

/*!
 * \brief Searches and set the next random available channel
 *
 * \param [OUT] Time to wait for the next transmission according to the duty
 *              cycle.
 *
 * \retval status  Function status [1: OK, 0: Unable to find a channel on the
 *                                  current datarate]
 */
static bool SetNextChannel( uint32_t* time );

/*!
 * \brief Initializes and opens the reception window
 *
 * \param [IN] freq window channel frequency
 * \param [IN] datarate window channel datarate
 * \param [IN] bandwidth window channel bandwidth
 * \param [IN] timeout window channel timeout
 *
 * \retval status Operation status [true: Success, false: Fail]
 */
static bool RxWindowSetup( uint32_t freq, int8_t datarate, uint32_t bandwidth, uint16_t timeout, bool rxContinuous );

/*!
 * \brief Verifies if the RX window 2 frequency is in range
 *
 * \param [IN] freq window channel frequency
 *
 * \retval status  Function status [1: OK, 0: Frequency not applicable]
 */
static bool Rx2FreqInRange( uint32_t freq );

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
static bool ValidatePayloadLength( uint8_t lenN, int8_t datarate, uint8_t fOptsLen );

/*!
 * \brief Counts the number of bits in a mask.
 *
 * \param [IN] mask A mask from which the function counts the active bits.
 * \param [IN] nbBits The number of bits to check.
 *
 * \retval Number of enabled bits in the mask.
 */
static uint8_t CountBits( uint16_t mask, uint8_t nbBits );

#if defined( USE_BAND_915 ) || defined( USE_BAND_915_HYBRID )
/*!
 * \brief Counts the number of enabled 125 kHz channels in the channel mask.
 *        This function can only be applied to US915 band.
 *
 * \param [IN] channelsMask Pointer to the first element of the channel mask
 *
 * \retval Number of enabled channels in the channel mask
 */
static uint8_t CountNbEnabled125kHzChannels( uint16_t *channelsMask );

#if defined( USE_BAND_915_HYBRID )
/*!
 * \brief Validates the correctness of the channel mask for US915, hybrid mode.
 *
 * \param [IN] mask Block definition to set.
 * \param [OUT] channelsMask Pointer to the first element of the channel mask
 */
static void ReenableChannels( uint16_t mask, uint16_t* channelsMask );

/*!
 * \brief Validates the correctness of the channel mask for US915, hybrid mode.
 *
 * \param [IN] channelsMask Pointer to the first element of the channel mask
 *
 * \retval [true: channel mask correct, false: channel mask not correct]
 */
static bool ValidateChannelMask( uint16_t* channelsMask );
#endif

#endif

/*!
 * \brief Validates the correctness of the datarate against the enable channels.
 *
 * \param [IN] datarate Datarate to be check
 * \param [IN] channelsMask Pointer to the first element of the channel mask
 *
 * \retval [true: datarate can be used, false: datarate can not be used]
 */
static bool ValidateDatarate( int8_t datarate, uint16_t* channelsMask );

/*!
 * \brief Limits the Tx power according to the number of enabled channels
 *
 * \param [IN] txPower txPower to limit
 * \param [IN] maxBandTxPower Maximum band allowed TxPower
 *
 * \retval Returns the maximum valid tx power
 */
static int8_t LimitTxPower( int8_t txPower, int8_t maxBandTxPower );

/*!
 * \brief Verifies, if a value is in a given range.
 *
 * \param value Value to verify, if it is in range
 *
 * \param min Minimum possible value
 *
 * \param max Maximum possible value
 *
 * \retval Returns the maximum valid tx power
 */
static bool ValueInRange( int8_t value, int8_t min, int8_t max );

/*!
 * \brief Calculates the next datarate to set, when ADR is on or off
 *
 * \param [IN] adrEnabled Specify whether ADR is on or off
 *
 * \param [IN] updateChannelMask Set to true, if the channel masks shall be updated
 *
 * \param [OUT] datarateOut Reports the datarate which will be used next
 *
 * \retval Returns the state of ADR ack request
 */
static bool AdrNextDr( bool adrEnabled, bool updateChannelMask, int8_t* datarateOut );

/*!
 * \brief Disables channel in a specified channel mask
 *
 * \param [IN] id - Id of the channel
 *
 * \param [IN] mask - Pointer to the channel mask to edit
 *
 * \retval [true, if disable was successful, false if not]
 */
static bool DisableChannelInMask( uint8_t id, uint16_t* mask );

/*!
 * \brief Decodes MAC commands in the fOpts field and in the payload
 */
static void ProcessMacCommands( uint8_t *payload, uint8_t macIndex, uint8_t commandsSize, uint8_t snr );

/*!
 * \brief LoRaMAC layer generic send frame
 *
 * \param [IN] macHdr      MAC header field
 * \param [IN] fPort       MAC payload port
 * \param [IN] fBuffer     MAC data buffer to be sent
 * \param [IN] fBufferSize MAC data buffer size
 * \retval status          Status of the operation.
 */
LoRaMacStatus_t Send( LoRaMacHeader_t *macHdr, uint8_t fPort, void *fBuffer, uint16_t fBufferSize );

/*!
 * \brief LoRaMAC layer frame buffer initialization
 *
 * \param [IN] macHdr      MAC header field
 * \param [IN] fCtrl       MAC frame control field
 * \param [IN] fOpts       MAC commands buffer
 * \param [IN] fPort       MAC payload port
 * \param [IN] fBuffer     MAC data buffer to be sent
 * \param [IN] fBufferSize MAC data buffer size
 * \retval status          Status of the operation.
 */
LoRaMacStatus_t PrepareFrame( LoRaMacHeader_t *macHdr, LoRaMacFrameCtrl_t *fCtrl, uint8_t fPort, void *fBuffer, uint16_t fBufferSize );

/*
 * \brief Schedules the frame according to the duty cycle
 *
 * \retval Status of the operation
 */
static LoRaMacStatus_t ScheduleTx( void );

/*
 * \brief Sets the duty cycle for the join procedure.
 *
 * \retval Duty cycle
 */
static uint16_t JoinDutyCycle( void );

/*
 * \brief Calculates the back-off time for the band of a channel.
 *
 * \param [IN] channel     The last Tx channel index
 */
static void CalculateBackOff( uint8_t channel );

/*
 * \brief Alternates the datarate of the channel for the join request.
 *
 * \param [IN] nbTrials    Number of performed join requests.
 * \retval Datarate to apply
 */
static int8_t AlternateDatarate( uint16_t nbTrials );

/*!
 * \brief LoRaMAC layer prepared frame buffer transmission with channel specification
 *
 * \remark PrepareFrame must be called at least once before calling this
 *         function.
 *
 * \param [IN] channel     Channel parameters
 * \retval status          Status of the operation.
 */
LoRaMacStatus_t SendFrameOnChannel( ChannelParams_t channel );

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
 * \brief Resets MAC specific parameters to default
 */
static void ResetMacParameters(void);

/*
 * XXX: TODO
 *
 * Need to understand how to handle mac commands that are waiting to be
 * transmitted. We do not want to constantly transmit them but if we dont
 * get a downlink packet for commands that we need to repeat until we hear
 * a downlink.
 */

/**
 * Checks to see if there are additional transmissions that need to occur.
 * If so, we restart the transmit queue timer to process them.
 */
static void
lora_mac_chk_kickstart_tx(void)
{
    if (!lora_node_txq_empty()) {
        lora_node_reset_txq_timer();
    }
}

/**
 * Called to send MCPS confirmations
 */
static void
lora_mac_send_mcps_confirm(LoRaMacEventInfoStatus_t status)
{
    /* We are no longer running a tx service */
    LoRaMacState &= ~LORAMAC_TX_RUNNING;
    if (LoRaMacFlags.Bits.McpsReq == 1) {
        McpsConfirm.Status = status;
        LoRaMacPrimitives->MacMcpsConfirm( &McpsConfirm );
        LoRaMacFlags.Bits.McpsReq = 0;
    }
    lora_mac_chk_kickstart_tx();
}

/**
 * Called to send MLME confirmations
 */
static void
lora_mac_send_mlme_confirm(LoRaMacEventInfoStatus_t status)
{
    /* We are no longer running a tx service */
    if (MlmeConfirm.MlmeRequest == MLME_JOIN) {
        LoRaMacState &= ~LORAMAC_TX_RUNNING;
    }

    MlmeConfirm.Status = status;
    LoRaMacPrimitives->MacMlmeConfirm( &MlmeConfirm );
    LoRaMacFlags.Bits.MlmeReq = 0;
}

static void
lora_mac_confirmed_tx_fail(void)
{
    STATS_INC(lora_mac_stats, confirmed_tx_fail);

    if (AckTimeoutRetriesCounter < AckTimeoutRetries) {
        AckTimeoutRetriesCounter++;
        if ((AckTimeoutRetriesCounter % 2) == 1) {
            LoRaMacParams.ChannelsDatarate = MAX( LoRaMacParams.ChannelsDatarate - 1, LORAMAC_TX_MIN_DATARATE );
        }

        if( ValidatePayloadLength(LoRaMacTxPayloadLen, LoRaMacParams.ChannelsDatarate, MacCommandsBufferIndex) == true)
        {
            // Sends the same frame again
            ScheduleTx( );
        } else {
            // The DR is not applicable for the payload size
            NodeAckRequested = false;
            McpsConfirm.NbRetries = AckTimeoutRetriesCounter;
            McpsConfirm.Datarate = LoRaMacParams.ChannelsDatarate;
            UpLinkCounter++;
            lora_mac_send_mcps_confirm(LORAMAC_EVENT_INFO_STATUS_TX_DR_PAYLOAD_SIZE_ERROR);
        }
    } else {
#if defined( USE_BAND_433 ) || defined( USE_BAND_780 ) || defined( USE_BAND_868 )
        // Re-enable default channels LC1, LC2, LC3
        LoRaMacParams.ChannelsMask[0] = LoRaMacParams.ChannelsMask[0] | ( LC( 1 ) + LC( 2 ) + LC( 3 ) );
#elif defined( USE_BAND_470 )
        // Re-enable default channels
        memcpy( ( uint8_t* )LoRaMacParams.ChannelsMask, ( uint8_t* )LoRaMacParamsDefaults.ChannelsMask, sizeof( LoRaMacParams.ChannelsMask ) );
#elif defined( USE_BAND_915 )
        // Re-enable default channels
        memcpy( ( uint8_t* )LoRaMacParams.ChannelsMask, ( uint8_t* )LoRaMacParamsDefaults.ChannelsMask, sizeof( LoRaMacParams.ChannelsMask ) );
#elif defined( USE_BAND_915_HYBRID )
        // Re-enable default channels
        ReenableChannels( LoRaMacParamsDefaults.ChannelsMask[4], LoRaMacParams.ChannelsMask );
#else
#error "Please define a frequency band in the compiler options."
#endif
        UpLinkCounter++;
        NodeAckRequested = false;
        McpsConfirm.NbRetries = AckTimeoutRetriesCounter;
        lora_mac_send_mcps_confirm(LORAMAC_EVENT_INFO_STATUS_TX_RETRIES_EXCEEDED);
    }
}

static void
lora_mac_confirmed_tx_success(void)
{
    /* XXX: should this be done on success? */
#if defined( USE_BAND_433 ) || defined( USE_BAND_780 ) || defined( USE_BAND_868 )
    // Re-enable default channels LC1, LC2, LC3
    LoRaMacParams.ChannelsMask[0] = LoRaMacParams.ChannelsMask[0] | ( LC( 1 ) + LC( 2 ) + LC( 3 ) );
#elif defined( USE_BAND_470 )
    // Re-enable default channels
    memcpy( ( uint8_t* )LoRaMacParams.ChannelsMask, ( uint8_t* )LoRaMacParamsDefaults.ChannelsMask, sizeof( LoRaMacParams.ChannelsMask ) );
#elif defined( USE_BAND_915 )
    // Re-enable default channels
    memcpy( ( uint8_t* )LoRaMacParams.ChannelsMask, ( uint8_t* )LoRaMacParamsDefaults.ChannelsMask, sizeof( LoRaMacParams.ChannelsMask ) );
#elif defined( USE_BAND_915_HYBRID )
    // Re-enable default channels
    ReenableChannels( LoRaMacParamsDefaults.ChannelsMask[4], LoRaMacParams.ChannelsMask );
#else
#error "Please define a frequency band in the compiler options."
#endif

    STATS_INC(lora_mac_stats, confirmed_tx_good);
    UpLinkCounter++;
    NodeAckRequested = false;
    McpsConfirm.AckReceived = true;
    McpsConfirm.NbRetries = AckTimeoutRetriesCounter;
    lora_mac_send_mcps_confirm(LORAMAC_EVENT_INFO_STATUS_OK);
}

static void
lora_mac_join_req_tx_fail(void)
{
    /* Have we exceeded the number of join request attempts */
    if (JoinRequestTrials >= MaxJoinRequestTrials) {
        /* Join was a failure */
        STATS_INC(lora_mac_stats, join_failures);
        MlmeConfirm.NbRetries = JoinRequestTrials;
        lora_mac_send_mlme_confirm(LORAMAC_EVENT_INFO_STATUS_JOIN_FAIL);

        /*
         * The reason we do this if failed to join is to flush the transmit
         * queue and any mac commands.
         */
        lora_mac_chk_kickstart_tx();
    } else {
        /* Add some transmit delay between join request transmissions */
        hal_timer_stop(&TxDelayedTimer);
        hal_timer_start(&TxDelayedTimer,
            randr(0, MYNEWT_VAL(LORA_JOIN_REQ_RAND_DELAY) * 1000));
    }
}

static void
lora_mac_unconfirmed_tx_done(void)
{
    STATS_INC(lora_mac_stats, unconfirmed_tx);

    /* Unconfirmed frames get repeated N times. */
    if (ChannelsNbRepCounter >= LoRaMacParams.ChannelsNbRep) {
        McpsConfirm.NbRetries = ChannelsNbRepCounter;
        ChannelsNbRepCounter = 0;
        AdrAckCounter++;
        UpLinkCounter++;
        lora_mac_send_mcps_confirm(LORAMAC_EVENT_INFO_STATUS_OK);
    } else {
       /* Add some transmit delay between unconfirmed transmissions */
        hal_timer_stop(&TxDelayedTimer);
        hal_timer_start(&TxDelayedTimer,
            randr(0, MYNEWT_VAL(LORA_UNCONFIRMED_TX_RAND_DELAY) * 1000));
    }
}

static void
lora_mac_tx_service_done(int rxd_confirmation)
{
    /* If no MLME or MCPS request in progress we just return. */
    if ((LoRaMacFlags.Bits.MlmeReq == 0) && (LoRaMacFlags.Bits.McpsReq == 0)) {
        assert((LoRaMacState & LORAMAC_TX_RUNNING) == 0);
        lora_mac_chk_kickstart_tx();
        return;
    }

    if (LoRaMacFlags.Bits.MlmeReq && (MlmeConfirm.MlmeRequest == MLME_JOIN)) {
        lora_mac_join_req_tx_fail();
    } else {
        if (LoRaMacFlags.Bits.McpsReq == 1) {
            if (NodeAckRequested) {
                if (rxd_confirmation) {
                    lora_mac_confirmed_tx_success();
                } else {
                    lora_mac_confirmed_tx_fail();
                }
            } else {
                lora_mac_unconfirmed_tx_done();
            }
        }
    }
}

static void
lora_mac_process_radio_tx(struct os_event *ev)
{
    /* XXX: We need to time this more accurately */
    uint32_t curTime = hal_timer_read(LORA_MAC_TIMER_NUM);

    if (LoRaMacDeviceClass != CLASS_C) {
        Radio.Sleep( );
    } else {
        lora_mac_rx_on_window2(true);
    }

    /* Always start receive window 1 */
    hal_timer_start_at(&RxWindowTimer1, curTime + (RxWindow1Delay * 1000));

    /* Only start receive window 2 if not a class C device */
    if (LoRaMacDeviceClass != CLASS_C) {
        hal_timer_start_at(&RxWindowTimer2,
                               curTime + (RxWindow2Delay * 1000));
    }

    if (NodeAckRequested == true) {
        hal_timer_start_at(&AckTimeoutTimer,  curTime +
                ((RxWindow2Delay + ACK_TIMEOUT +
                  randr(-ACK_TIMEOUT_RND, ACK_TIMEOUT_RND)) * 1000));
    } else {
        /*
         * For unconfirmed transmisson for class C devices,
         * we want to make sure we listen for the second rx
         * window before moving on to another transmission.
         */
        if (LoRaMacDeviceClass == CLASS_C) {
            hal_timer_start_at(&RxWindowTimer2,
                               curTime + (RxWindow2Delay * 1000));
        }
    }

    // Update last tx done time for the current channel
    Bands[Channels[Channel].Band].LastTxDoneTime = curTime;

    // Update Aggregated last tx done time
    AggregatedLastTxDoneTime = curTime;

    lora_node_log(LORA_NODE_LOG_TX_DONE, Channel, 0, curTime);

    // Update Backoff
    CalculateBackOff(Channel);

    if (NodeAckRequested == false) {
        ChannelsNbRepCounter++;
    }

    /* We increment join request transmissions here */
    if ((LoRaMacFlags.Bits.MlmeReq == 1) &&
        (MlmeConfirm.MlmeRequest == MLME_JOIN)) {
        STATS_INC(lora_mac_stats, join_req_tx);
    }
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
    bool skipIndication = false;
    bool send_indicate = false;
    int tx_service_over = 0;

    uint8_t *payload;
    uint16_t size;
    int8_t snr;

    uint8_t pktHeaderLen = 0;
    uint32_t address = 0;
    uint8_t appPayloadStartIndex = 0;
    uint8_t port = 0xFF;
    uint8_t frameLen = 0;
    uint32_t mic = 0;
    uint32_t micRx = 0;

    uint16_t sequenceCounter = 0;
    uint16_t sequenceCounterPrev = 0;
    uint16_t sequenceCounterDiff = 0;
    uint32_t downLinkCounter = 0;

    MulticastParams_t *curMulticastParams = NULL;
    uint8_t *nwkSKey = LoRaMacNwkSKey;
    uint8_t *appSKey = LoRaMacAppSKey;

    uint8_t multicast = 0;

    bool isMicOk = false;

    /* Put radio to sleep if not class C */
    if (LoRaMacDeviceClass != CLASS_C) {
        Radio.Sleep( );
    }

    STATS_INC(lora_mac_stats, rx_frames);

    /* Payload, size and snr are filled in by radio rx ISR */
    payload = McpsIndication.Buffer;
    size = McpsIndication.BufferSize;
    snr = McpsIndication.Snr;

    /* Reset rest of global indication element */
    McpsIndication.RxSlot = RxSlot;
    McpsIndication.Port = 0;
    McpsIndication.Multicast = 0;
    McpsIndication.FramePending = 0;
    McpsIndication.RxData = false;
    McpsIndication.AckReceived = false;
    McpsIndication.DownLinkCounter = 0;

    lora_node_log(LORA_NODE_LOG_RX_DONE, Channel, size, 0);

    macHdr.Value = payload[pktHeaderLen++];

    switch (macHdr.Bits.MType)
    {
        case FRAME_TYPE_JOIN_ACCEPT:
            STATS_INC(lora_mac_stats, join_accept_rx);

            if (IsLoRaMacNetworkJoined == true) {
                /* XXX: count statistic here? */
                goto process_rx_done;
            }

            /*
             * XXX: This is odd, but if we receive a join accept and we are not
             * joined but have not started the join process not sure what to
             * do. Guess we will just ignore this packet.
             */
            if ((LoRaMacFlags.Bits.MlmeReq == 0) ||
                (MlmeConfirm.MlmeRequest != MLME_JOIN)) {
                goto process_rx_done;
            }

            LoRaMacJoinDecrypt( payload + 1, size - 1, LoRaMacAppKey, LoRaMacRxPayload + 1 );

            LoRaMacRxPayload[0] = macHdr.Value;

            LoRaMacJoinComputeMic( LoRaMacRxPayload, size - LORAMAC_MFR_LEN, LoRaMacAppKey, &mic );

            micRx |= ( uint32_t )LoRaMacRxPayload[size - LORAMAC_MFR_LEN];
            micRx |= ( ( uint32_t )LoRaMacRxPayload[size - LORAMAC_MFR_LEN + 1] << 8 );
            micRx |= ( ( uint32_t )LoRaMacRxPayload[size - LORAMAC_MFR_LEN + 2] << 16 );
            micRx |= ( ( uint32_t )LoRaMacRxPayload[size - LORAMAC_MFR_LEN + 3] << 24 );

            if (micRx == mic) {
                LoRaMacJoinComputeSKeys( LoRaMacAppKey, LoRaMacRxPayload + 1, LoRaMacDevNonce, LoRaMacNwkSKey, LoRaMacAppSKey );

                LoRaMacNetID = ( uint32_t )LoRaMacRxPayload[4];
                LoRaMacNetID |= ( ( uint32_t )LoRaMacRxPayload[5] << 8 );
                LoRaMacNetID |= ( ( uint32_t )LoRaMacRxPayload[6] << 16 );

                LoRaMacDevAddr = ( uint32_t )LoRaMacRxPayload[7];
                LoRaMacDevAddr |= ( ( uint32_t )LoRaMacRxPayload[8] << 8 );
                LoRaMacDevAddr |= ( ( uint32_t )LoRaMacRxPayload[9] << 16 );
                LoRaMacDevAddr |= ( ( uint32_t )LoRaMacRxPayload[10] << 24 );

                // DLSettings
                LoRaMacParams.Rx1DrOffset = ( LoRaMacRxPayload[11] >> 4 ) & 0x07;
                LoRaMacParams.Rx2Channel.Datarate = LoRaMacRxPayload[11] & 0x0F;

                // RxDelay
                LoRaMacParams.ReceiveDelay1 = ( LoRaMacRxPayload[12] & 0x0F );
                if (LoRaMacParams.ReceiveDelay1 == 0) {
                    LoRaMacParams.ReceiveDelay1 = 1;
                }
                LoRaMacParams.ReceiveDelay1 *= 1e3;
                LoRaMacParams.ReceiveDelay2 = LoRaMacParams.ReceiveDelay1 + 1e3;

#if !( defined( USE_BAND_915 ) || defined( USE_BAND_915_HYBRID ) )
                //CFList
                if( ( size - 1 ) > 16 )
                {
                    ChannelParams_t param;
                    param.DrRange.Value = ( DR_5 << 4 ) | DR_0;

                    LoRaMacState |= LORAMAC_TX_CONFIG;
                    for( uint8_t i = 3, j = 0; i < ( 5 + 3 ); i++, j += 3 )
                    {
                        param.Frequency = ( ( uint32_t )LoRaMacRxPayload[13 + j] | ( ( uint32_t )LoRaMacRxPayload[14 + j] << 8 ) | ( ( uint32_t )LoRaMacRxPayload[15 + j] << 16 ) ) * 100;
                        if( param.Frequency != 0 ) {
                            LoRaMacChannelAdd( i, param );
                        } else {
                            LoRaMacChannelRemove( i );
                        }
                    }
                    LoRaMacState &= ~LORAMAC_TX_CONFIG;
                }
#endif
                STATS_INC(lora_mac_stats, joins);
                hal_timer_stop(&RxWindowTimer2);
                IsLoRaMacNetworkJoined = true;
                UpLinkCounter = 0;
                ChannelsNbRepCounter = 0;
                LoRaMacParams.ChannelsDatarate = LoRaMacParamsDefaults.ChannelsDatarate;
                MlmeConfirm.NbRetries = JoinRequestTrials;
                lora_mac_send_mlme_confirm(LORAMAC_EVENT_INFO_STATUS_OK);
            }
            break;
        case FRAME_TYPE_DATA_CONFIRMED_DOWN:
        case FRAME_TYPE_DATA_UNCONFIRMED_DOWN:
            /* If not joined I do not know why we would accept a frame */
            if (IsLoRaMacNetworkJoined == false) {
                goto process_rx_done;
            }

            address = payload[pktHeaderLen++];
            address |= ( (uint32_t)payload[pktHeaderLen++] << 8 );
            address |= ( (uint32_t)payload[pktHeaderLen++] << 16 );
            address |= ( (uint32_t)payload[pktHeaderLen++] << 24 );

            if (address != LoRaMacDevAddr) {
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
                downLinkCounter = DownLinkCounter;
            }

            fCtrl.Value = payload[pktHeaderLen++];

            sequenceCounter = ( uint16_t )payload[pktHeaderLen++];
            sequenceCounter |= ( uint16_t )payload[pktHeaderLen++] << 8;

            appPayloadStartIndex = 8 + fCtrl.Bits.FOptsLen;

            micRx |= ( uint32_t )payload[size - LORAMAC_MFR_LEN];
            micRx |= ( ( uint32_t )payload[size - LORAMAC_MFR_LEN + 1] << 8 );
            micRx |= ( ( uint32_t )payload[size - LORAMAC_MFR_LEN + 2] << 16 );
            micRx |= ( ( uint32_t )payload[size - LORAMAC_MFR_LEN + 3] << 24 );

            sequenceCounterPrev = (uint16_t)downLinkCounter;
            sequenceCounterDiff = (sequenceCounter - sequenceCounterPrev);

            /* Check for correct MIC */
            downLinkCounter += sequenceCounterDiff;
            LoRaMacComputeMic( payload, size - LORAMAC_MFR_LEN, nwkSKey, address, DOWN_LINK, downLinkCounter, &mic );
            if (micRx == mic) {
                isMicOk = true;
                /* XXX: inc mic good stat */
                McpsIndication.Status = LORAMAC_EVENT_INFO_STATUS_OK;
                McpsIndication.Multicast = multicast;
                McpsIndication.FramePending = fCtrl.Bits.FPending;
                McpsIndication.DownLinkCounter = downLinkCounter;
            } else {
                STATS_INC(lora_mac_stats, rx_mic_failures);
            }

            /* Check for a the maximum allowed counter difference */
            if (sequenceCounterDiff >= MAX_FCNT_GAP) {
                /*
                 * XXX: For now we will hand up the indication with this error
                 * status and let the application handle re-join. Note this in
                 * the documentation! This is only if the MIC is valid. If not,
                 * we will just ignore the received frame.
                 */
                if (isMicOk) {
                    McpsIndication.Status = LORAMAC_EVENT_INFO_STATUS_DOWNLINK_TOO_MANY_FRAMES_LOSS;
                    send_indicate = true;
                }
                goto process_rx_done;
            }

            if (isMicOk == true) {
                AdrAckCounter = 0;
                MacCommandsBufferToRepeatIndex = 0;

                // Update 32 bits downlink counter
                if (multicast == 1) {
                    McpsIndication.McpsIndication = MCPS_MULTICAST;

                    if((curMulticastParams->DownLinkCounter == downLinkCounter ) &&
                        (curMulticastParams->DownLinkCounter != 0)) {
                        /* XXX: I need to understand multicast. Are these sent
                         * in normal rx windows and act the same as normal
                           transmissions? */

                        /* XXX: count stat */
                        McpsIndication.Status = LORAMAC_EVENT_INFO_STATUS_DOWNLINK_REPEATED;
                        send_indicate = true;
                        goto process_rx_done;
                    }
                    curMulticastParams->DownLinkCounter = downLinkCounter;
                } else {
                    if (macHdr.Bits.MType == FRAME_TYPE_DATA_CONFIRMED_DOWN) {
                        SrvAckRequested = true;
                        McpsIndication.McpsIndication = MCPS_CONFIRMED;

                        if ((DownLinkCounter == downLinkCounter) &&
                            (DownLinkCounter != 0) ) {
                            /* Duplicated confirmed downlink. Skip indication.*/
                            skipIndication = true;
                        }
                    } else {
                        SrvAckRequested = false;
                        McpsIndication.McpsIndication = MCPS_UNCONFIRMED;

                        /* XXX: this should never happen. What should we do?
                           Count stat */
                        if ((DownLinkCounter == downLinkCounter ) &&
                            (DownLinkCounter != 0)) {
                            McpsIndication.Status = LORAMAC_EVENT_INFO_STATUS_DOWNLINK_REPEATED;
                            send_indicate = true;
                            goto process_rx_done;
                        }
                    }
                    DownLinkCounter = downLinkCounter;
                }

                /* XXX: Make sure MacCommands are handled properly */
                // This must be done before parsing the payload and the MAC commands.
                // We need to reset the MacCommandsBufferIndex here, since we need
                // to take retransmissions and repititions into account.
                if (McpsConfirm.McpsRequest == MCPS_CONFIRMED) {
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

                    McpsIndication.Port = port;

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
                            ProcessMacCommands( LoRaMacRxPayload, 0, frameLen, snr );
                        } else {
                            /* This is an invalid frame. Ignore it */
                            goto process_rx_done;
                            /* XXX: stat */
                        }
                    } else {
                        STATS_INC(lora_mac_stats, rx_mcps);

                        if( fCtrl.Bits.FOptsLen > 0 ) {
                            // Decode Options field MAC commands. Omit the fPort.
                            ProcessMacCommands( payload, 8, appPayloadStartIndex - 1, snr );
                        }

                        LoRaMacPayloadDecrypt( payload + appPayloadStartIndex,
                                               frameLen,
                                               appSKey,
                                               address,
                                               DOWN_LINK,
                                               downLinkCounter,
                                               LoRaMacRxPayload );

                        if (skipIndication == false ) {
                            McpsIndication.Buffer = LoRaMacRxPayload;
                            McpsIndication.BufferSize = frameLen;
                            McpsIndication.RxData = true;
                            send_indicate = true;
                        }
                    }
                } else {
                    if (fCtrl.Bits.FOptsLen > 0) {
                        // Decode Options field MAC commands
                        ProcessMacCommands( payload, 8, appPayloadStartIndex, snr );
                    }
                }

                /* We received a valid frame. */
                if (NodeAckRequested) {
                    if (skipIndication == false) {
                        // Check if the frame is an acknowledgement
                        if (fCtrl.Bits.Ack == 1) {
                            McpsIndication.AckReceived = true;

                            /* Stop AckTimeout timer as no more retransmissions needed. */
                            hal_timer_stop(&AckTimeoutTimer);
                            hal_timer_stop(&RxWindowTimer2);
                            lora_mac_tx_service_done(1);
                            goto process_rx_done;
                        }
                    }

                    /*
                     * XXX:
                     * We received a valid frame but no ack. If class
                     * A should we stop rx window 2 since we received
                     * a frame? Not sure about this.
                     */
                    if (LoRaMacDeviceClass == CLASS_A) {
                        hal_timer_stop(&RxWindowTimer2);
                    }
                } else {
                    /*
                     * No need to stop window 2 if class A and we received
                     * a frame on window 2. For any class we are
                     * allowed to transmit again since we heard a frame.
                     */
                    if ((LoRaMacDeviceClass == CLASS_C) || (RxSlot == 0)) {
                        hal_timer_stop(&RxWindowTimer2);
                    }
                    tx_service_over = 1;
                }
            }
            break;
        case FRAME_TYPE_PROPRIETARY:
            /* XXX: we do not handle proprietary frames right now */
            /* XXX: stat */
            break;
        default:
            /* XXX: stat */
            break;
    }

process_rx_done:
    if (LoRaMacDeviceClass == CLASS_C) {
        lora_mac_rx_on_window2(true);
        if (tx_service_over) {
            lora_mac_tx_service_done(0);
        }
    } else {
        /* If second receive window and a transmit service is running end it */
        if (tx_service_over || ((NodeAckRequested == false) && (RxSlot == 1) &&
            (LoRaMacState & LORAMAC_TX_RUNNING))) {
            lora_mac_tx_service_done(0);
        }
    }

    /* Send MCPS indication if flag set */
    if (send_indicate) {
        LoRaMacPrimitives->MacMcpsIndication(&McpsIndication);
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
        lora_mac_rx_on_window2(true);
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
        if (RxSlot == 1) {
            if (NodeAckRequested == false) {
                lora_mac_tx_service_done(0);
            }
        }
    } else {
        /*
         * If this is a class C device and the RxSlot is 1 it means that
         * we got an error during rx window 2. No way this could happen unless
         * there is an unconfirmed frame waiting to be transmitted.
         */
        if (RxSlot == 1) {
            lora_mac_tx_service_done(0);
        }
        lora_mac_rx_on_window2(true);
    }
}

static void
lora_mac_process_radio_rx_timeout(struct os_event *ev)
{
    lora_node_log(LORA_NODE_LOG_RX_TIMEOUT, Channel, 0, 0);

    if (LoRaMacDeviceClass != CLASS_C) {
        Radio.Sleep( );
        if (RxSlot == 1) {
            /* Let the ACK retry timer handle confirmed transmissions */
            if (NodeAckRequested == false) {
                lora_mac_tx_service_done(0);
            }
        }
    } else {
        lora_mac_tx_service_done(0);
        lora_mac_rx_on_window2(true);
    }
}

static void
lora_mac_process_tx_delay_timeout(struct os_event *ev)
{
    LoRaMacHeader_t macHdr;
    LoRaMacFrameCtrl_t fCtrl;

    LoRaMacState &= ~LORAMAC_TX_DELAYED;

    /*
     * It is possible that the delay timer is running but we finished
     * the transmit service. If that is the case, just return
     */
    if ((LoRaMacFlags.Bits.MlmeReq == 0) && (LoRaMacFlags.Bits.McpsReq == 0)) {
        lora_mac_chk_kickstart_tx();
        return;
    }

    if ((LoRaMacFlags.Bits.MlmeReq == 1) && (MlmeConfirm.MlmeRequest == MLME_JOIN)) {
        ResetMacParameters( );

        // Add a +1, since we start to count from 0
        LoRaMacParams.ChannelsDatarate = AlternateDatarate( JoinRequestTrials + 1 );

        macHdr.Value = 0;
        macHdr.Bits.MType = FRAME_TYPE_JOIN_REQ;

        fCtrl.Value = 0;
        fCtrl.Bits.Adr = AdrCtrlOn;

        /* In case of join request retransmissions, the stack must prepare
         * the frame again, because the network server keeps track of the random
         * LoRaMacDevNonce values to prevent reply attacks. */
        PrepareFrame(&macHdr, &fCtrl, 0, NULL, 0);
    }

    ScheduleTx( );
}

static void
lora_mac_process_rx_win1_timeout(struct os_event *ev)
{
    uint16_t symbTimeout = 5; // DR_2, DR_1, DR_0
    int8_t datarate = 0;
    uint32_t bandwidth = 0; // LoRa 125 kHz

#if (defined(USE_BAND_915) || defined(USE_BAND_915_HYBRID))
    uint16_t symb_usecs;
#endif

    RxSlot = 0;

    if (LoRaMacDeviceClass == CLASS_C) {
        Radio.Standby( );
    }

#if defined( USE_BAND_433 ) || defined( USE_BAND_780 ) || defined( USE_BAND_868 )
    datarate = LoRaMacParams.ChannelsDatarate - LoRaMacParams.Rx1DrOffset;
    if( datarate < 0 )
    {
        datarate = DR_0;
    }

    // For higher datarates, we increase the number of symbols generating a Rx Timeout
    if( ( datarate == DR_3 ) || ( datarate == DR_4 ) )
    { // DR_4, DR_3
        symbTimeout = 8;
    }
    else if( datarate == DR_5 )
    {
        symbTimeout = 10;
    }
    else if( datarate == DR_6 )
    {// LoRa 250 kHz
        bandwidth  = 1;
        symbTimeout = 14;
    }
    RxWindowSetup( Channels[Channel].Frequency, datarate, bandwidth, symbTimeout, false );
#elif defined( USE_BAND_470 )
    datarate = LoRaMacParams.ChannelsDatarate - LoRaMacParams.Rx1DrOffset;
    if( datarate < 0 )
    {
        datarate = DR_0;
    }

    // For higher datarates, we increase the number of symbols generating a Rx Timeout
    if( ( datarate == DR_3 ) || ( datarate == DR_4 ) )
    { // DR_4, DR_3
        symbTimeout = 8;
    }
    else if( datarate == DR_5 )
    {
        symbTimeout = 10;
    }
    RxWindowSetup( LORAMAC_FIRST_RX1_CHANNEL + ( Channel % 48 ) * LORAMAC_STEPWIDTH_RX1_CHANNEL, datarate, bandwidth, symbTimeout, false );
#elif ( defined( USE_BAND_915 ) || defined( USE_BAND_915_HYBRID ) )
    datarate = datarateOffsets[LoRaMacParams.ChannelsDatarate][LoRaMacParams.Rx1DrOffset];

    /*
     * XXX: why is this done? We are searching for a preamble and the preamble
     * is fixed at 8 symbols. Why does this need to change for based on the
     * datarate?
     */

    // For higher datarates, we increase the number of symbols generating a Rx Timeout
    switch (datarate) {
        case DR_0:       // SF10 - BW125
            /* XXX: this was 5, I made it 8 */
            //symbTimeout = 5;
            symbTimeout = 8;
            break;

        case DR_1:       // SF9  - BW125
        case DR_2:       // SF8  - BW125
        case DR_8:       // SF12 - BW500
        case DR_9:       // SF11 - BW500
        case DR_10:      // SF10 - BW500
            symbTimeout = 8;
            break;

        case DR_3:       // SF7  - BW125
        case DR_11:      // SF9  - BW500
            symbTimeout = 10;
            break;

        case DR_4:       // SF8  - BW500
        case DR_12:      // SF8  - BW500
            symbTimeout = 14;
            break;

        case DR_13:      // SF7  - BW500
            symbTimeout = 16;
            break;
        default:
            break;
    }

    /*
     * XXX: this seems to me to be quite chip specific. Probably should be moved
     * into radio code as only the radio knows the details of the timer. Note
      * that we use the uncoded symbol length to give us a bit more wait time.
     */
    symb_usecs = g_lora_uncoded_symbol_len_usecs[datarate];
    symbTimeout += ((RADIO_WAKEUP_TIME * 1000) + (symb_usecs - 1)) / symb_usecs;

    // LoRa 500 kHz
    if (datarate >= DR_4) {
        bandwidth  = 2;
    }
    RxWindowSetup( LORAMAC_FIRST_RX1_CHANNEL + ( Channel % 8 ) * LORAMAC_STEPWIDTH_RX1_CHANNEL, datarate, bandwidth, symbTimeout, false );
#else
    #error "Please define a frequency band in the compiler options."
#endif
}

/**
 * Called to set receiver on window 2 parameters. The parameter rx_continuous
 * is used exclusively for Class C devices. It is used when receiving at the
 * start of receive window 2 and it used so that we can generate a timeout if
 * no packet is received.
 *
 *
 * @param rx_continuous
 */
void
lora_mac_rx_on_window2(bool rx_continuous)
{
    uint16_t symbTimeout = 5; // DR_2, DR_1, DR_0
    uint32_t bandwidth = 0; // LoRa 125 kHz

    /*
     * RxSlot = 1 means we are receiving on the "real" window 2. RxSlot = 2
     * means we are a class C device receiving on window 2 parameters but
     * not during the actual second receive window.
     */
    if (LoRaMacDeviceClass == CLASS_C) {
        if (rx_continuous) {
            RxSlot = 2;
        } else {
            RxSlot = 1;
        }
    } else {
        RxSlot = 1;
    }

#if defined( USE_BAND_433 ) || defined( USE_BAND_780 ) || defined( USE_BAND_868 )
    // For higher datarates, we increase the number of symbols generating a Rx Timeout
    if( ( LoRaMacParams.Rx2Channel.Datarate == DR_3 ) || ( LoRaMacParams.Rx2Channel.Datarate == DR_4 ) )
    { // DR_4, DR_3
        symbTimeout = 8;
    }
    else if( LoRaMacParams.Rx2Channel.Datarate == DR_5 )
    {
        symbTimeout = 10;
    }
    else if( LoRaMacParams.Rx2Channel.Datarate == DR_6 )
    {// LoRa 250 kHz
        bandwidth  = 1;
        symbTimeout = 14;
    }
#elif defined( USE_BAND_470 )
    // For higher datarates, we increase the number of symbols generating a Rx Timeout
    if( ( LoRaMacParams.Rx2Channel.Datarate == DR_3 ) || ( LoRaMacParams.Rx2Channel.Datarate == DR_4 ) )
    { // DR_4, DR_3
        symbTimeout = 8;
    }
    else if( LoRaMacParams.Rx2Channel.Datarate == DR_5 )
    {
        symbTimeout = 10;
    }
#elif ( defined( USE_BAND_915 ) || defined( USE_BAND_915_HYBRID ) )
    // For higher datarates, we increase the number of symbols generating a Rx Timeout
    switch( LoRaMacParams.Rx2Channel.Datarate )
    {
        case DR_0:       // SF10 - BW125
            symbTimeout = 5;
            break;

        case DR_1:       // SF9  - BW125
        case DR_2:       // SF8  - BW125
        case DR_8:       // SF12 - BW500
        case DR_9:       // SF11 - BW500
        case DR_10:      // SF10 - BW500
            symbTimeout = 8;
            break;

        case DR_3:       // SF7  - BW125
        case DR_11:      // SF9  - BW500
            symbTimeout = 10;
            break;

        case DR_4:       // SF8  - BW500
        case DR_12:      // SF8  - BW500
            symbTimeout = 14;
            break;

        case DR_13:      // SF7  - BW500
            symbTimeout = 16;
            break;
        default:
            break;
    }
    if( LoRaMacParams.Rx2Channel.Datarate >= DR_4 )
    {// LoRa 500 kHz
        bandwidth  = 2;
    }
#else
    #error "Please define a frequency band in the compiler options."
#endif

    /* XXX: what happens if call to this fails? Can it? */
    RxWindowSetup(LoRaMacParams.Rx2Channel.Frequency,
                  LoRaMacParams.Rx2Channel.Datarate, bandwidth, symbTimeout,
                  rx_continuous);
}

/**
 * Receive Window 2 timeout
 *
 * Called when the receive window 2 timer expires.
 *
 * Context: MAC
 *
 * @param unused
 */
static void
lora_mac_process_rx_win2_timeout(struct os_event *ev)
{
    lora_mac_rx_on_window2(false);
}

static void
lora_mac_process_ack_timeout(struct os_event *ev)
{
    /* This should only be called for Confirmed packets */
    if (NodeAckRequested) {
        lora_mac_tx_service_done(0);
    } else {
        /* XXX: stat */
    }
}

static bool
SetNextChannel(uint32_t *time)
{
    uint8_t i, k, j;
    uint8_t nbEnabledChannels = 0;
    uint8_t delay_tx = 0;
    uint8_t enabledChannels[LORA_MAX_NB_CHANNELS];
    uint32_t next_tx_delay = ( uint32_t )( -1 );
    uint32_t elapsed_tx_done_time;

    memset( enabledChannels, 0, LORA_MAX_NB_CHANNELS );

#if defined( USE_BAND_915 ) || defined( USE_BAND_915_HYBRID )
    if( CountNbEnabled125kHzChannels( ChannelsMaskRemaining ) == 0 )
    { // Restore default channels
        memcpy( ( uint8_t* ) ChannelsMaskRemaining, ( uint8_t* ) LoRaMacParams.ChannelsMask, 8 );
    }
    if( ( LoRaMacParams.ChannelsDatarate >= DR_4 ) && ( ( ChannelsMaskRemaining[4] & 0x00FF ) == 0 ) )
    { // Make sure, that the channels are activated
        ChannelsMaskRemaining[4] = LoRaMacParams.ChannelsMask[4];
    }
#elif defined( USE_BAND_470 )
    if( ( CountBits( LoRaMacParams.ChannelsMask[0], 16 ) == 0 ) &&
        ( CountBits( LoRaMacParams.ChannelsMask[1], 16 ) == 0 ) &&
        ( CountBits( LoRaMacParams.ChannelsMask[2], 16 ) == 0 ) &&
        ( CountBits( LoRaMacParams.ChannelsMask[3], 16 ) == 0 ) &&
        ( CountBits( LoRaMacParams.ChannelsMask[4], 16 ) == 0 ) &&
        ( CountBits( LoRaMacParams.ChannelsMask[5], 16 ) == 0 ) )
    {
        memcpy( ( uint8_t* )LoRaMacParams.ChannelsMask, ( uint8_t* )LoRaMacParamsDefaults.ChannelsMask, sizeof( LoRaMacParams.ChannelsMask ) );
    }
#else
    if( CountBits( LoRaMacParams.ChannelsMask[0], 16 ) == 0 )
    {
        // Re-enable default channels, if no channel is enabled
        LoRaMacParams.ChannelsMask[0] = LoRaMacParams.ChannelsMask[0] | ( LC( 1 ) + LC( 2 ) + LC( 3 ) );
    }
#endif

    // Update Aggregated duty cycle
    elapsed_tx_done_time = TimerGetElapsedTime(AggregatedLastTxDoneTime);
    if (AggregatedTimeOff <= elapsed_tx_done_time) {
        AggregatedTimeOff = 0;

        // Update bands Time OFF
        for (i = 0; i < LORA_MAX_NB_BANDS; i++) {
            if ((IsLoRaMacNetworkJoined == false) || (DutyCycleOn == true)) {
                if (Bands[i].TimeOff <= TimerGetElapsedTime(Bands[i].LastTxDoneTime)) {
                    Bands[i].TimeOff = 0;
                }

                if (Bands[i].TimeOff != 0) {
                    next_tx_delay = MIN( Bands[i].TimeOff - TimerGetElapsedTime(Bands[i].LastTxDoneTime), next_tx_delay);
                }
            } else {
                if (DutyCycleOn == false) {
                    Bands[i].TimeOff = 0;
                }
            }
        }

        // Search how many channels are enabled
        for (i = 0, k = 0; i < LORA_MAX_NB_CHANNELS; i += 16, k++) {
            for (j = 0; j < 16; j++) {
#if defined( USE_BAND_915 ) || defined( USE_BAND_915_HYBRID )
                if( ( ChannelsMaskRemaining[k] & ( 1 << j ) ) != 0 )
#else
                if( ( LoRaMacParams.ChannelsMask[k] & ( 1 << j ) ) != 0 )
#endif
                {
                    if( Channels[i + j].Frequency == 0 )
                    { // Check if the channel is enabled
                        continue;
                    }
#if defined( USE_BAND_868 ) || defined( USE_BAND_433 ) || defined( USE_BAND_780 )
                    if( IsLoRaMacNetworkJoined == false )
                    {
                        if( ( JOIN_CHANNELS & ( 1 << j ) ) == 0 )
                        {
                            continue;
                        }
                    }
#endif
                    if( ( ( Channels[i + j].DrRange.Fields.Min <= LoRaMacParams.ChannelsDatarate ) &&
                          ( LoRaMacParams.ChannelsDatarate <= Channels[i + j].DrRange.Fields.Max ) ) == false ) {
                        // Check if the current channel selection supports the given datarate
                        continue;
                    }

                    if (Bands[Channels[i + j].Band].TimeOff > 0) {
                        // Check if the band is available for transmission
                        delay_tx++;
                        continue;
                    }
                    enabledChannels[nbEnabledChannels++] = i + j;
                }
            }
        }
    } else {
        delay_tx++;
        next_tx_delay = AggregatedTimeOff - elapsed_tx_done_time;
    }

    if (nbEnabledChannels > 0) {
        Channel = enabledChannels[randr( 0, nbEnabledChannels - 1 )];
#if defined( USE_BAND_915 ) || defined( USE_BAND_915_HYBRID )
        if (Channel < ( LORA_MAX_NB_CHANNELS - 8 )) {
            DisableChannelInMask( Channel, ChannelsMaskRemaining );
        }
#endif
        *time = 0;
        return true;
    } else {
        if (delay_tx > 0) {
            // Delay transmission due to aggregated time off or to a band time off
            *time = next_tx_delay;
            return true;
        }
        // Datarate not supported by any channel
        *time = 0;
        return false;
    }
}

static bool
RxWindowSetup( uint32_t freq, int8_t datarate, uint32_t bandwidth, uint16_t timeout, bool rxContinuous )
{
    uint8_t downlinkDatarate = Datarates[datarate];
    RadioModems_t modem;

    if (Radio.GetStatus( ) != RF_IDLE) {
        return false;
    }

    lora_node_log(LORA_NODE_LOG_RX_WIN_SETUP, datarate, timeout, freq);

    Radio.SetChannel( freq );

    // Store downlink datarate
    McpsIndication.RxDatarate = ( uint8_t ) datarate;

#if defined( USE_BAND_433 ) || defined( USE_BAND_780 ) || defined( USE_BAND_868 )
    if( datarate == DR_7 ) {
        modem = MODEM_FSK;
        Radio.SetRxConfig( modem, 50e3, downlinkDatarate * 1e3, 0, 83.333e3, 5, 0, false, 0, true, 0, 0, false, rxContinuous );
    } else {
        modem = MODEM_LORA;
        Radio.SetRxConfig( modem, bandwidth, downlinkDatarate, 1, 0, 8, timeout, false, 0, false, 0, 0, true, rxContinuous );
    }
#elif defined( USE_BAND_470 ) || defined( USE_BAND_915 ) || defined( USE_BAND_915_HYBRID )
    modem = MODEM_LORA;
    Radio.SetRxConfig( modem, bandwidth, downlinkDatarate, 1, 0, 8, timeout, false, 0, false, 0, 0, true, rxContinuous );
#endif

    if( RepeaterSupport == true ) {
        Radio.SetMaxPayloadLength( modem, MaxPayloadOfDatarateRepeater[datarate] + LORA_MAC_FRMPAYLOAD_OVERHEAD );
    } else {
        Radio.SetMaxPayloadLength( modem, MaxPayloadOfDatarate[datarate] + LORA_MAC_FRMPAYLOAD_OVERHEAD );
    }

    if( rxContinuous == false ) {
        Radio.Rx( LoRaMacParams.MaxRxWindow );
    } else {
        Radio.Rx( 0 ); // Continuous mode
    }
    return true;
}

static bool Rx2FreqInRange( uint32_t freq )
{
#if defined( USE_BAND_433 ) || defined( USE_BAND_780 ) || defined( USE_BAND_868 )
    if( Radio.CheckRfFrequency( freq ) == true )
#elif defined( USE_BAND_470 ) || defined( USE_BAND_915 ) || defined( USE_BAND_915_HYBRID )
    if( ( Radio.CheckRfFrequency( freq ) == true ) &&
        ( freq >= LORAMAC_FIRST_RX1_CHANNEL ) &&
        ( freq <= LORAMAC_LAST_RX1_CHANNEL ) &&
        ( ( ( freq - ( uint32_t ) LORAMAC_FIRST_RX1_CHANNEL ) % ( uint32_t ) LORAMAC_STEPWIDTH_RX1_CHANNEL ) == 0 ) )
#endif
    {
        return true;
    }
    return false;
}

static bool ValidatePayloadLength( uint8_t lenN, int8_t datarate, uint8_t fOptsLen )
{
    uint16_t maxN = 0;
    uint16_t payloadSize = 0;

    // Get the maximum payload length
    if( RepeaterSupport == true )
    {
        maxN = MaxPayloadOfDatarateRepeater[datarate];
    }
    else
    {
        maxN = MaxPayloadOfDatarate[datarate];
    }

    // Calculate the resulting payload size
    payloadSize = ( lenN + fOptsLen );

    // Validation of the application payload size
    if( ( payloadSize <= maxN ) && ( payloadSize <= LORAMAC_PHY_MAXPAYLOAD ) )
    {
        return true;
    }
    return false;
}

static uint8_t CountBits( uint16_t mask, uint8_t nbBits )
{
    uint8_t nbActiveBits = 0;

    for( uint8_t j = 0; j < nbBits; j++ )
    {
        if( ( mask & ( 1 << j ) ) == ( 1 << j ) )
        {
            nbActiveBits++;
        }
    }
    return nbActiveBits;
}

#if defined( USE_BAND_915 ) || defined( USE_BAND_915_HYBRID )
static uint8_t CountNbEnabled125kHzChannels( uint16_t *channelsMask )
{
    uint8_t nb125kHzChannels = 0;

    for( uint8_t i = 0, k = 0; i < LORA_MAX_NB_CHANNELS - 8; i += 16, k++ )
    {
        nb125kHzChannels += CountBits( channelsMask[k], 16 );
    }

    return nb125kHzChannels;
}

#if defined( USE_BAND_915_HYBRID )
static void ReenableChannels( uint16_t mask, uint16_t* channelsMask )
{
    uint16_t blockMask = mask;

    for( uint8_t i = 0, j = 0; i < 4; i++, j += 2 )
    {
        channelsMask[i] = 0;
        if( ( blockMask & ( 1 << j ) ) != 0 )
        {
            channelsMask[i] |= 0x00FF;
        }
        if( ( blockMask & ( 1 << ( j + 1 ) ) ) != 0 )
        {
            channelsMask[i] |= 0xFF00;
        }
    }
    channelsMask[4] = blockMask;
    channelsMask[5] = 0x0000;
}

static bool ValidateChannelMask( uint16_t* channelsMask )
{
    bool chanMaskState = false;
    uint16_t block1 = 0;
    uint16_t block2 = 0;
    uint8_t index = 0;

    for( uint8_t i = 0; i < 4; i++ )
    {
        block1 = channelsMask[i] & 0x00FF;
        block2 = channelsMask[i] & 0xFF00;

        if( ( CountBits( block1, 16 ) > 5 ) && ( chanMaskState == false ) )
        {
            channelsMask[i] &= block1;
            channelsMask[4] = 1 << ( i * 2 );
            chanMaskState = true;
            index = i;
        }
        else if( ( CountBits( block2, 16 ) > 5 ) && ( chanMaskState == false ) )
        {
            channelsMask[i] &= block2;
            channelsMask[4] = 1 << ( i * 2 + 1 );
            chanMaskState = true;
            index = i;
        }
    }

    // Do only change the channel mask, if we have found a valid block.
    if( chanMaskState == true )
    {
        for( uint8_t i = 0; i < 4; i++ )
        {
            if( i != index )
            {
                channelsMask[i] = 0;
            }
        }
    }
    return chanMaskState;
}
#endif
#endif

static bool ValidateDatarate( int8_t datarate, uint16_t* channelsMask )
{
    if( ValueInRange( datarate, LORAMAC_TX_MIN_DATARATE, LORAMAC_TX_MAX_DATARATE ) == false )
    {
        return false;
    }
    for( uint8_t i = 0, k = 0; i < LORA_MAX_NB_CHANNELS; i += 16, k++ )
    {
        for( uint8_t j = 0; j < 16; j++ )
        {
            if( ( ( channelsMask[k] & ( 1 << j ) ) != 0 ) )
            {// Check datarate validity for enabled channels
                if( ValueInRange( datarate, Channels[i + j].DrRange.Fields.Min, Channels[i + j].DrRange.Fields.Max ) == true )
                {
                    // At least 1 channel has been found we can return OK.
                    return true;
                }
            }
        }
    }
    return false;
}

static int8_t LimitTxPower( int8_t txPower, int8_t maxBandTxPower )
{
    int8_t resultTxPower = txPower;

    // Limit tx power to the band max
    resultTxPower =  MAX( txPower, maxBandTxPower );

#if defined( USE_BAND_915 ) || defined( USE_BAND_915_HYBRID )
    if( ( LoRaMacParams.ChannelsDatarate == DR_4 ) ||
        ( ( LoRaMacParams.ChannelsDatarate >= DR_8 ) && ( LoRaMacParams.ChannelsDatarate <= DR_13 ) ) )
    {// Limit tx power to max 26dBm
        resultTxPower =  MAX( txPower, TX_POWER_26_DBM );
    }
    else
    {
        if( CountNbEnabled125kHzChannels( LoRaMacParams.ChannelsMask ) < 50 )
        {// Limit tx power to max 21dBm
            resultTxPower = MAX( txPower, TX_POWER_20_DBM );
        }
    }
#endif
    return resultTxPower;
}

static bool ValueInRange( int8_t value, int8_t min, int8_t max )
{
    if( ( value >= min ) && ( value <= max ) )
    {
        return true;
    }
    return false;
}

static bool DisableChannelInMask( uint8_t id, uint16_t* mask )
{
    uint8_t index = 0;
    index = id / 16;

    if( ( index > 4 ) || ( id >= LORA_MAX_NB_CHANNELS ) )
    {
        return false;
    }

    // Deactivate channel
    mask[index] &= ~( 1 << ( id % 16 ) );

    return true;
}

static bool AdrNextDr( bool adrEnabled, bool updateChannelMask, int8_t* datarateOut )
{
    bool adrAckReq = false;
    int8_t datarate = LoRaMacParams.ChannelsDatarate;

    if (adrEnabled == true) {
        if (datarate == LORAMAC_TX_MIN_DATARATE) {
            AdrAckCounter = 0;
            adrAckReq = false;
        } else {
            if( AdrAckCounter >= ADR_ACK_LIMIT ) {
                adrAckReq = true;
                LoRaMacParams.ChannelsTxPower = LORAMAC_MAX_TX_POWER;
            } else {
                adrAckReq = false;
            }

            if( AdrAckCounter >= ( ADR_ACK_LIMIT + ADR_ACK_DELAY ) ) {
                if( ( AdrAckCounter % ADR_ACK_DELAY ) == 0 )
                {
#if defined( USE_BAND_433 ) || defined( USE_BAND_780 ) || defined( USE_BAND_868 )
                    if( datarate > LORAMAC_TX_MIN_DATARATE )
                    {
                        datarate--;
                    }
                    if( datarate == LORAMAC_TX_MIN_DATARATE )
                    {
                        if( updateChannelMask == true )
                        {
                            // Re-enable default channels LC1, LC2, LC3
                            LoRaMacParams.ChannelsMask[0] = LoRaMacParams.ChannelsMask[0] | ( LC( 1 ) + LC( 2 ) + LC( 3 ) );
                        }
                    }
#elif defined( USE_BAND_470 )
                    if( datarate > LORAMAC_TX_MIN_DATARATE )
                    {
                        datarate--;
                    }
                    if( datarate == LORAMAC_TX_MIN_DATARATE )
                    {
                        if( updateChannelMask == true )
                        {
                            // Re-enable default channels
                            memcpy( ( uint8_t* )LoRaMacParams.ChannelsMask, ( uint8_t* )LoRaMacParamsDefaults.ChannelsMask, sizeof( LoRaMacParams.ChannelsMask ) );
                        }
                    }
#elif defined( USE_BAND_915 ) || defined( USE_BAND_915_HYBRID )
                    if( ( datarate > LORAMAC_TX_MIN_DATARATE ) && ( datarate == DR_8 ) )
                    {
                        datarate = DR_4;
                    }
                    else if( datarate > LORAMAC_TX_MIN_DATARATE )
                    {
                        datarate--;
                    }
                    if( datarate == LORAMAC_TX_MIN_DATARATE )
                    {
                        if( updateChannelMask == true )
                        {
#if defined( USE_BAND_915 )
                            // Re-enable default channels
                            memcpy( ( uint8_t* )LoRaMacParams.ChannelsMask, ( uint8_t* )LoRaMacParamsDefaults.ChannelsMask, sizeof( LoRaMacParams.ChannelsMask ) );
#else // defined( USE_BAND_915_HYBRID )
                            // Re-enable default channels
                            ReenableChannels( LoRaMacParamsDefaults.ChannelsMask[4], LoRaMacParams.ChannelsMask );
#endif
                        }
                    }
#else
#error "Please define a frequency band in the compiler options."
#endif
                }
            }
        }
    }

    *datarateOut = datarate;

    return adrAckReq;
}

static LoRaMacStatus_t AddMacCommand( uint8_t cmd, uint8_t p1, uint8_t p2 )
{
    LoRaMacStatus_t status = LORAMAC_STATUS_BUSY;
    // The maximum buffer length must take MAC commands to re-send into account.
    uint8_t bufLen = LORA_MAC_COMMAND_MAX_LENGTH - MacCommandsBufferToRepeatIndex;

    switch( cmd )
    {
        case MOTE_MAC_LINK_CHECK_REQ:
            if( MacCommandsBufferIndex < bufLen )
            {
                MacCommandsBuffer[MacCommandsBufferIndex++] = cmd;
                // No payload for this command
                status = LORAMAC_STATUS_OK;
            }
            break;
        case MOTE_MAC_LINK_ADR_ANS:
            if( MacCommandsBufferIndex < ( bufLen - 1 ) )
            {
                MacCommandsBuffer[MacCommandsBufferIndex++] = cmd;
                // Margin
                MacCommandsBuffer[MacCommandsBufferIndex++] = p1;
                status = LORAMAC_STATUS_OK;
            }
            break;
        case MOTE_MAC_DUTY_CYCLE_ANS:
            if( MacCommandsBufferIndex < bufLen )
            {
                MacCommandsBuffer[MacCommandsBufferIndex++] = cmd;
                // No payload for this answer
                status = LORAMAC_STATUS_OK;
            }
            break;
        case MOTE_MAC_RX_PARAM_SETUP_ANS:
            if( MacCommandsBufferIndex < ( bufLen - 1 ) )
            {
                MacCommandsBuffer[MacCommandsBufferIndex++] = cmd;
                // Status: Datarate ACK, Channel ACK
                MacCommandsBuffer[MacCommandsBufferIndex++] = p1;
                status = LORAMAC_STATUS_OK;
            }
            break;
        case MOTE_MAC_DEV_STATUS_ANS:
            if( MacCommandsBufferIndex < ( bufLen - 2 ) )
            {
                MacCommandsBuffer[MacCommandsBufferIndex++] = cmd;
                // 1st byte Battery
                // 2nd byte Margin
                MacCommandsBuffer[MacCommandsBufferIndex++] = p1;
                MacCommandsBuffer[MacCommandsBufferIndex++] = p2;
                status = LORAMAC_STATUS_OK;
            }
            break;
        case MOTE_MAC_NEW_CHANNEL_ANS:
            if( MacCommandsBufferIndex < ( bufLen - 1 ) )
            {
                MacCommandsBuffer[MacCommandsBufferIndex++] = cmd;
                // Status: Datarate range OK, Channel frequency OK
                MacCommandsBuffer[MacCommandsBufferIndex++] = p1;
                status = LORAMAC_STATUS_OK;
            }
            break;
        case MOTE_MAC_RX_TIMING_SETUP_ANS:
            if( MacCommandsBufferIndex < bufLen )
            {
                MacCommandsBuffer[MacCommandsBufferIndex++] = cmd;
                // No payload for this answer
                status = LORAMAC_STATUS_OK;
            }
            break;
        default:
            return LORAMAC_STATUS_SERVICE_UNKNOWN;
    }

    if (status == LORAMAC_STATUS_OK) {
        MacCommandsInNextTx = true;
    }
    return status;
}

static uint8_t ParseMacCommandsToRepeat( uint8_t* cmdBufIn, uint8_t length, uint8_t* cmdBufOut )
{
    uint8_t i = 0;
    uint8_t cmdCount = 0;

    if( ( cmdBufIn == NULL ) || ( cmdBufOut == NULL ) )
    {
        return 0;
    }

    for( i = 0; i < length; i++ )
    {
        switch( cmdBufIn[i] )
        {
            // STICKY
            case MOTE_MAC_RX_PARAM_SETUP_ANS:
            {
                cmdBufOut[cmdCount++] = cmdBufIn[i++];
                cmdBufOut[cmdCount++] = cmdBufIn[i];
                break;
            }
            case MOTE_MAC_RX_TIMING_SETUP_ANS:
            {
                cmdBufOut[cmdCount++] = cmdBufIn[i];
                break;
            }
            // NON-STICKY
            case MOTE_MAC_DEV_STATUS_ANS:
            { // 2 bytes payload
                i += 2;
                break;
            }
            case MOTE_MAC_LINK_ADR_ANS:
            case MOTE_MAC_NEW_CHANNEL_ANS:
            { // 1 byte payload
                i++;
                break;
            }
            case MOTE_MAC_DUTY_CYCLE_ANS:
            case MOTE_MAC_LINK_CHECK_REQ:
            { // 0 byte payload
                break;
            }
            default:
                break;
        }
    }

    return cmdCount;
}

static void
ProcessMacCommands(uint8_t *payload, uint8_t macIndex, uint8_t commandsSize, uint8_t snr)
{
    while( macIndex < commandsSize )
    {
        // Decode Frame MAC commands
        switch( payload[macIndex++] )
        {
            case SRV_MAC_LINK_CHECK_ANS:
                MlmeConfirm.DemodMargin = payload[macIndex++];
                MlmeConfirm.NbGateways = payload[macIndex++];
                if ((LoRaMacFlags.Bits.MlmeReq == 1) &&
                    (MlmeConfirm.MlmeRequest == MLME_LINK_CHECK)) {
                    STATS_INC(lora_mac_stats, link_chk_ans_rxd);
                    lora_mac_send_mlme_confirm(LORAMAC_EVENT_INFO_STATUS_OK);
                }
                break;
            case SRV_MAC_LINK_ADR_REQ:
                {
                    uint8_t i;
                    uint8_t status = 0x07;
                    uint16_t chMask;
                    int8_t txPower = 0;
                    int8_t datarate = 0;
                    uint8_t nbRep = 0;
                    uint8_t chMaskCntl = 0;
                    uint16_t channelsMask[6] = { 0, 0, 0, 0, 0, 0 };

                    // Initialize local copy of the channels mask array
                    for( i = 0; i < 6; i++ )
                    {
                        channelsMask[i] = LoRaMacParams.ChannelsMask[i];
                    }
                    datarate = payload[macIndex++];
                    txPower = datarate & 0x0F;
                    datarate = ( datarate >> 4 ) & 0x0F;

                    if( ( AdrCtrlOn == false ) &&
                        ( ( LoRaMacParams.ChannelsDatarate != datarate ) || ( LoRaMacParams.ChannelsTxPower != txPower ) ) )
                    { // ADR disabled don't handle ADR requests if server tries to change datarate or txpower
                        // Answer the server with fail status
                        // Power ACK     = 0
                        // Data rate ACK = 0
                        // Channel mask  = 0
                        AddMacCommand( MOTE_MAC_LINK_ADR_ANS, 0, 0 );
                        macIndex += 3;  // Skip over the remaining bytes of the request
                        break;
                    }
                    chMask = ( uint16_t )payload[macIndex++];
                    chMask |= ( uint16_t )payload[macIndex++] << 8;

                    nbRep = payload[macIndex++];
                    chMaskCntl = ( nbRep >> 4 ) & 0x07;
                    nbRep &= 0x0F;
                    if( nbRep == 0 )
                    {
                        nbRep = 1;
                    }
#if defined( USE_BAND_433 ) || defined( USE_BAND_780 ) || defined( USE_BAND_868 )
                    if( ( chMaskCntl == 0 ) && ( chMask == 0 ) )
                    {
                        status &= 0xFE; // Channel mask KO
                    }
                    else if( ( ( chMaskCntl >= 1 ) && ( chMaskCntl <= 5 )) ||
                             ( chMaskCntl >= 7 ) )
                    {
                        // RFU
                        status &= 0xFE; // Channel mask KO
                    }
                    else
                    {
                        for( i = 0; i < LORA_MAX_NB_CHANNELS; i++ )
                        {
                            if( chMaskCntl == 6 )
                            {
                                if( Channels[i].Frequency != 0 )
                                {
                                    chMask |= 1 << i;
                                }
                            }
                            else
                            {
                                if( ( ( chMask & ( 1 << i ) ) != 0 ) &&
                                    ( Channels[i].Frequency == 0 ) )
                                {// Trying to enable an undefined channel
                                    status &= 0xFE; // Channel mask KO
                                }
                            }
                        }
                        channelsMask[0] = chMask;
                    }
#elif defined( USE_BAND_470 )
                    if( chMaskCntl == 6 )
                    {
                        // Enable all 125 kHz channels
                        for( uint8_t i = 0, k = 0; i < LORA_MAX_NB_CHANNELS; i += 16, k++ )
                        {
                            for( uint8_t j = 0; j < 16; j++ )
                            {
                                if( Channels[i + j].Frequency != 0 )
                                {
                                    channelsMask[k] |= 1 << j;
                                }
                            }
                        }
                    }
                    else if( chMaskCntl == 7 )
                    {
                        status &= 0xFE; // Channel mask KO
                    }
                    else
                    {
                        for( uint8_t i = 0; i < 16; i++ )
                        {
                            if( ( ( chMask & ( 1 << i ) ) != 0 ) &&
                                ( Channels[chMaskCntl * 16 + i].Frequency == 0 ) )
                            {// Trying to enable an undefined channel
                                status &= 0xFE; // Channel mask KO
                            }
                        }
                        channelsMask[chMaskCntl] = chMask;
                    }
#elif defined( USE_BAND_915 ) || defined( USE_BAND_915_HYBRID )
                    if( chMaskCntl == 6 )
                    {
                        // Enable all 125 kHz channels
                        channelsMask[0] = 0xFFFF;
                        channelsMask[1] = 0xFFFF;
                        channelsMask[2] = 0xFFFF;
                        channelsMask[3] = 0xFFFF;
                        // Apply chMask to channels 64 to 71
                        channelsMask[4] = chMask;
                    }
                    else if( chMaskCntl == 7 )
                    {
                        // Disable all 125 kHz channels
                        channelsMask[0] = 0x0000;
                        channelsMask[1] = 0x0000;
                        channelsMask[2] = 0x0000;
                        channelsMask[3] = 0x0000;
                        // Apply chMask to channels 64 to 71
                        channelsMask[4] = chMask;
                    }
                    else if( chMaskCntl == 5 )
                    {
                        // RFU
                        status &= 0xFE; // Channel mask KO
                    }
                    else
                    {
                        channelsMask[chMaskCntl] = chMask;

                        // FCC 15.247 paragraph F mandates to hop on at least 2 125 kHz channels
                        if( ( datarate < DR_4 ) && ( CountNbEnabled125kHzChannels( channelsMask ) < 2 ) )
                        {
                            status &= 0xFE; // Channel mask KO
                        }

#if defined( USE_BAND_915_HYBRID )
                        if( ValidateChannelMask( channelsMask ) == false )
                        {
                            status &= 0xFE; // Channel mask KO
                        }
#endif
                    }
#else
    #error "Please define a frequency band in the compiler options."
#endif
                    if( ValidateDatarate( datarate, channelsMask ) == false )
                    {
                        status &= 0xFD; // Datarate KO
                    }

                    //
                    // Remark MaxTxPower = 0 and MinTxPower = 5
                    //
                    if( ValueInRange( txPower, LORAMAC_MAX_TX_POWER, LORAMAC_MIN_TX_POWER ) == false )
                    {
                        status &= 0xFB; // TxPower KO
                    }
                    if( ( status & 0x07 ) == 0x07 )
                    {
                        LoRaMacParams.ChannelsDatarate = datarate;
                        LoRaMacParams.ChannelsTxPower = txPower;

                        memcpy( ( uint8_t* )LoRaMacParams.ChannelsMask, ( uint8_t* )channelsMask, sizeof( LoRaMacParams.ChannelsMask ) );

                        LoRaMacParams.ChannelsNbRep = nbRep;
#if defined( USE_BAND_915 ) || defined( USE_BAND_915_HYBRID )
                        // Reset ChannelsMaskRemaining to the new ChannelsMask
                        ChannelsMaskRemaining[0] &= channelsMask[0];
                        ChannelsMaskRemaining[1] &= channelsMask[1];
                        ChannelsMaskRemaining[2] &= channelsMask[2];
                        ChannelsMaskRemaining[3] &= channelsMask[3];
                        ChannelsMaskRemaining[4] = channelsMask[4];
                        ChannelsMaskRemaining[5] = channelsMask[5];
#endif
                    }
                    AddMacCommand( MOTE_MAC_LINK_ADR_ANS, status, 0 );
                }
                break;
            case SRV_MAC_DUTY_CYCLE_REQ:
                MaxDCycle = payload[macIndex++];
                AggregatedDCycle = 1 << MaxDCycle;
                AddMacCommand( MOTE_MAC_DUTY_CYCLE_ANS, 0, 0 );
                break;
            case SRV_MAC_RX_PARAM_SETUP_REQ:
                {
                    uint8_t status = 0x07;
                    int8_t datarate = 0;
                    int8_t drOffset = 0;
                    uint32_t freq = 0;

                    drOffset = ( payload[macIndex] >> 4 ) & 0x07;
                    datarate = payload[macIndex] & 0x0F;
                    macIndex++;

                    freq =  ( uint32_t )payload[macIndex++];
                    freq |= ( uint32_t )payload[macIndex++] << 8;
                    freq |= ( uint32_t )payload[macIndex++] << 16;
                    freq *= 100;

                    if( Rx2FreqInRange( freq ) == false )
                    {
                        status &= 0xFE; // Channel frequency KO
                    }

                    if( ValueInRange( datarate, LORAMAC_RX_MIN_DATARATE, LORAMAC_RX_MAX_DATARATE ) == false )
                    {
                        status &= 0xFD; // Datarate KO
                    }
#if ( defined( USE_BAND_915 ) || defined( USE_BAND_915_HYBRID ) )
                    if( ( ValueInRange( datarate, DR_5, DR_7 ) == true ) ||
                        ( datarate > DR_13 ) )
                    {
                        status &= 0xFD; // Datarate KO
                    }
#endif
                    if( ValueInRange( drOffset, LORAMAC_MIN_RX1_DR_OFFSET, LORAMAC_MAX_RX1_DR_OFFSET ) == false )
                    {
                        status &= 0xFB; // Rx1DrOffset range KO
                    }

                    if( ( status & 0x07 ) == 0x07 )
                    {
                        LoRaMacParams.Rx2Channel.Datarate = datarate;
                        LoRaMacParams.Rx2Channel.Frequency = freq;
                        LoRaMacParams.Rx1DrOffset = drOffset;
                    }
                    AddMacCommand( MOTE_MAC_RX_PARAM_SETUP_ANS, status, 0 );
                }
                break;
            case SRV_MAC_DEV_STATUS_REQ:
                {
                    uint8_t batteryLevel = BAT_LEVEL_NO_MEASURE;
                    if( ( LoRaMacCallbacks != NULL ) && ( LoRaMacCallbacks->GetBatteryLevel != NULL ) )
                    {
                        batteryLevel = LoRaMacCallbacks->GetBatteryLevel( );
                    }
                    AddMacCommand( MOTE_MAC_DEV_STATUS_ANS, batteryLevel, snr );
                    break;
                }
            case SRV_MAC_NEW_CHANNEL_REQ:
                {
                    uint8_t status = 0x03;

#if defined( USE_BAND_470 ) || defined( USE_BAND_915 ) || defined( USE_BAND_915_HYBRID )
                    status &= 0xFC; // Channel frequency and datarate KO
                    macIndex += 5;
#else
                    int8_t channelIndex = 0;
                    ChannelParams_t chParam;

                    channelIndex = payload[macIndex++];
                    chParam.Frequency = ( uint32_t )payload[macIndex++];
                    chParam.Frequency |= ( uint32_t )payload[macIndex++] << 8;
                    chParam.Frequency |= ( uint32_t )payload[macIndex++] << 16;
                    chParam.Frequency *= 100;
                    chParam.DrRange.Value = payload[macIndex++];

                    LoRaMacState |= LORAMAC_TX_CONFIG;
                    if( chParam.Frequency == 0 )
                    {
                        if( channelIndex < 3 )
                        {
                            status &= 0xFC;
                        }
                        else
                        {
                            if( LoRaMacChannelRemove( channelIndex ) != LORAMAC_STATUS_OK )
                            {
                                status &= 0xFC;
                            }
                        }
                    }
                    else
                    {
                        switch( LoRaMacChannelAdd( channelIndex, chParam ) )
                        {
                            case LORAMAC_STATUS_OK:
                            {
                                break;
                            }
                            case LORAMAC_STATUS_FREQUENCY_INVALID:
                            {
                                status &= 0xFE;
                                break;
                            }
                            case LORAMAC_STATUS_DATARATE_INVALID:
                            {
                                status &= 0xFD;
                                break;
                            }
                            case LORAMAC_STATUS_FREQ_AND_DR_INVALID:
                            {
                                status &= 0xFC;
                                break;
                            }
                            default:
                            {
                                status &= 0xFC;
                                break;
                            }
                        }
                    }
                    LoRaMacState &= ~LORAMAC_TX_CONFIG;
#endif
                    AddMacCommand( MOTE_MAC_NEW_CHANNEL_ANS, status, 0 );
                }
                break;
            case SRV_MAC_RX_TIMING_SETUP_REQ:
                {
                    uint8_t delay = payload[macIndex++] & 0x0F;

                    if( delay == 0 )
                    {
                        delay++;
                    }
                    LoRaMacParams.ReceiveDelay1 = delay * 1e3;
                    LoRaMacParams.ReceiveDelay2 = LoRaMacParams.ReceiveDelay1 + 1e3;
                    AddMacCommand( MOTE_MAC_RX_TIMING_SETUP_ANS, 0, 0 );
                }
                break;
            default:
                // Unknown command. ABORT MAC commands processing
                return;
        }
    }
}

LoRaMacStatus_t
Send(LoRaMacHeader_t *macHdr, uint8_t fPort, void *fBuffer, uint16_t fBufferSize)
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
    status = PrepareFrame( macHdr, &fCtrl, fPort, fBuffer, fBufferSize );

    // Validate status
    if (status == LORAMAC_STATUS_OK) {
        status = ScheduleTx( );
    }
    return status;
}

static LoRaMacStatus_t
ScheduleTx(void)
{
    uint32_t duty_cycle_time_off = 0;

    // Check if the device is off
    if (MaxDCycle == 255) {
        return LORAMAC_STATUS_DEVICE_OFF;
    }

    if (MaxDCycle == 0) {
        AggregatedTimeOff = 0;
    }

    // Select channel
    while (SetNextChannel(&duty_cycle_time_off) == false) {
        // Set the default datarate
        LoRaMacParams.ChannelsDatarate = LoRaMacParamsDefaults.ChannelsDatarate;

#if defined( USE_BAND_433 ) || defined( USE_BAND_780 ) || defined( USE_BAND_868 )
        // Re-enable default channels LC1, LC2, LC3
        LoRaMacParams.ChannelsMask[0] = LoRaMacParams.ChannelsMask[0] | ( LC( 1 ) + LC( 2 ) + LC( 3 ) );
#endif
    }

    // Schedule transmission of frame
    if (duty_cycle_time_off == 0) {
        // Try to send now
        return SendFrameOnChannel(Channels[Channel]);
    } else {
        // Send later - prepare timer
        LoRaMacState |= LORAMAC_TX_DELAYED;

        /* NOTE: dutyCycleTimeOff is already in cputime timer units. */
        hal_timer_stop(&TxDelayedTimer);
        hal_timer_start(&TxDelayedTimer, duty_cycle_time_off);

        return LORAMAC_STATUS_OK;
    }
}

static uint16_t
JoinDutyCycle(void)
{
    uint16_t duty_cycle;
    uint32_t elapsed_secs;
    uint64_t elapsed_usecs;

    elapsed_usecs = os_get_uptime_usec() - LoRaMacInitializationTime;
    elapsed_secs = (uint32_t)(elapsed_usecs / 1000000);

    if (elapsed_secs < 3600) {
        duty_cycle = BACKOFF_DC_1_HOUR;
    } else if (elapsed_secs < (3600 + 36000)) {
        duty_cycle = BACKOFF_DC_10_HOURS;
    } else {
        duty_cycle = BACKOFF_DC_24_HOURS;
    }
    return duty_cycle;
}

static void
CalculateBackOff(uint8_t channel)
{
    uint16_t duty_cycle = Bands[Channels[channel].Band].DCycle;
    uint16_t join_duty_cycle;
    uint32_t tx_ticks;

    // Reset time-off to initial value.
    Bands[Channels[channel].Band].TimeOff = 0;

    /* Convert the tx time on air (which is in msecs) to lora mac timer ticks */
    tx_ticks = TxTimeOnAir * 1000;

    if (IsLoRaMacNetworkJoined == false) {
        // The node has not joined yet. Apply join duty cycle to all regions.
        join_duty_cycle = JoinDutyCycle( );
        duty_cycle = MAX(duty_cycle, join_duty_cycle);

        // Update Band time-off.
        Bands[Channels[channel].Band].TimeOff = (tx_ticks * duty_cycle) - tx_ticks;
    } else {
        if (DutyCycleOn == true) {
            Bands[Channels[channel].Band].TimeOff = (tx_ticks * duty_cycle) - tx_ticks;
        }
    }

    // Update Aggregated Time OFF
    AggregatedTimeOff = AggregatedTimeOff + ((tx_ticks * AggregatedDCycle) - tx_ticks);
}

static int8_t AlternateDatarate( uint16_t nbTrials )
{
    int8_t datarate = LORAMAC_TX_MIN_DATARATE;
#if defined( USE_BAND_915 ) || defined( USE_BAND_915_HYBRID )
#if defined( USE_BAND_915 )
    // Re-enable 500 kHz default channels
    LoRaMacParams.ChannelsMask[4] = 0x00FF;
#else // defined( USE_BAND_915_HYBRID )
    // Re-enable 500 kHz default channels
    ReenableChannels( LoRaMacParamsDefaults.ChannelsMask[4], LoRaMacParams.ChannelsMask );
#endif

    if( ( nbTrials & 0x01 ) == 0x01 ) {
        datarate = DR_4;
    } else {
        datarate = DR_0;
    }
#else
    if( ( nbTrials % 48 ) == 0 ) {
        datarate = DR_0;
    } else if( ( nbTrials % 32 ) == 0 ) {
        datarate = DR_1;
    } else if( ( nbTrials % 24 ) == 0 ) {
        datarate = DR_2;
    } else if( ( nbTrials % 16 ) == 0 ) {
        datarate = DR_3;
    } else if( ( nbTrials % 8 ) == 0 ) {
        datarate = DR_4;
    } else {
        datarate = DR_5;
    }
#endif
    return datarate;
}

static void ResetMacParameters( void )
{
    IsLoRaMacNetworkJoined = false;

    // Counters
    UpLinkCounter = 0;
    DownLinkCounter = 0;
    AdrAckCounter = 0;

    ChannelsNbRepCounter = 0;

    AckTimeoutRetries = 1;
    AckTimeoutRetriesCounter = 1;

    MaxDCycle = 0;
    AggregatedDCycle = 1;

    MacCommandsBufferIndex = 0;
    MacCommandsBufferToRepeatIndex = 0;

    LoRaMacParams.ChannelsTxPower = LoRaMacParamsDefaults.ChannelsTxPower;
    LoRaMacParams.ChannelsDatarate = LoRaMacParamsDefaults.ChannelsDatarate;

    LoRaMacParams.MaxRxWindow = LoRaMacParamsDefaults.MaxRxWindow;
    LoRaMacParams.ReceiveDelay1 = LoRaMacParamsDefaults.ReceiveDelay1;
    LoRaMacParams.ReceiveDelay2 = LoRaMacParamsDefaults.ReceiveDelay2;
    LoRaMacParams.JoinAcceptDelay1 = LoRaMacParamsDefaults.JoinAcceptDelay1;
    LoRaMacParams.JoinAcceptDelay2 = LoRaMacParamsDefaults.JoinAcceptDelay2;

    LoRaMacParams.Rx1DrOffset = LoRaMacParamsDefaults.Rx1DrOffset;
    LoRaMacParams.ChannelsNbRep = LoRaMacParamsDefaults.ChannelsNbRep;

    LoRaMacParams.Rx2Channel = LoRaMacParamsDefaults.Rx2Channel;

    memcpy( ( uint8_t* ) LoRaMacParams.ChannelsMask, ( uint8_t* ) LoRaMacParamsDefaults.ChannelsMask, sizeof( LoRaMacParams.ChannelsMask ) );

#if defined( USE_BAND_915 ) || defined( USE_BAND_915_HYBRID )
    memcpy( ( uint8_t* ) ChannelsMaskRemaining, ( uint8_t* ) LoRaMacParamsDefaults.ChannelsMask, sizeof( LoRaMacParams.ChannelsMask ) );
#endif

    NodeAckRequested = false;
    SrvAckRequested = false;
    MacCommandsInNextTx = false;

    // Reset Multicast downlink counters
    MulticastParams_t *cur = MulticastChannels;
    while( cur != NULL )
    {
        cur->DownLinkCounter = 0;
        cur = cur->Next;
    }

    // Initialize channel index.
    Channel = LORA_MAX_NB_CHANNELS;

    // Store the current initialization time
    LoRaMacInitializationTime = os_get_uptime_usec();
}

LoRaMacStatus_t
PrepareFrame( LoRaMacHeader_t *macHdr, LoRaMacFrameCtrl_t *fCtrl, uint8_t fPort, void *fBuffer, uint16_t fBufferSize )
{
    uint16_t i;
    uint8_t pktHeaderLen = 0;
    uint32_t mic = 0;
    const void* payload = fBuffer;
    uint8_t framePort = fPort;

    LoRaMacBufferPktLen = 0;

    NodeAckRequested = false;

    if (fBuffer == NULL) {
        fBufferSize = 0;
    }

    LoRaMacTxPayloadLen = fBufferSize;

    LoRaMacBuffer[pktHeaderLen++] = macHdr->Value;

    switch( macHdr->Bits.MType )
    {
        case FRAME_TYPE_JOIN_REQ:
            RxWindow1Delay = LoRaMacParams.JoinAcceptDelay1 - RADIO_WAKEUP_TIME;
            RxWindow2Delay = LoRaMacParams.JoinAcceptDelay2 - RADIO_WAKEUP_TIME;

            LoRaMacBufferPktLen = pktHeaderLen;

            swap_buf( LoRaMacBuffer + LoRaMacBufferPktLen, LoRaMacAppEui, 8 );
            LoRaMacBufferPktLen += 8;
            swap_buf( LoRaMacBuffer + LoRaMacBufferPktLen, LoRaMacDevEui, 8 );
            LoRaMacBufferPktLen += 8;

            LoRaMacDevNonce = Radio.Random( );

            LoRaMacBuffer[LoRaMacBufferPktLen++] = LoRaMacDevNonce & 0xFF;
            LoRaMacBuffer[LoRaMacBufferPktLen++] = ( LoRaMacDevNonce >> 8 ) & 0xFF;

            LoRaMacJoinComputeMic( LoRaMacBuffer, LoRaMacBufferPktLen & 0xFF, LoRaMacAppKey, &mic );

            LoRaMacBuffer[LoRaMacBufferPktLen++] = mic & 0xFF;
            LoRaMacBuffer[LoRaMacBufferPktLen++] = ( mic >> 8 ) & 0xFF;
            LoRaMacBuffer[LoRaMacBufferPktLen++] = ( mic >> 16 ) & 0xFF;
            LoRaMacBuffer[LoRaMacBufferPktLen++] = ( mic >> 24 ) & 0xFF;
            break;
        case FRAME_TYPE_DATA_CONFIRMED_UP:
            NodeAckRequested = true;
            //Intentional fallthrough
        case FRAME_TYPE_DATA_UNCONFIRMED_UP:
            // No network has been joined yet
            if (IsLoRaMacNetworkJoined == false) {
                return LORAMAC_STATUS_NO_NETWORK_JOINED;
            }

            fCtrl->Bits.AdrAckReq = AdrNextDr( fCtrl->Bits.Adr, true, &LoRaMacParams.ChannelsDatarate );

            if( ValidatePayloadLength( LoRaMacTxPayloadLen, LoRaMacParams.ChannelsDatarate, MacCommandsBufferIndex ) == false )
            {
                return LORAMAC_STATUS_LENGTH_ERROR;
            }

            RxWindow1Delay = LoRaMacParams.ReceiveDelay1 - RADIO_WAKEUP_TIME;
            RxWindow2Delay = LoRaMacParams.ReceiveDelay2 - RADIO_WAKEUP_TIME;

            if (SrvAckRequested == true) {
                SrvAckRequested = false;
                fCtrl->Bits.Ack = 1;
            }

            LoRaMacBuffer[pktHeaderLen++] = ( LoRaMacDevAddr ) & 0xFF;
            LoRaMacBuffer[pktHeaderLen++] = ( LoRaMacDevAddr >> 8 ) & 0xFF;
            LoRaMacBuffer[pktHeaderLen++] = ( LoRaMacDevAddr >> 16 ) & 0xFF;
            LoRaMacBuffer[pktHeaderLen++] = ( LoRaMacDevAddr >> 24 ) & 0xFF;
            LoRaMacBuffer[pktHeaderLen++] = fCtrl->Value;
            LoRaMacBuffer[pktHeaderLen++] = UpLinkCounter & 0xFF;
            LoRaMacBuffer[pktHeaderLen++] = ( UpLinkCounter >> 8 ) & 0xFF;

            /* XXX: Insure that MacCommandsBufferIndex does not exceed
               maximum length if we have payload. */
            // Copy MAC commands which must be re-sent into MAC command buffer
            memcpy(&MacCommandsBuffer[MacCommandsBufferIndex],
                   MacCommandsBufferToRepeat, MacCommandsBufferToRepeatIndex);
            MacCommandsBufferIndex += MacCommandsBufferToRepeatIndex;

            if ((payload != NULL) && (LoRaMacTxPayloadLen > 0)) {
                if((MacCommandsBufferIndex <= LORA_MAC_COMMAND_MAX_LENGTH) && (MacCommandsInNextTx == true))
                {
                    fCtrl->Bits.FOptsLen += MacCommandsBufferIndex;

                    // Update FCtrl field with new value of OptionsLength
                    LoRaMacBuffer[0x05] = fCtrl->Value;
                    for (i = 0; i < MacCommandsBufferIndex; i++) {
                        LoRaMacBuffer[pktHeaderLen++] = MacCommandsBuffer[i];
                    }
                }
            } else {
                if((MacCommandsBufferIndex > 0) && (MacCommandsInNextTx)) {
                    LoRaMacTxPayloadLen = MacCommandsBufferIndex;
                    payload = MacCommandsBuffer;
                    framePort = 0;
                }
            }
            MacCommandsInNextTx = false;

            // Store MAC commands which must be re-send in case the device does not receive a downlink anymore
            MacCommandsBufferToRepeatIndex = ParseMacCommandsToRepeat( MacCommandsBuffer, MacCommandsBufferIndex, MacCommandsBufferToRepeat );
            if (MacCommandsBufferToRepeatIndex > 0) {
                MacCommandsInNextTx = true;
            }
            MacCommandsBufferIndex = 0;

            if( ( payload != NULL ) && ( LoRaMacTxPayloadLen > 0 ) )
            {
                LoRaMacBuffer[pktHeaderLen++] = framePort;

                if( framePort == 0 )
                {
                    LoRaMacPayloadEncrypt( (uint8_t* ) payload, LoRaMacTxPayloadLen, LoRaMacNwkSKey, LoRaMacDevAddr, UP_LINK, UpLinkCounter, LoRaMacPayload );
                }
                else
                {
                    LoRaMacPayloadEncrypt( (uint8_t* ) payload, LoRaMacTxPayloadLen, LoRaMacAppSKey, LoRaMacDevAddr, UP_LINK, UpLinkCounter, LoRaMacPayload );
                }
                memcpy( LoRaMacBuffer + pktHeaderLen, LoRaMacPayload, LoRaMacTxPayloadLen );
            }
            LoRaMacBufferPktLen = pktHeaderLen + LoRaMacTxPayloadLen;

            LoRaMacComputeMic( LoRaMacBuffer, LoRaMacBufferPktLen, LoRaMacNwkSKey, LoRaMacDevAddr, UP_LINK, UpLinkCounter, &mic );

            LoRaMacBuffer[LoRaMacBufferPktLen + 0] = mic & 0xFF;
            LoRaMacBuffer[LoRaMacBufferPktLen + 1] = ( mic >> 8 ) & 0xFF;
            LoRaMacBuffer[LoRaMacBufferPktLen + 2] = ( mic >> 16 ) & 0xFF;
            LoRaMacBuffer[LoRaMacBufferPktLen + 3] = ( mic >> 24 ) & 0xFF;

            LoRaMacBufferPktLen += LORAMAC_MFR_LEN;

            break;
        case FRAME_TYPE_PROPRIETARY:
            if( ( fBuffer != NULL ) && ( LoRaMacTxPayloadLen > 0 ) )
            {
                memcpy( LoRaMacBuffer + pktHeaderLen, ( uint8_t* ) fBuffer, LoRaMacTxPayloadLen );
                LoRaMacBufferPktLen = pktHeaderLen + LoRaMacTxPayloadLen;
            }
            break;
        default:
            return LORAMAC_STATUS_SERVICE_UNKNOWN;
    }

    return LORAMAC_STATUS_OK;
}

LoRaMacStatus_t
SendFrameOnChannel( ChannelParams_t channel )
{
    int8_t datarate = Datarates[LoRaMacParams.ChannelsDatarate];
    int8_t txPowerIndex = 0;
    int8_t txPower = 0;

    txPowerIndex = LimitTxPower( LoRaMacParams.ChannelsTxPower, Bands[channel.Band].TxMaxPower );
    txPower = TxPowers[txPowerIndex];

    /* Set MCPS confirm information */
    McpsConfirm.Datarate = LoRaMacParams.ChannelsDatarate;
    McpsConfirm.TxPower = txPowerIndex;
    McpsConfirm.UpLinkFrequency = channel.Frequency;

    Radio.SetChannel( channel.Frequency );

#if defined( USE_BAND_433 ) || defined( USE_BAND_780 ) || defined( USE_BAND_868 )
    if( LoRaMacParams.ChannelsDatarate == DR_7 )
    { // High Speed FSK channel
        Radio.SetMaxPayloadLength( MODEM_FSK, LoRaMacBufferPktLen );
        Radio.SetTxConfig( MODEM_FSK, txPower, 25e3, 0, datarate * 1e3, 0, 5, false, true, 0, 0, false, 3e3 );
        TxTimeOnAir = Radio.TimeOnAir( MODEM_FSK, LoRaMacBufferPktLen );

    }
    else if( LoRaMacParams.ChannelsDatarate == DR_6 )
    { // High speed LoRa channel
        Radio.SetMaxPayloadLength( MODEM_LORA, LoRaMacBufferPktLen );
        Radio.SetTxConfig( MODEM_LORA, txPower, 0, 1, datarate, 1, 8, false, true, 0, 0, false, 3e3 );
        TxTimeOnAir = Radio.TimeOnAir( MODEM_LORA, LoRaMacBufferPktLen );
    }
    else
    { // Normal LoRa channel
        Radio.SetMaxPayloadLength( MODEM_LORA, LoRaMacBufferPktLen );
        Radio.SetTxConfig( MODEM_LORA, txPower, 0, 0, datarate, 1, 8, false, true, 0, 0, false, 3e3 );
        TxTimeOnAir = Radio.TimeOnAir( MODEM_LORA, LoRaMacBufferPktLen );
    }
#elif defined( USE_BAND_915 ) || defined( USE_BAND_915_HYBRID )
    Radio.SetMaxPayloadLength( MODEM_LORA, LoRaMacBufferPktLen );
    if( LoRaMacParams.ChannelsDatarate >= DR_4 )
    { // High speed LoRa channel BW500 kHz
        Radio.SetTxConfig( MODEM_LORA, txPower, 0, 2, datarate, 1, 8, false, true, 0, 0, false, 3e3 );
        TxTimeOnAir = Radio.TimeOnAir( MODEM_LORA, LoRaMacBufferPktLen );
    }
    else
    { // Normal LoRa channel
        Radio.SetTxConfig( MODEM_LORA, txPower, 0, 0, datarate, 1, 8, false, true, 0, 0, false, 3e3 );
        TxTimeOnAir = Radio.TimeOnAir( MODEM_LORA, LoRaMacBufferPktLen );
    }
#elif defined( USE_BAND_470 )
    Radio.SetMaxPayloadLength( MODEM_LORA, LoRaMacBufferPktLen );
    Radio.SetTxConfig( MODEM_LORA, txPower, 0, 0, datarate, 1, 8, false, true, 0, 0, false, 3e3 );
    TxTimeOnAir = Radio.TimeOnAir( MODEM_LORA, LoRaMacBufferPktLen );
#else
    #error "Please define a frequency band in the compiler options."
#endif

    // Store the time on air
    McpsConfirm.TxTimeOnAir = TxTimeOnAir;
    MlmeConfirm.TxTimeOnAir = TxTimeOnAir;

    if (IsLoRaMacNetworkJoined == false) {
        JoinRequestTrials++;
    }

    // Send now
    Radio.Send( LoRaMacBuffer, LoRaMacBufferPktLen );

    LoRaMacState |= LORAMAC_TX_RUNNING;

    return LORAMAC_STATUS_OK;
}

LoRaMacStatus_t SetTxContinuousWave( uint16_t timeout )
{
    //int8_t txPowerIndex = 0;
    //int8_t txPower = 0;

    //txPowerIndex = LimitTxPower( LoRaMacParams.ChannelsTxPower, Bands[Channels[Channel].Band].TxMaxPower );
    //txPower = TxPowers[txPowerIndex];

    //Radio.SetTxContinuousWave( Channels[Channel].Frequency, txPower, timeout );

    LoRaMacState |= LORAMAC_TX_RUNNING;

    return LORAMAC_STATUS_OK;
}

LoRaMacStatus_t
LoRaMacInitialization( LoRaMacPrimitives_t *primitives, LoRaMacCallback_t *callbacks )
{
    assert(primitives != NULL);

    assert((primitives->MacMcpsConfirm != NULL) &&
               (primitives->MacMcpsIndication != NULL)  &&
               (primitives->MacMlmeConfirm != NULL));

    LoRaMacPrimitives = primitives;
    LoRaMacCallbacks = callbacks;

    LoRaMacFlags.Value = 0;

    LoRaMacDeviceClass = CLASS_A;
    LoRaMacState = LORAMAC_IDLE;

    JoinRequestTrials = 0;
    MaxJoinRequestTrials = 1;
    RepeaterSupport = false;

    // Reset duty cycle times
    AggregatedLastTxDoneTime = 0;
    AggregatedTimeOff = 0;

    // Duty cycle
#if defined( USE_BAND_433 )
    DutyCycleOn = true;
#elif defined( USE_BAND_470 )
    DutyCycleOn = false;
#elif defined( USE_BAND_780 )
    DutyCycleOn = true;
#elif defined( USE_BAND_868 )
    DutyCycleOn = true;
#elif defined( USE_BAND_915 ) || defined( USE_BAND_915_HYBRID )
    DutyCycleOn = false;
#else
    #error "Please define a frequency band in the compiler options."
#endif

    // Reset to defaults
    LoRaMacParamsDefaults.ChannelsTxPower = LORAMAC_DEFAULT_TX_POWER;
    LoRaMacParamsDefaults.ChannelsDatarate = LORAMAC_DEFAULT_DATARATE;

    LoRaMacParamsDefaults.MaxRxWindow = MAX_RX_WINDOW;
    LoRaMacParamsDefaults.ReceiveDelay1 = RECEIVE_DELAY1;
    LoRaMacParamsDefaults.ReceiveDelay2 = RECEIVE_DELAY2;
    LoRaMacParamsDefaults.JoinAcceptDelay1 = JOIN_ACCEPT_DELAY1;
    LoRaMacParamsDefaults.JoinAcceptDelay2 = JOIN_ACCEPT_DELAY2;

    LoRaMacParamsDefaults.ChannelsNbRep = 1;
    LoRaMacParamsDefaults.Rx1DrOffset = 0;

    LoRaMacParamsDefaults.Rx2Channel = ( Rx2ChannelParams_t )RX_WND_2_CHANNEL;

    // Channel mask
#if defined( USE_BAND_433 )
    LoRaMacParamsDefaults.ChannelsMask[0] = LC( 1 ) + LC( 2 ) + LC( 3 );
#elif defined ( USE_BAND_470 )
    LoRaMacParamsDefaults.ChannelsMask[0] = 0xFFFF;
    LoRaMacParamsDefaults.ChannelsMask[1] = 0xFFFF;
    LoRaMacParamsDefaults.ChannelsMask[2] = 0xFFFF;
    LoRaMacParamsDefaults.ChannelsMask[3] = 0xFFFF;
    LoRaMacParamsDefaults.ChannelsMask[4] = 0xFFFF;
    LoRaMacParamsDefaults.ChannelsMask[5] = 0xFFFF;
#elif defined( USE_BAND_780 )
    LoRaMacParamsDefaults.ChannelsMask[0] = LC( 1 ) + LC( 2 ) + LC( 3 );
#elif defined( USE_BAND_868 )
    LoRaMacParamsDefaults.ChannelsMask[0] = LC( 1 ) + LC( 2 ) + LC( 3 );
#elif defined( USE_BAND_915 )
    LoRaMacParamsDefaults.ChannelsMask[0] = 0xFFFF;
    LoRaMacParamsDefaults.ChannelsMask[1] = 0xFFFF;
    LoRaMacParamsDefaults.ChannelsMask[2] = 0xFFFF;
    LoRaMacParamsDefaults.ChannelsMask[3] = 0xFFFF;
    LoRaMacParamsDefaults.ChannelsMask[4] = 0x00FF;
    LoRaMacParamsDefaults.ChannelsMask[5] = 0x0000;
#elif defined( USE_BAND_915_HYBRID )
    LoRaMacParamsDefaults.ChannelsMask[0] = 0x00FF;
    LoRaMacParamsDefaults.ChannelsMask[1] = 0x0000;
    LoRaMacParamsDefaults.ChannelsMask[2] = 0x0000;
    LoRaMacParamsDefaults.ChannelsMask[3] = 0x0000;
    LoRaMacParamsDefaults.ChannelsMask[4] = 0x0001;
    LoRaMacParamsDefaults.ChannelsMask[5] = 0x0000;
#else
    #error "Please define a frequency band in the compiler options."
#endif

#if defined( USE_BAND_915 ) || defined( USE_BAND_915_HYBRID )
    // 125 kHz channels
    for( uint8_t i = 0; i < LORA_MAX_NB_CHANNELS - 8; i++ )
    {
        Channels[i].Frequency = 902.3e6 + i * 200e3;
        Channels[i].DrRange.Value = ( DR_3 << 4 ) | DR_0;
        Channels[i].Band = 0;
    }
    // 500 kHz channels
    for( uint8_t i = LORA_MAX_NB_CHANNELS - 8; i < LORA_MAX_NB_CHANNELS; i++ )
    {
        Channels[i].Frequency = 903.0e6 + ( i - ( LORA_MAX_NB_CHANNELS - 8 ) ) * 1.6e6;
        Channels[i].DrRange.Value = ( DR_4 << 4 ) | DR_4;
        Channels[i].Band = 0;
    }
#elif defined( USE_BAND_470 )
    // 125 kHz channels
    for( uint8_t i = 0; i < LORA_MAX_NB_CHANNELS; i++ )
    {
        Channels[i].Frequency = 470.3e6 + i * 200e3;
        Channels[i].DrRange.Value = ( DR_5 << 4 ) | DR_0;
        Channels[i].Band = 0;
    }
#endif

    ResetMacParameters( );

    /* XXX: determine which of these should be os callouts */
    hal_timer_config(LORA_MAC_TIMER_NUM, LORA_MAC_TIMER_FREQ);
    hal_timer_set_cb(LORA_MAC_TIMER_NUM, &TxDelayedTimer, OnTxDelayedTimerEvent,
                     NULL);
    hal_timer_set_cb(LORA_MAC_TIMER_NUM, &RxWindowTimer1, OnRxWindow1TimerEvent,
                     NULL);
    hal_timer_set_cb(LORA_MAC_TIMER_NUM, &RxWindowTimer2, OnRxWindow2TimerEvent,
                     NULL);
    hal_timer_set_cb(LORA_MAC_TIMER_NUM, &AckTimeoutTimer,
                     OnAckTimeoutTimerEvent, NULL);

    /* Init MAC radio events */
    g_lora_mac_radio_tx_timeout_event.ev_cb = lora_mac_process_radio_tx_timeout;
    g_lora_mac_radio_tx_event.ev_cb = lora_mac_process_radio_tx;
    g_lora_mac_radio_rx_event.ev_cb = lora_mac_process_radio_rx;
    g_lora_mac_radio_rx_timeout_event.ev_cb = lora_mac_process_radio_rx_timeout;
    g_lora_mac_radio_rx_err_event.ev_cb = lora_mac_process_radio_rx_err;
    g_lora_mac_ack_timeout_event.ev_cb = lora_mac_process_ack_timeout;
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

    PublicNetwork = true;
    Radio.SetPublicNetwork( PublicNetwork );
    Radio.Sleep( );

    return LORAMAC_STATUS_OK;
}

LoRaMacStatus_t
LoRaMacQueryTxPossible(uint8_t size, LoRaMacTxInfo_t* txInfo)
{
    int8_t datarate = LoRaMacParamsDefaults.ChannelsDatarate;
    uint8_t fOptLen = MacCommandsBufferIndex + MacCommandsBufferToRepeatIndex;

    assert(txInfo != NULL);

    AdrNextDr(AdrCtrlOn, false, &datarate);

    if (RepeaterSupport == true) {
        txInfo->CurrentPayloadSize = MaxPayloadOfDatarateRepeater[datarate];
    } else {
        txInfo->CurrentPayloadSize = MaxPayloadOfDatarate[datarate];
    }

    if (txInfo->CurrentPayloadSize >= fOptLen) {
        txInfo->MaxPossiblePayload = txInfo->CurrentPayloadSize - fOptLen;
    } else {
        return LORAMAC_STATUS_MAC_CMD_LENGTH_ERROR;
    }

    if (ValidatePayloadLength(size, datarate, 0 ) == false) {
        return LORAMAC_STATUS_LENGTH_ERROR;
    }

    if (ValidatePayloadLength(size, datarate, fOptLen) == false) {
        return LORAMAC_STATUS_MAC_CMD_LENGTH_ERROR;
    }

    return LORAMAC_STATUS_OK;
}

LoRaMacStatus_t LoRaMacMibGetRequestConfirm( MibRequestConfirm_t *mibGet )
{
    LoRaMacStatus_t status = LORAMAC_STATUS_OK;

    if( mibGet == NULL )
    {
        return LORAMAC_STATUS_PARAMETER_INVALID;
    }

    switch( mibGet->Type )
    {
        case MIB_DEVICE_CLASS:
        {
            mibGet->Param.Class = LoRaMacDeviceClass;
            break;
        }
        case MIB_NETWORK_JOINED:
        {
            mibGet->Param.IsNetworkJoined = IsLoRaMacNetworkJoined;
            break;
        }
        case MIB_ADR:
        {
            mibGet->Param.AdrEnable = AdrCtrlOn;
            break;
        }
        case MIB_NET_ID:
        {
            mibGet->Param.NetID = LoRaMacNetID;
            break;
        }
        case MIB_DEV_ADDR:
        {
            mibGet->Param.DevAddr = LoRaMacDevAddr;
            break;
        }
        case MIB_NWK_SKEY:
        {
            mibGet->Param.NwkSKey = LoRaMacNwkSKey;
            break;
        }
        case MIB_APP_SKEY:
        {
            mibGet->Param.AppSKey = LoRaMacAppSKey;
            break;
        }
        case MIB_PUBLIC_NETWORK:
        {
            mibGet->Param.EnablePublicNetwork = PublicNetwork;
            break;
        }
        case MIB_REPEATER_SUPPORT:
        {
            mibGet->Param.EnableRepeaterSupport = RepeaterSupport;
            break;
        }
        case MIB_CHANNELS:
        {
            mibGet->Param.ChannelList = Channels;
            break;
        }
        case MIB_RX2_CHANNEL:
        {
            mibGet->Param.Rx2Channel = LoRaMacParams.Rx2Channel;
            break;
        }
        case MIB_RX2_DEFAULT_CHANNEL:
        {
            mibGet->Param.Rx2Channel = LoRaMacParamsDefaults.Rx2Channel;
            break;
        }
        case MIB_CHANNELS_DEFAULT_MASK:
        {
            mibGet->Param.ChannelsDefaultMask = LoRaMacParamsDefaults.ChannelsMask;
            break;
        }
        case MIB_CHANNELS_MASK:
        {
            mibGet->Param.ChannelsMask = LoRaMacParams.ChannelsMask;
            break;
        }
        case MIB_CHANNELS_NB_REP:
        {
            mibGet->Param.ChannelNbRep = LoRaMacParams.ChannelsNbRep;
            break;
        }
        case MIB_MAX_RX_WINDOW_DURATION:
        {
            mibGet->Param.MaxRxWindow = LoRaMacParams.MaxRxWindow;
            break;
        }
        case MIB_RECEIVE_DELAY_1:
        {
            mibGet->Param.ReceiveDelay1 = LoRaMacParams.ReceiveDelay1;
            break;
        }
        case MIB_RECEIVE_DELAY_2:
        {
            mibGet->Param.ReceiveDelay2 = LoRaMacParams.ReceiveDelay2;
            break;
        }
        case MIB_JOIN_ACCEPT_DELAY_1:
        {
            mibGet->Param.JoinAcceptDelay1 = LoRaMacParams.JoinAcceptDelay1;
            break;
        }
        case MIB_JOIN_ACCEPT_DELAY_2:
        {
            mibGet->Param.JoinAcceptDelay2 = LoRaMacParams.JoinAcceptDelay2;
            break;
        }
        case MIB_CHANNELS_DEFAULT_DATARATE:
        {
            mibGet->Param.ChannelsDefaultDatarate = LoRaMacParamsDefaults.ChannelsDatarate;
            break;
        }
        case MIB_CHANNELS_DATARATE:
        {
            mibGet->Param.ChannelsDatarate = LoRaMacParams.ChannelsDatarate;
            break;
        }
        case MIB_CHANNELS_DEFAULT_TX_POWER:
        {
            mibGet->Param.ChannelsDefaultTxPower = LoRaMacParamsDefaults.ChannelsTxPower;
            break;
        }
        case MIB_CHANNELS_TX_POWER:
        {
            mibGet->Param.ChannelsTxPower = LoRaMacParams.ChannelsTxPower;
            break;
        }
        case MIB_UPLINK_COUNTER:
        {
            mibGet->Param.UpLinkCounter = UpLinkCounter;
            break;
        }
        case MIB_DOWNLINK_COUNTER:
        {
            mibGet->Param.DownLinkCounter = DownLinkCounter;
            break;
        }
        case MIB_MULTICAST_CHANNEL:
        {
            mibGet->Param.MulticastList = MulticastChannels;
            break;
        }
        default:
            status = LORAMAC_STATUS_SERVICE_UNKNOWN;
            break;
    }

    return status;
}

LoRaMacStatus_t LoRaMacMibSetRequestConfirm( MibRequestConfirm_t *mibSet )
{
    LoRaMacStatus_t status = LORAMAC_STATUS_OK;

    assert(mibSet != NULL);

    if ((LoRaMacState & LORAMAC_TX_RUNNING ) == LORAMAC_TX_RUNNING) {
        return LORAMAC_STATUS_BUSY;
    }

    switch (mibSet->Type) {
        case MIB_DEVICE_CLASS:
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
                    // Set the NodeAckRequested indicator to default
                    NodeAckRequested = false;
                    lora_mac_rx_on_window2(true);
                    break;
            }
            break;
        case MIB_NETWORK_JOINED:
            IsLoRaMacNetworkJoined = mibSet->Param.IsNetworkJoined;
            break;
        case MIB_ADR:
            AdrCtrlOn = mibSet->Param.AdrEnable;
            break;
        case MIB_NET_ID:
            LoRaMacNetID = mibSet->Param.NetID;
            break;
        case MIB_DEV_ADDR:
            LoRaMacDevAddr = mibSet->Param.DevAddr;
            break;
        case MIB_NWK_SKEY:
            if( mibSet->Param.NwkSKey != NULL )
            {
                memcpy( LoRaMacNwkSKey, mibSet->Param.NwkSKey,
                               sizeof( LoRaMacNwkSKey ) );
            }
            else
            {
                status = LORAMAC_STATUS_PARAMETER_INVALID;
            }
            break;
        case MIB_APP_SKEY:
        {
            if( mibSet->Param.AppSKey != NULL )
            {
                memcpy( LoRaMacAppSKey, mibSet->Param.AppSKey,
                               sizeof( LoRaMacAppSKey ) );
            }
            else
            {
                status = LORAMAC_STATUS_PARAMETER_INVALID;
            }
            break;
        }
        case MIB_PUBLIC_NETWORK:
        {
            PublicNetwork = mibSet->Param.EnablePublicNetwork;
            Radio.SetPublicNetwork( PublicNetwork );
            break;
        }
        case MIB_REPEATER_SUPPORT:
        {
             RepeaterSupport = mibSet->Param.EnableRepeaterSupport;
            break;
        }
        case MIB_RX2_CHANNEL:
        {
            LoRaMacParams.Rx2Channel = mibSet->Param.Rx2Channel;
            break;
        }
        case MIB_RX2_DEFAULT_CHANNEL:
        {
            LoRaMacParamsDefaults.Rx2Channel = mibSet->Param.Rx2DefaultChannel;
            break;
        }
        case MIB_CHANNELS_DEFAULT_MASK:
        {
            if (mibSet->Param.ChannelsDefaultMask) {
#if defined( USE_BAND_915 ) || defined( USE_BAND_915_HYBRID )
                bool chanMaskState = true;

#if defined( USE_BAND_915_HYBRID )
                chanMaskState = ValidateChannelMask( mibSet->Param.ChannelsDefaultMask );
#endif
                if( chanMaskState == true ) {
                    if( ( CountNbEnabled125kHzChannels( mibSet->Param.ChannelsMask ) < 2 ) &&
                        ( CountNbEnabled125kHzChannels( mibSet->Param.ChannelsMask ) > 0 ) ){
                        status = LORAMAC_STATUS_PARAMETER_INVALID;
                    }
                    else {
                        memcpy( ( uint8_t* ) LoRaMacParamsDefaults.ChannelsMask,
                                 ( uint8_t* ) mibSet->Param.ChannelsDefaultMask, sizeof( LoRaMacParamsDefaults.ChannelsMask ) );
                        for ( uint8_t i = 0; i < sizeof( LoRaMacParamsDefaults.ChannelsMask ) / 2; i++ )
                        {
                            // Disable channels which are no longer available
                            ChannelsMaskRemaining[i] &= LoRaMacParamsDefaults.ChannelsMask[i];
                        }
                    }
                } else {
                    status = LORAMAC_STATUS_PARAMETER_INVALID;
                }
#elif defined( USE_BAND_470 )
                memcpy( ( uint8_t* ) LoRaMacParamsDefaults.ChannelsMask,
                         ( uint8_t* ) mibSet->Param.ChannelsDefaultMask, sizeof( LoRaMacParamsDefaults.ChannelsMask ) );
#else
                memcpy( ( uint8_t* ) LoRaMacParamsDefaults.ChannelsMask,
                         ( uint8_t* ) mibSet->Param.ChannelsDefaultMask, 2 );
#endif
            } else {
                status = LORAMAC_STATUS_PARAMETER_INVALID;
            }
            break;
        }
        case MIB_CHANNELS_MASK:
        {
            if( mibSet->Param.ChannelsMask )
            {
#if defined( USE_BAND_915 ) || defined( USE_BAND_915_HYBRID )
                bool chanMaskState = true;

#if defined( USE_BAND_915_HYBRID )
                chanMaskState = ValidateChannelMask( mibSet->Param.ChannelsMask );
#endif
                if( chanMaskState == true )
                {
                    if( ( CountNbEnabled125kHzChannels( mibSet->Param.ChannelsMask ) < 2 ) &&
                        ( CountNbEnabled125kHzChannels( mibSet->Param.ChannelsMask ) > 0 ) )
                    {
                        status = LORAMAC_STATUS_PARAMETER_INVALID;
                    }
                    else
                    {
                        memcpy( ( uint8_t* ) LoRaMacParams.ChannelsMask,
                                 ( uint8_t* ) mibSet->Param.ChannelsMask, sizeof( LoRaMacParams.ChannelsMask ) );
                        for ( uint8_t i = 0; i < sizeof( LoRaMacParams.ChannelsMask ) / 2; i++ )
                        {
                            // Disable channels which are no longer available
                            ChannelsMaskRemaining[i] &= LoRaMacParams.ChannelsMask[i];
                        }
                    }
                }
                else
                {
                    status = LORAMAC_STATUS_PARAMETER_INVALID;
                }
#elif defined( USE_BAND_470 )
                memcpy( ( uint8_t* ) LoRaMacParams.ChannelsMask,
                         ( uint8_t* ) mibSet->Param.ChannelsMask, sizeof( LoRaMacParams.ChannelsMask ) );
#else
                memcpy( ( uint8_t* ) LoRaMacParams.ChannelsMask,
                         ( uint8_t* ) mibSet->Param.ChannelsMask, 2 );
#endif
            }
            else
            {
                status = LORAMAC_STATUS_PARAMETER_INVALID;
            }
            break;
        }
        case MIB_CHANNELS_NB_REP:
        {
            if( ( mibSet->Param.ChannelNbRep >= 1 ) &&
                ( mibSet->Param.ChannelNbRep <= 15 ) )
            {
                LoRaMacParams.ChannelsNbRep = mibSet->Param.ChannelNbRep;
            }
            else
            {
                status = LORAMAC_STATUS_PARAMETER_INVALID;
            }
            break;
        }
        case MIB_MAX_RX_WINDOW_DURATION:
        {
            LoRaMacParams.MaxRxWindow = mibSet->Param.MaxRxWindow;
            break;
        }
        case MIB_RECEIVE_DELAY_1:
        {
            LoRaMacParams.ReceiveDelay1 = mibSet->Param.ReceiveDelay1;
            break;
        }
        case MIB_RECEIVE_DELAY_2:
        {
            LoRaMacParams.ReceiveDelay2 = mibSet->Param.ReceiveDelay2;
            break;
        }
        case MIB_JOIN_ACCEPT_DELAY_1:
        {
            LoRaMacParams.JoinAcceptDelay1 = mibSet->Param.JoinAcceptDelay1;
            break;
        }
        case MIB_JOIN_ACCEPT_DELAY_2:
        {
            LoRaMacParams.JoinAcceptDelay2 = mibSet->Param.JoinAcceptDelay2;
            break;
        }
        case MIB_CHANNELS_DEFAULT_DATARATE:
        {
#if defined( USE_BAND_433 ) || defined( USE_BAND_780 ) || defined( USE_BAND_868 )
            if( ValueInRange( mibSet->Param.ChannelsDefaultDatarate,
                              DR_0, DR_5 ) )
            {
                LoRaMacParamsDefaults.ChannelsDatarate = mibSet->Param.ChannelsDefaultDatarate;
            }
#else
            if( ValueInRange( mibSet->Param.ChannelsDefaultDatarate,
                              LORAMAC_TX_MIN_DATARATE, LORAMAC_TX_MAX_DATARATE ) )
            {
                LoRaMacParamsDefaults.ChannelsDatarate = mibSet->Param.ChannelsDefaultDatarate;
            }
#endif
            else
            {
                status = LORAMAC_STATUS_PARAMETER_INVALID;
            }
            break;
        }
        case MIB_CHANNELS_DATARATE:
        {
            if( ValueInRange( mibSet->Param.ChannelsDatarate,
                              LORAMAC_TX_MIN_DATARATE, LORAMAC_TX_MAX_DATARATE ) )
            {
                LoRaMacParams.ChannelsDatarate = mibSet->Param.ChannelsDatarate;
            }
            else
            {
                status = LORAMAC_STATUS_PARAMETER_INVALID;
            }
            break;
        }
        case MIB_CHANNELS_DEFAULT_TX_POWER:
        {
            if( ValueInRange( mibSet->Param.ChannelsDefaultTxPower,
                              LORAMAC_MAX_TX_POWER, LORAMAC_MIN_TX_POWER ) )
            {
                LoRaMacParamsDefaults.ChannelsTxPower = mibSet->Param.ChannelsDefaultTxPower;
            }
            else
            {
                status = LORAMAC_STATUS_PARAMETER_INVALID;
            }
            break;
        }
        case MIB_CHANNELS_TX_POWER:
        {
            if( ValueInRange( mibSet->Param.ChannelsTxPower,
                              LORAMAC_MAX_TX_POWER, LORAMAC_MIN_TX_POWER ) )
            {
                LoRaMacParams.ChannelsTxPower = mibSet->Param.ChannelsTxPower;
            }
            else
            {
                status = LORAMAC_STATUS_PARAMETER_INVALID;
            }
            break;
        }
        case MIB_UPLINK_COUNTER:
        {
            UpLinkCounter = mibSet->Param.UpLinkCounter;
            break;
        }
        case MIB_DOWNLINK_COUNTER:
        {
            DownLinkCounter = mibSet->Param.DownLinkCounter;
            break;
        }
        default:
            status = LORAMAC_STATUS_SERVICE_UNKNOWN;
            break;
    }

    return status;
}

LoRaMacStatus_t LoRaMacChannelAdd( uint8_t id, ChannelParams_t params )
{
#if defined( USE_BAND_470 ) || defined( USE_BAND_915 ) || defined( USE_BAND_915_HYBRID )
    return LORAMAC_STATUS_PARAMETER_INVALID;
#else
    bool datarateInvalid = false;
    bool frequencyInvalid = false;
    uint8_t band = 0;

    // The id must not exceed LORA_MAX_NB_CHANNELS
    if( id >= LORA_MAX_NB_CHANNELS )
    {
        return LORAMAC_STATUS_PARAMETER_INVALID;
    }
    // Validate if the MAC is in a correct state
    if( ( LoRaMacState & LORAMAC_TX_RUNNING ) == LORAMAC_TX_RUNNING )
    {
        if( ( LoRaMacState & LORAMAC_TX_CONFIG ) != LORAMAC_TX_CONFIG )
        {
            return LORAMAC_STATUS_BUSY;
        }
    }
    // Validate the datarate
    if( ( params.DrRange.Fields.Min > params.DrRange.Fields.Max ) ||
        ( ValueInRange( params.DrRange.Fields.Min, LORAMAC_TX_MIN_DATARATE,
                        LORAMAC_TX_MAX_DATARATE ) == false ) ||
        ( ValueInRange( params.DrRange.Fields.Max, LORAMAC_TX_MIN_DATARATE,
                        LORAMAC_TX_MAX_DATARATE ) == false ) )
    {
        datarateInvalid = true;
    }

#if defined( USE_BAND_433 ) || defined( USE_BAND_780 ) || defined( USE_BAND_868 )
    if( id < 3 )
    {
        if( params.Frequency != Channels[id].Frequency )
        {
            frequencyInvalid = true;
        }

        if( params.DrRange.Fields.Min > DR_0 )
        {
            datarateInvalid = true;
        }
        if( ValueInRange( params.DrRange.Fields.Max, DR_5, LORAMAC_TX_MAX_DATARATE ) == false )
        {
            datarateInvalid = true;
        }
    }
#endif

    // Validate the frequency
    if( ( Radio.CheckRfFrequency( params.Frequency ) == true ) && ( params.Frequency > 0 ) && ( frequencyInvalid == false ) )
    {
#if defined( USE_BAND_868 )
        if( ( params.Frequency >= 863000000 ) && ( params.Frequency < 865000000 ) )
        {
            band = BAND_G1_2;
        }
        else if( ( params.Frequency >= 865000000 ) && ( params.Frequency <= 868000000 ) )
        {
            band = BAND_G1_0;
        }
        else if( ( params.Frequency > 868000000 ) && ( params.Frequency <= 868600000 ) )
        {
            band = BAND_G1_1;
        }
        else if( ( params.Frequency >= 868700000 ) && ( params.Frequency <= 869200000 ) )
        {
            band = BAND_G1_2;
        }
        else if( ( params.Frequency >= 869400000 ) && ( params.Frequency <= 869650000 ) )
        {
            band = BAND_G1_3;
        }
        else if( ( params.Frequency >= 869700000 ) && ( params.Frequency <= 870000000 ) )
        {
            band = BAND_G1_4;
        }
        else
        {
            frequencyInvalid = true;
        }
#endif
    }
    else
    {
        frequencyInvalid = true;
    }

    if( ( datarateInvalid == true ) && ( frequencyInvalid == true ) )
    {
        return LORAMAC_STATUS_FREQ_AND_DR_INVALID;
    }
    if( datarateInvalid == true )
    {
        return LORAMAC_STATUS_DATARATE_INVALID;
    }
    if( frequencyInvalid == true )
    {
        return LORAMAC_STATUS_FREQUENCY_INVALID;
    }

    // Every parameter is valid, activate the channel
    Channels[id] = params;
    Channels[id].Band = band;
    LoRaMacParams.ChannelsMask[0] |= ( 1 << id );

    return LORAMAC_STATUS_OK;
#endif
}

LoRaMacStatus_t LoRaMacChannelRemove( uint8_t id )
{
#if defined( USE_BAND_433 ) || defined( USE_BAND_780 ) || defined( USE_BAND_868 )
    if( ( LoRaMacState & LORAMAC_TX_RUNNING ) == LORAMAC_TX_RUNNING )
    {
        if( ( LoRaMacState & LORAMAC_TX_CONFIG ) != LORAMAC_TX_CONFIG )
        {
            return LORAMAC_STATUS_BUSY;
        }
    }

    if( ( id < 3 ) || ( id >= LORA_MAX_NB_CHANNELS ) )
    {
        return LORAMAC_STATUS_PARAMETER_INVALID;
    }
    else
    {
        // Remove the channel from the list of channels
        Channels[id] = ( ChannelParams_t ){ 0, { 0 }, 0 };

        // Disable the channel as it doesn't exist anymore
        if( DisableChannelInMask( id, LoRaMacParams.ChannelsMask ) == false )
        {
            return LORAMAC_STATUS_PARAMETER_INVALID;
        }
    }
    return LORAMAC_STATUS_OK;
#elif ( defined( USE_BAND_915 ) || defined( USE_BAND_915_HYBRID ) || defined( USE_BAND_470 ) )
    return LORAMAC_STATUS_PARAMETER_INVALID;
#endif
}

LoRaMacStatus_t LoRaMacMulticastChannelLink( MulticastParams_t *channelParam )
{
    if( channelParam == NULL )
    {
        return LORAMAC_STATUS_PARAMETER_INVALID;
    }
    if( ( LoRaMacState & LORAMAC_TX_RUNNING ) == LORAMAC_TX_RUNNING )
    {
        return LORAMAC_STATUS_BUSY;
    }

    // Reset downlink counter
    channelParam->DownLinkCounter = 0;

    if( MulticastChannels == NULL )
    {
        // New node is the fist element
        MulticastChannels = channelParam;
    }
    else
    {
        MulticastParams_t *cur = MulticastChannels;

        // Search the last node in the list
        while( cur->Next != NULL )
        {
            cur = cur->Next;
        }
        // This function always finds the last node
        cur->Next = channelParam;
    }

    return LORAMAC_STATUS_OK;
}

LoRaMacStatus_t LoRaMacMulticastChannelUnlink( MulticastParams_t *channelParam )
{
    if( channelParam == NULL )
    {
        return LORAMAC_STATUS_PARAMETER_INVALID;
    }
    if( ( LoRaMacState & LORAMAC_TX_RUNNING ) == LORAMAC_TX_RUNNING )
    {
        return LORAMAC_STATUS_BUSY;
    }

    if( MulticastChannels != NULL )
    {
        if( MulticastChannels == channelParam )
        {
          // First element
          MulticastChannels = channelParam->Next;
        }
        else
        {
            MulticastParams_t *cur = MulticastChannels;

            // Search the node in the list
            while( cur->Next && cur->Next != channelParam )
            {
                cur = cur->Next;
            }
            // If we found the node, remove it
            if( cur->Next )
            {
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

    /* If currently running or MLME request in progress, return busy */
    if ((LoRaMacState & LORAMAC_TX_RUNNING ) == LORAMAC_TX_RUNNING) {
        return LORAMAC_STATUS_BUSY;
    }

    if (LoRaMacFlags.Bits.MlmeReq == 1) {
        return LORAMAC_STATUS_BUSY;
    }

    memset( ( uint8_t* ) &MlmeConfirm, 0, sizeof( MlmeConfirm ) );
    MlmeConfirm.Status = LORAMAC_EVENT_INFO_STATUS_ERROR;
    MlmeConfirm.MlmeRequest = mlmeRequest->Type;

    switch( mlmeRequest->Type )
    {
        case MLME_JOIN:
            if( ( LoRaMacState & LORAMAC_TX_DELAYED ) == LORAMAC_TX_DELAYED )
            {
                return LORAMAC_STATUS_BUSY;
            }

            if( ( mlmeRequest->Req.Join.DevEui == NULL ) ||
                ( mlmeRequest->Req.Join.AppEui == NULL ) ||
                ( mlmeRequest->Req.Join.AppKey == NULL ) ||
                ( mlmeRequest->Req.Join.NbTrials == 0 ) )
            {
                return LORAMAC_STATUS_PARAMETER_INVALID;
            }

#if ( defined( USE_BAND_915 ) || defined( USE_BAND_915_HYBRID ) )
            // Enables at least the usage of the 2 datarates.
            if (mlmeRequest->Req.Join.NbTrials < 2) {
                mlmeRequest->Req.Join.NbTrials = 2;
            }
#else
            // Enables at least the usage of all datarates.
            if (mlmeRequest->Req.Join.NbTrials < 48) {
                mlmeRequest->Req.Join.NbTrials = 48;
            }
#endif

            LoRaMacFlags.Bits.MlmeReq = 1;

            LoRaMacDevEui = mlmeRequest->Req.Join.DevEui;
            LoRaMacAppEui = mlmeRequest->Req.Join.AppEui;
            LoRaMacAppKey = mlmeRequest->Req.Join.AppKey;
            MaxJoinRequestTrials = mlmeRequest->Req.Join.NbTrials;

            // Reset variable JoinRequestTrials
            JoinRequestTrials = 0;

            // Setup header information
            macHdr.Value = 0;
            macHdr.Bits.MType  = FRAME_TYPE_JOIN_REQ;

            ResetMacParameters( );

            // Add a +1, since we start to count from 0
            LoRaMacParams.ChannelsDatarate = AlternateDatarate( JoinRequestTrials + 1 );

            status = Send( &macHdr, 0, NULL, 0 );
            break;
        case MLME_LINK_CHECK:
            LoRaMacFlags.Bits.MlmeReq = 1;
            // LoRaMac will send this command piggy-pack
            status = AddMacCommand( MOTE_MAC_LINK_CHECK_REQ, 0, 0 );
            if (status == LORAMAC_STATUS_OK) {
                STATS_INC(lora_mac_stats, link_chk_tx);
            }
            break;
        case MLME_TXCW:
            LoRaMacFlags.Bits.MlmeReq = 1;
            status = SetTxContinuousWave( mlmeRequest->Req.TxCw.Timeout );
            break;
        default:
            break;
    }

    if (status != LORAMAC_STATUS_OK) {
        LoRaMacFlags.Bits.MlmeReq = 0;
    }

    return status;
}

LoRaMacStatus_t
LoRaMacMcpsRequest(McpsReq_t *mcpsRequest)
{
    LoRaMacStatus_t status = LORAMAC_STATUS_SERVICE_UNKNOWN;
    LoRaMacHeader_t macHdr;
    uint8_t fPort = 0;
    void *fBuffer;
    uint16_t fBufferSize;
    int8_t datarate;

    assert(mcpsRequest != NULL);

    if (((LoRaMacState & LORAMAC_TX_RUNNING) == LORAMAC_TX_RUNNING) ||
        ((LoRaMacState & LORAMAC_TX_DELAYED) == LORAMAC_TX_DELAYED)) {
        return LORAMAC_STATUS_BUSY;
    }

    macHdr.Value = 0;

    /* Reset confirm parameters */
    memset(&McpsConfirm, 0, sizeof(McpsConfirm));
    McpsConfirm.Status = LORAMAC_EVENT_INFO_STATUS_ERROR;
    McpsConfirm.om = mcpsRequest->om;
    McpsConfirm.NbRetries = 0;
    McpsConfirm.AckReceived = false;
    McpsConfirm.UpLinkCounter = UpLinkCounter;
    McpsConfirm.McpsRequest = mcpsRequest->Type;

    switch (mcpsRequest->Type) {
        case MCPS_UNCONFIRMED:
            AckTimeoutRetries = 1;

            macHdr.Bits.MType = FRAME_TYPE_DATA_UNCONFIRMED_UP;
            fPort = mcpsRequest->Req.Unconfirmed.fPort;
            fBuffer = mcpsRequest->Req.Unconfirmed.fBuffer;
            fBufferSize = mcpsRequest->Req.Unconfirmed.fBufferSize;
            datarate = mcpsRequest->Req.Unconfirmed.Datarate;
            break;
        case MCPS_CONFIRMED:
            AckTimeoutRetriesCounter = 1;
            AckTimeoutRetries = mcpsRequest->Req.Confirmed.NbTrials;
            if (AckTimeoutRetries > MAX_ACK_RETRIES) {
                AckTimeoutRetries = MAX_ACK_RETRIES;
            }

            macHdr.Bits.MType = FRAME_TYPE_DATA_CONFIRMED_UP;
            fPort = mcpsRequest->Req.Confirmed.fPort;
            fBuffer = mcpsRequest->Req.Confirmed.fBuffer;
            fBufferSize = mcpsRequest->Req.Confirmed.fBufferSize;
            datarate = mcpsRequest->Req.Confirmed.Datarate;
            break;
        case MCPS_PROPRIETARY:
            AckTimeoutRetries = 1;

            macHdr.Bits.MType = FRAME_TYPE_PROPRIETARY;
            fBuffer = mcpsRequest->Req.Proprietary.fBuffer;
            fBufferSize = mcpsRequest->Req.Proprietary.fBufferSize;
            datarate = mcpsRequest->Req.Proprietary.Datarate;
            break;
    default:
            datarate = 0;
            assert(0);
            break;
    }

    if (AdrCtrlOn == false) {
        if (ValueInRange(datarate, LORAMAC_TX_MIN_DATARATE, LORAMAC_TX_MAX_DATARATE))
        {
            LoRaMacParams.ChannelsDatarate = datarate;
        } else {
            return LORAMAC_STATUS_PARAMETER_INVALID;
        }
    }

    status = Send( &macHdr, fPort, fBuffer, fBufferSize );
    if (status == LORAMAC_STATUS_OK) {
        LoRaMacFlags.Bits.McpsReq = 1;
    } else {
        NodeAckRequested = false;
    }

    return status;
}

void LoRaMacTestRxWindowsOn( bool enable )
{
}

void LoRaMacTestSetMic( uint16_t txPacketCounter )
{
    UpLinkCounter = txPacketCounter;
    IsUpLinkCounterFixed = true;
}

void LoRaMacTestSetDutyCycleOn( bool enable )
{
#if ( defined( USE_BAND_868 ) || defined( USE_BAND_433 ) || defined( USE_BAND_780 ) )
    DutyCycleOn = enable;
#else
    DutyCycleOn = false;
#endif
}

void LoRaMacTestSetChannel( uint8_t channel )
{
    Channel = channel;
}

LoRaMacStatus_t
lora_mac_tx_state(void)
{
    LoRaMacStatus_t rc;

    if (((LoRaMacState & LORAMAC_TX_RUNNING) == LORAMAC_TX_RUNNING) ||
        ((LoRaMacState & LORAMAC_TX_DELAYED) == LORAMAC_TX_DELAYED)) {
        rc = LORAMAC_STATUS_BUSY;
    } else {
        rc = LORAMAC_STATUS_OK;;
    }

    return rc;
}
