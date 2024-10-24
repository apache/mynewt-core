/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * ICBMsg backend.
 *
 * This is an IPC service backend that dynamically allocates buffers for data storage
 * and uses ICMsg to send references to them.
 *
 * Shared memory organization
 * --------------------------
 *
 * Single channel (RX or TX) of the shared memory is divided into two areas: ICMsg area
 * followed by Blocks area. ICMsg is used to send and receive short 3-byte messages.
 * The ICMsg messages are queued inside the ICSM area using PBUF format.
 * Blocks area is evenly divided into aligned blocks. Blocks are used to allocate
 * buffers containing actual data. Data buffers can span multiple blocks. The first block
 * starts with the size of the following data.
 *
 *  +------------+-------------+
 *  | ICMsg area | Blocks area |
 *  +------------+-------------+
 *       _______/               \_________________________________________
 *      /                                                                 \
 *      +-----------+-----------+-----------+-----------+-   -+-----------+
 *      |  Block 0  |  Block 1  |  Block 2  |  Block 3  | ... | Block N-1 |
 *      +-----------+-----------+-----------+-----------+-   -+-----------+
 *            _____/                                     \_____
 *           /                                                 \
 *           +------+--------------------------------+---------+
 *           | size | data_buffer[size] ...          | padding |
 *           +------+--------------------------------+---------+
 *
 * The sender holds information about reserved blocks using bitarray and it is responsible
 * for allocating and releasing the blocks. The receiver just tells the sender that it
 * does not need a specific buffer anymore.
 *
 * Control messages
 * ----------------
 *
 * ICMsg is used to send and receive small 3-byte control messages.
 *
 *  - Send data
 *    | MSG_DATA | endpoint address | block index |
 *    This message is used to send data buffer to specific endpoint.
 *
 *  - Release data
 *    | MSG_RELEASE_DATA | 0 | block index |
 *    This message is a response to the "Send data" message and it is used to inform that
 *    specific buffer is not used anymore and can be released. Endpoint addresses does
 *    not matter here, so it is zero.
 *
 *  - Bound endpoint
 *    | MSG_BOUND | endpoint address | block index |
 *    This message starts the bounding of the endpoint. The buffer contains a
 *    null-terminated endpoint name.
 *
 *  - Release bound endpoint
 *    | MSG_RELEASE_BOUND | endpoint address | block index |
 *    This message is a response to the "Bound endpoint" message and it is used to inform
 *    that a specific buffer (starting at "block index") is not used anymore and
 *    a the endpoint is bounded and can now receive a data.
 *
 * Bounding endpoints
 * ------------------
 *
 * When ICMsg is bounded and user registers an endpoint on initiator side, the backend
 * sends "Bound endpoint". Endpoint address is assigned by the initiator. When follower
 * gets the message and user on follower side also registered the same endpoint,
 * the backend calls "bound" callback and sends back "Release bound endpoint".
 * The follower saves the endpoint address. The follower's endpoint is ready to send
 * and receive data. When the initiator gets "Release bound endpoint" message or any
 * data messages, it calls bound endpoint and it is ready to send data.
 */


#include <errno.h>
#include <assert.h>
#include <os/os.h>
#include <ipc/ipc.h>
#include <icbmsg/icbmsg.h>
#include "utils.h"
#include "pbuf.h"

#if MYNEWT_VAL(MCU_APP_CORE)
__attribute__((section(".ipc0_tx"))) static uint8_t ipc0_tx_start[0x4000];
__attribute__((section(".ipc0_rx"))) static uint8_t ipc0_rx_start[0x4000];
#define TX_BLOCKS_NUM (16)
#define RX_BLOCKS_NUM (24)
#else
__attribute__((section(".ipc0_tx"))) static uint8_t ipc0_rx_start[0x4000];
__attribute__((section(".ipc0_rx"))) static uint8_t ipc0_tx_start[0x4000];
#define TX_BLOCKS_NUM (24)
#define RX_BLOCKS_NUM (16)
#endif

#define ipc0_tx_end (ipc0_tx_start + sizeof(ipc0_tx_start))
#define ipc0_rx_end (ipc0_rx_start + sizeof(ipc0_rx_start))

#define tx_BLOCKS_NUM TX_BLOCKS_NUM
#define rx_BLOCKS_NUM RX_BLOCKS_NUM

/* A string used to synchronize cores */
static const uint8_t magic[] = {0x45, 0x6d, 0x31, 0x6c, 0x31, 0x4b,
                                0x30, 0x72, 0x6e, 0x33, 0x6c, 0x69, 0x34};


#define PBUF_RX_READ_BUF_SIZE 128
static uint8_t icmsg_rx_buffer[PBUF_RX_READ_BUF_SIZE] __attribute__((aligned(4)));

/** Allowed number of endpoints within an IPC instance. */
#define NUM_EPT MYNEWT_VAL(IPC_ICBMSG_NUM_EP)

/** Special endpoint address indicating invalid (or empty) entry. */
#define EPT_ADDR_INVALID 0xFF

/** Size of the header (size field) of the block. */
#define BLOCK_HEADER_SIZE (sizeof(struct block_header))

/** Flag indicating that ICMsg was bounded for this instance. */
#define CONTROL_BOUNDED (1 << 31)

/** Registered endpoints count mask in flags. */
#define FLAG_EPT_COUNT_MASK 0xFFFF

enum icmsg_state {
    ICMSG_STATE_OFF,
    ICMSG_STATE_BUSY,
    ICMSG_STATE_READY,
};

enum msg_type {
    /* Data message. */
    MSG_DATA = 0,
    /* Release data buffer message. */
    MSG_RELEASE_DATA,
    /* Endpoint bounding message. */
    MSG_BOUND,
    /* Release endpoint bound message.
     * This message is also indicator for the receiving side
     * that the endpoint bounding was fully processed on
     * the sender side.
     */
    MSG_RELEASE_BOUND,
};

enum ept_bounding_state {
    /* Endpoint in not configured (initial state). */
    EPT_UNCONFIGURED = 0,
    /* Endpoint is configured, waiting for work queue to
     * start bounding process.
     */
    EPT_CONFIGURED,
    /* Only on initiator. Bound message was send,
     * but bound callback was not called yet, because
     * we are waiting for any incoming messages.
     */
    EPT_BOUNDING,
    /* Bounding is done. Bound callback was called. */
    EPT_READY,
};

struct channel_config {
    /* Address where the blocks start. */
    uint8_t *blocks_ptr;
    /* Size of one block. */
    size_t block_size;
    /* Number of blocks. */
    size_t block_count;
};

struct ipc_instance;

struct ept_data {
    struct ipc_instance *ipc;
    struct ipc_ept_cfg *cfg;
    /* Bounding state. */
    uint32_t state;
    /* Endpoint address. */
    uint8_t addr;
};

struct ipc_instance {
    /* Bit is set when TX block is in use */
    uint8_t tx_usage_bitmap[DIV_ROUND_UP(TX_BLOCKS_NUM, 8)];
    /* Bit is set, if the buffer starting at
     * this block should be kept after exit
     * from receive handler.
     */
    uint8_t rx_usage_bitmap[DIV_ROUND_UP(RX_BLOCKS_NUM, 8)];
    /* TX ICSMsg Area packet buffer */
    struct pbuf tx_pb;
    /* RX ICSMsg Area packet buffer */
    struct pbuf rx_pb;
    /* TX channel config. */
    struct channel_config tx;
    /* RX channel config. */
    struct channel_config rx;
    /* Array of registered endpoints. */
    struct ept_data ept[NUM_EPT];
    /* Flags on higher bits, number of registered
     * endpoints on lower.
     */
    uint32_t flags;
    /* This side has an initiator role. */
    bool is_initiator;
    uint8_t state;
    uint8_t ipc_id;
};
struct block_header {
    /* Size of the data field. It must be volatile, because
     * when this value is read and validated for security
     * reasons, compiler cannot generate code that reads
     * it again after validation.
     */
    volatile size_t size;
};

struct block_content {
    struct block_header header;
    /* Buffer data. */
    uint8_t data[];
};

struct control_message {
    /* Message type. */
    uint8_t msg_type;
    /* Endpoint address or zero for MSG_RELEASE_DATA. */
    uint8_t ept_addr;
    /* Block index to send or release. */
    uint8_t block_index;
};

static struct ipc_instance ipc_instances[1];

/**
 * Calculate pointer to block from its index and channel configuration (RX or TX).
 * No validation is performed.
 */
static struct block_content *
block_from_index(const struct channel_config *ch_conf, size_t block_index)
{
    return (struct block_content *)(ch_conf->blocks_ptr + block_index * ch_conf->block_size);
}

/**
 * Calculate pointer to data buffer from block index and channel configuration (RX or TX).
 * Also validate the index and optionally the buffer size allocated on the this block.
 */
static uint8_t *
buffer_from_index_validate(const struct channel_config *ch_conf,
                           size_t block_index, size_t *size)
{
    size_t allocable_size;
    size_t buffer_size;
    uint8_t *end_ptr;
    struct block_content *block;

    if (block_index >= ch_conf->block_count) {
        /* Block index invalid */
        return NULL;
    }

    block = block_from_index(ch_conf, block_index);

    if (size != NULL) {
        allocable_size = ch_conf->block_count * ch_conf->block_size;
        end_ptr = ch_conf->blocks_ptr + allocable_size;
        buffer_size = block->header.size;

        if ((buffer_size > allocable_size - BLOCK_HEADER_SIZE) ||
            (&block->data[buffer_size] > end_ptr)) {
            /* Block corrupted */
            return NULL;
        }

        *size = buffer_size;
    }

    return block->data;
}

static int
find_zero_bits(uint8_t bitmap[], size_t total_bits,
               size_t n, size_t *start_index)
{
    size_t zero_count = 0;
    size_t first_zero_bit_pos;
    size_t bit_id;
    size_t byte_id;
    uint8_t bit_pos;

    /* Find the first sequence of n consecutive 0 bits */
    for (bit_id = 0; bit_id < total_bits; ++bit_id) {
        byte_id = bit_id / 8;
        bit_pos = bit_id % 8;

        if ((bitmap[byte_id] & (1 << bit_pos)) == 0) {
            if (zero_count == 0) {
                first_zero_bit_pos = bit_id;
            }
            zero_count++;

            if (zero_count == n) {
                *start_index = first_zero_bit_pos;
                return 0;
            }
        } else {
            zero_count = 0;
        }
    }

    return -1;
}

static void
alloc_bitmap_bits(uint8_t bitmap[], size_t n, size_t start_index)
{
    for (size_t i = 0; i < n; ++i) {
        size_t bit_index = start_index + i;
        size_t byte_index = bit_index / 8;
        size_t bit_pos = bit_index % 8;

        bitmap[byte_index] |= (1 << bit_pos);
    }
}

static void
free_bitmap_bits(uint8_t bitmap[], size_t n, size_t start_index)
{
    for (size_t i = 0; i < n; ++i) {
        size_t bit_index = start_index + i;
        size_t byte_index = bit_index / 8;
        size_t bit_pos = bit_index % 8;

        bitmap[byte_index] &= ~(1 << bit_pos);
    }
}

static int
alloc_tx_buffer(struct ipc_instance *ipc, uint32_t size, uint8_t **buffer,
                size_t *tx_block_index)
{
    int rc;
    struct block_content *block;
    size_t total_bits = sizeof(ipc->tx_usage_bitmap) * 8;
    size_t total_size = size + BLOCK_HEADER_SIZE;
    size_t num_blocks = DIV_ROUND_UP(total_size, ipc->tx.block_size);

    rc = find_zero_bits(ipc->tx_usage_bitmap, total_bits,
                        num_blocks, tx_block_index);
    if (rc) {
        return rc;
    }

    alloc_bitmap_bits(ipc->tx_usage_bitmap, num_blocks, *tx_block_index);

    /* Get block pointer and adjust size to actually allocated space. */
    size = ipc->tx.block_size * num_blocks - BLOCK_HEADER_SIZE;
    block = block_from_index(&ipc->tx, *tx_block_index);
    block->header.size = size;
    *buffer = block->data;

    return 0;
}

/**
 * Allocate buffer for transmission.
 */
int
ipc_icbmsg_alloc_tx_buf(uint8_t ipc_id, struct ipc_icmsg_buf *buf, uint32_t size)
{
    struct ipc_instance *ipc = &ipc_instances[ipc_id];

    return alloc_tx_buffer(ipc, size, &buf->data, &buf->block_id);
}

/**
 * Release all or part of the blocks occupied by the buffer.
 */
static void
release_tx_blocks(struct ipc_instance *ipc, size_t release_index, size_t size)
{
    size_t num_blocks;
    size_t total_size;

    /* Calculate number of blocks. */
    total_size = size + BLOCK_HEADER_SIZE;
    num_blocks = DIV_ROUND_UP(total_size, ipc->tx.block_size);

    if (num_blocks > 0) {
        free_bitmap_bits(ipc->tx_usage_bitmap, num_blocks, release_index);
    }
}

/**
 * Send control message over ICMsg with mutex locked. Mutex must be locked because
 * ICMsg may return error on concurrent invocations even when there is enough space
 * in queue.
 */
static int
send_control_message(struct ept_data *ept, enum msg_type msg_type, uint8_t block_index)
{
    int ret;
    struct ipc_instance *ipc = ept->ipc;

    const struct control_message message = {
        .msg_type = (uint8_t)msg_type,
        .ept_addr = ept->addr,
        .block_index = block_index,
    };

    if (ipc->state != ICMSG_STATE_READY) {
        return -EBUSY;
    }

    ret = pbuf_write(&ipc->tx_pb, (const char *)&message, sizeof(message));

    if (ret < 0) {
        return ret;
    } else if (ret < sizeof(message)) {
        return -EBADMSG;
    }

    ipc_signal(ept->cfg->tx_channel);

    return 0;
}

/**
 * Send data contained in specified block.
 */
static int
send_block(struct ept_data *ept, enum msg_type msg_type, size_t tx_block_index, size_t size)
{
    int r;
    struct ipc_instance *ipc = ept->ipc;
    struct block_content *block;

    block = block_from_index(&ipc->tx, tx_block_index);
    block->header.size = size;

    r = send_control_message(ept, msg_type, tx_block_index);
    if (r < 0) {
        release_tx_blocks(ipc, tx_block_index, size);
    }

    return r;
}

/**
 * Find endpoint that was registered with name that matches name
 * contained in the endpoint bound message received from remote.
 */
static int
find_ept_by_name(struct ipc_instance *ipc, const char *name)
{
    const struct channel_config *rx_conf = &ipc->rx;
    const char *buffer_end = (const char *)rx_conf->blocks_ptr +
                             rx_conf->block_count * rx_conf->block_size;
    struct ept_data *ept;
    size_t name_size;
    size_t i;

    /* Requested name is in shared memory, so we have to assume that it
     * can be corrupted. Extra care must be taken to avoid out of
     * bounds reads.
     */
    name_size = strnlen(name, buffer_end - name - 1) + 1;

    for (i = 0; i < NUM_EPT; i++) {
        ept = &ipc->ept[i];
        if (ept->state == EPT_CONFIGURED &&
            strncmp(ept->cfg->name, name, name_size) == 0) {
            return i;
        }
    }

    return -ENOENT;
}

/**
 * Send bound message on specified endpoint.
 */
static int
send_bound_message(struct ept_data *ept)
{
    int rc;
    size_t msg_len;
    uint8_t *buffer;
    size_t tx_block_index;

    msg_len = strlen(ept->cfg->name) + 1;
    rc = alloc_tx_buffer(ept->ipc, msg_len, &buffer, &tx_block_index);
    if (rc >= 0) {
        strcpy((char *)buffer, ept->cfg->name);
        rc = send_block(ept, MSG_BOUND, tx_block_index, msg_len);
    }

    return rc;
}

/**
 * Get endpoint from endpoint address. Also validates if the address is correct and
 * endpoint is in correct state for receiving.
 */
static struct ept_data *
get_ept_and_rx_validate(struct ipc_instance *ipc, uint8_t ept_addr)
{
    struct ept_data *ept;

    if (ept_addr >= NUM_EPT) {
        return NULL;
    }

    ept = &ipc->ept[ept_addr];

    if (ept->state == EPT_READY) {
        /* Valid state - nothing to do. */
    } else if (ept->state == EPT_BOUNDING) {
        /* The remote endpoint is ready */
        ept->state = EPT_READY;
    } else {
        return NULL;
    }

    return ept;
}

/**
 * Data message received.
 */
static int
received_data(struct ipc_instance *ipc, size_t rx_block_index, uint8_t ept_addr)
{
    uint8_t *buffer;
    struct ept_data *ept;
    size_t size;

    /* Validate. */
    buffer = buffer_from_index_validate(&ipc->rx, rx_block_index, &size);
    ept = get_ept_and_rx_validate(ipc, ept_addr);
    if (buffer == NULL || ept == NULL) {
        return -EINVAL;
    }

    /* Call the endpoint callback. */
    ept->cfg->cb.received(buffer, size, ept->cfg->user_data);

    /* Release the buffer */
    send_control_message(ept, MSG_RELEASE_DATA, rx_block_index);

    return 0;
}

/**
 * Release data message received.
 */
static int
received_release_data(struct ipc_instance *ipc, size_t tx_block_index)
{
    size_t size;
    uint8_t *buffer;

    /* Validate. */
    buffer = buffer_from_index_validate(&ipc->tx, tx_block_index, &size);
    if (buffer == NULL) {
        return -EINVAL;
    }

    /* Release. */
    release_tx_blocks(ipc, tx_block_index, size);

    return 0;
}

/**
 * Bound endpoint message received.
 */
static int
received_bound(struct ipc_instance *ipc, size_t rx_block_index, uint8_t rem_ept_addr)
{
    struct ept_data *ept;
    uint8_t *buffer;
    size_t size;
    uint8_t ept_addr;

    /* Validate */
    buffer = buffer_from_index_validate(&ipc->rx, rx_block_index, &size);
    if (buffer == NULL) {
        /* Received invalid block index */
        return -1;
    }

    ept_addr = find_ept_by_name(ipc, (const char *)buffer);
    if (ept_addr < 0) {
        return 0;
    }

    /* Set the remote endpoint address */
    ept = &ipc->ept[ept_addr];
    ept->addr = rem_ept_addr;

    if (ept->state != EPT_CONFIGURED) {
        /* Unexpected bounding from remote on endpoint */
        return -EINVAL;
    }

    ept->state = EPT_READY;

    send_control_message(ept, MSG_RELEASE_BOUND, rx_block_index);

    return 0;
}

/**
 * Handles ICMsg control messages received from the remote.
 */
static void
control_received(struct ipc_instance *ipc, const struct control_message *message)
{
    uint8_t ept_addr;

    ept_addr = message->ept_addr;
    if (ept_addr >= NUM_EPT) {
        return;
    }

    switch (message->msg_type) {
    case MSG_RELEASE_DATA:
        received_release_data(ipc, message->block_index);
        break;
    case MSG_RELEASE_BOUND:
        assert(received_release_data(ipc, message->block_index) == 0);
        assert(get_ept_and_rx_validate(ipc, ept_addr) != NULL);
        break;
    case MSG_BOUND:
        received_bound(ipc, message->block_index, ept_addr);
        break;
    case MSG_DATA:
        received_data(ipc, message->block_index, ept_addr);
        break;
    default:
        /* Silently ignore other messages types. They can be used in future
         * protocol version.
         */
        break;
    }
}

void
ipc_process_signal(uint8_t ipc_id)
{
    int icmsg_len;
    struct ipc_instance *ipc = &ipc_instances[ipc_id];

    icmsg_len = pbuf_read(&ipc->rx_pb, NULL, 0);
    if (icmsg_len <= 0) {
        /* Unlikely, no data in buffer. */
        return;
    }

    if (sizeof(icmsg_rx_buffer) < icmsg_len) {
        return;
    }

    icmsg_len = pbuf_read(&ipc->rx_pb, (char *)icmsg_rx_buffer, sizeof(icmsg_rx_buffer));

    if (ipc->state == ICMSG_STATE_READY) {
        if (icmsg_len < sizeof(struct control_message)) {
            return;
        }

        control_received(ipc, (const struct control_message *)icmsg_rx_buffer);
    } else {
        /* After core restart, the first message in ICMsg area should be
         * the magic string.
         */
        assert(ipc->state == ICMSG_STATE_BUSY);

        /* Allow magic number longer than sizeof(magic) for future protocol version. */
        bool endpoint_invalid = (icmsg_len < sizeof(magic) ||
                                 memcmp(magic, icmsg_rx_buffer, sizeof(magic)));

        assert(!endpoint_invalid);

        /* Set flag that ICMsg is bounded and now, endpoint bounding may start. */
        ipc->flags |= CONTROL_BOUNDED;
        ipc->state = ICMSG_STATE_READY;
    }
}

/**
 * Send to endpoint (without copy).
 */
int
ipc_icbmsg_send_buf(uint8_t ipc_id, uint8_t ept_addr, struct ipc_icmsg_buf *buf)
{
    struct ipc_instance *ipc = &ipc_instances[ipc_id];
    struct ept_data *ept = &ipc->ept[ept_addr];

    /* Send data message. */
    return send_block(ept, MSG_DATA, buf->block_id, buf->len);
}

/**
 * Send to endpoint (with copy).
 */
int
ipc_icbmsg_send(uint8_t ipc_id, uint8_t ept_addr, const void *data, uint16_t len)
{
    int rc;
    uint8_t *buffer;
    struct ipc_instance *ipc = &ipc_instances[ipc_id];
    struct ept_data *ept = &ipc->ept[ept_addr];
    size_t tx_block_index;

    /* Allocate the buffer. */
    rc = alloc_tx_buffer(ipc, len, &buffer, &tx_block_index);
    if (rc < 0) {
        return rc;
    }

    /* Copy data to allocated buffer. */
    memcpy(buffer, data, len);

    /* Send data message. */
    rc = send_block(ept, MSG_DATA, tx_block_index, len);
    if (rc < 0) {
        return rc;
    }

    return 0;
}

/**
 * Register new endpoint
 */
uint8_t
ipc_icmsg_register_ept(uint8_t ipc_id, struct ipc_ept_cfg *cfg)
{
    int rc;
    struct ipc_instance *ipc;
    struct ept_data *ept = NULL;
    uint8_t ept_addr;

    assert(ipc_id < sizeof(ipc_instances));
    ipc = &ipc_instances[ipc_id];

    /* Reserve new endpoint index. */
    ept_addr = (++ipc->flags) & FLAG_EPT_COUNT_MASK;
    assert(ept_addr < NUM_EPT);

    /* Add new endpoint. */
    ept = &ipc->ept[ept_addr];
    ept->ipc = ipc;
    ept->state = EPT_CONFIGURED;
    ept->cfg = cfg;

    if (ipc->is_initiator) {
        ept->addr = ept_addr;
        ept->state = EPT_BOUNDING;

        rc = send_bound_message(ept);
        assert(rc == 0);
    } else {
        ept->addr = EPT_ADDR_INVALID;
    }

    return ept_addr;
}

/**
 * Number of bytes per each ICMsg message. It is used to calculate size of ICMsg area.
 */
#define BYTES_PER_ICMSG_MESSAGE (ROUND_UP(sizeof(struct control_message),      \
                                          sizeof(void *)) + PBUF_PACKET_LEN_SZ)

/**
 * Maximum ICMsg overhead. It is used to calculate size of ICMsg area.
 */
#define ICMSG_BUFFER_OVERHEAD(i) \
    (PBUF_HEADER_OVERHEAD(GET_CACHE_ALIGNMENT(i)) + 2 * BYTES_PER_ICMSG_MESSAGE)

/**
 * Returns required block alignment for instance "i".
 */
#define GET_CACHE_ALIGNMENT(i) (sizeof(uint32_t))

/**
 * Calculates minimum size required for ICMsg region for specific number of local
 * and remote blocks. The minimum size ensures that ICMsg queue is will never overflow
 * because it can hold data message for each local block and release message
 * for each remote block.
 */
#define GET_ICMSG_MIN_SIZE(i, local_blocks, remote_blocks) \
    (ICMSG_BUFFER_OVERHEAD(i) + BYTES_PER_ICMSG_MESSAGE *  \
     (local_blocks + remote_blocks))

/**
 * Calculate aligned block size by evenly dividing remaining space after removing
 * the space for ICMsg.
 */
#define GET_BLOCK_SIZE(i, total_size, local_blocks, remote_blocks) ROUND_DOWN(     \
        ((total_size) - GET_ICMSG_MIN_SIZE(i, (local_blocks), (remote_blocks))) /  \
        (local_blocks), GET_CACHE_ALIGNMENT(i))

/**
 * Calculate offset where area for blocks starts which is just after the ICMsg.
 */
#define GET_BLOCKS_OFFSET(i, total_size, local_blocks, remote_blocks) \
    ((total_size) - GET_BLOCK_SIZE(i, (total_size), (local_blocks),   \
                                   (remote_blocks)) * (local_blocks))

/**
 * Return shared memory start address aligned to block alignment and cache line.
 */
#define GET_MEM_ADDR_INST(i, direction)                               \
    ROUND_UP(ipc ## i ## _ ## direction ## _start, GET_CACHE_ALIGNMENT(i))

/**
 * Return shared memory end address aligned to block alignment and cache line.
 */
#define GET_MEM_END_INST(i, direction)                                \
    ROUND_DOWN(ipc ## i ## _ ## direction ## _end, GET_CACHE_ALIGNMENT(i))

/**
 * Return shared memory size aligned to block alignment and cache line.
 */
#define GET_MEM_SIZE_INST(i, direction)                               \
    (GET_MEM_END_INST(i, direction) - GET_MEM_ADDR_INST(i, direction))

/**
 * Returns GET_ICMSG_SIZE, but for specific instance and direction.
 * 'loc' and 'rem' parameters tells the direction. They can be either "tx, rx"
 *  or "rx, tx".
 */
#define GET_ICMSG_SIZE_INST(i, loc, rem)         \
    GET_BLOCKS_OFFSET(i,                         \
                      GET_MEM_SIZE_INST(i, loc), \
                      loc ## _BLOCKS_NUM,          \
                      rem ## _BLOCKS_NUM)

/**
 * Returns address where area for blocks starts for specific instance and direction.
 * 'loc' and 'rem' parameters tells the direction. They can be either "tx, rx"
 *  or "rx, tx".
 */
#define GET_BLOCKS_ADDR_INST(i, loc, rem) \
    GET_MEM_ADDR_INST(i, loc) +       \
    GET_ICMSG_SIZE_INST(i, loc, rem)

/**
 * Returns block size for specific instance and direction.
 * 'loc' and 'rem' parameters tells the direction. They can be either "tx, rx"
 *  or "rx, tx".
 */
#define GET_BLOCK_SIZE_INST(i, loc, rem)          \
    GET_BLOCK_SIZE(i,                         \
                   GET_MEM_SIZE_INST(i, loc), \
                   loc ## _BLOCKS_NUM,          \
                   rem ## _BLOCKS_NUM)

#define IPC_INSTANCE_INIT(i) (struct ipc_instance) {               \
        .tx_pb = {                                                     \
            .cfg = PBUF_CFG_INIT(GET_MEM_ADDR_INST(i, tx),             \
                                 GET_ICMSG_SIZE_INST(i, tx, rx),       \
                                 GET_CACHE_ALIGNMENT(i)),              \
        },                                                             \
        .rx_pb = {                                                     \
            .cfg = PBUF_CFG_INIT(GET_MEM_ADDR_INST(i, rx),             \
                                 GET_ICMSG_SIZE_INST(i, rx, tx),       \
                                 GET_CACHE_ALIGNMENT(i)),              \
        },                                                             \
        .tx = {                                                        \
            .blocks_ptr = (uint8_t *)GET_BLOCKS_ADDR_INST(i, tx, rx),  \
            .block_count = TX_BLOCKS_NUM,                              \
            .block_size = GET_BLOCK_SIZE_INST(i, tx, rx),              \
        },                                                             \
        .rx = {                                                        \
            .blocks_ptr = (uint8_t *)GET_BLOCKS_ADDR_INST(i, rx, tx),  \
            .block_count = RX_BLOCKS_NUM,                              \
            .block_size = GET_BLOCK_SIZE_INST(i, rx, tx),              \
        },                                                             \
}

/**
 * Backend initialization.
 */
int
ipc_open(uint8_t ipc_id)
{
    int rc;
    struct ipc_instance *ipc;

    assert(ipc_id < sizeof(ipc_instances));

    ipc = &ipc_instances[ipc_id];
    *ipc = IPC_INSTANCE_INIT(0);

    assert(ipc->state == ICMSG_STATE_OFF);
    ipc->state = ICMSG_STATE_BUSY;

    memset(ipc->tx_usage_bitmap, 0, DIV_ROUND_UP(TX_BLOCKS_NUM, 8));
    memset(ipc->rx_usage_bitmap, 0, DIV_ROUND_UP(RX_BLOCKS_NUM, 8));

    ipc->is_initiator = (ipc->rx.blocks_ptr < ipc->tx.blocks_ptr);

    rc = pbuf_init(&ipc->tx_pb);
    assert(rc == 0);

    /* Initialize local copies of rx_pb. */
    ipc->rx_pb.data.wr_idx = 0;
    ipc->rx_pb.data.rd_idx = 0;

    rc = pbuf_write(&ipc->tx_pb, (char *)magic, sizeof(magic));
    assert(rc == sizeof(magic));

    return 0;
}

uint8_t
ipc_ready(uint8_t ipc_id)
{
    struct ipc_instance *ipc = &ipc_instances[ipc_id];

    if (ipc->state == ICMSG_STATE_READY) {
        return 1;
    }

    return 0;
}

uint8_t
ipc_icsmsg_ept_ready(uint8_t ipc_id, uint8_t ept_addr)
{
    struct ipc_instance *ipc = &ipc_instances[ipc_id];

    if (ipc->ept[ept_addr].state == EPT_READY) {
        return 1;
    }

    return 0;
}
