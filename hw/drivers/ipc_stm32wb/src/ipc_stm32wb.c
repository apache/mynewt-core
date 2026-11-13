/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <assert.h>
#include <string.h>
#include <os/mynewt.h>
#include <os/endian.h>
#include <mcu/stm32_hal.h>
#include <stm32wbxx_ll_pwr.h>
#include <ipc_stm32wb/ipc_stm32wb.h>

/*
 * CPU2 finds all mailbox structures through reference table located at
 * the beginning of SRAM2a (FLASH_IPCCBR = 0). Lists are circular doubly
 * linked lists with nodes embedded at the beginning of each packet.
 *
 * IPCC channels (same number is used in both directions):
 *  1 - BLE command (CPU1->CPU2), BLE events and ACL data (CPU2->CPU1)
 *  2 - system command (CPU1->CPU2), system events (CPU2->CPU1)
 *  4 - release of event buffers back to CPU2 (CPU1->CPU2)
 *  6 - BLE ACL data (CPU1->CPU2)
 */
#define CH_BLE                  1
#define CH_SYS                  2
#define CH_MM_RELEASE           4
#define CH_BLE_ACL              6
#define CH_BIT(ch)              (1U << ((ch) - 1))
#define CH_ALL                  0x3fU

#define PKT_HDR_SIZE            8
/* Header, packet type, HCI header (4 for ACL) and 255 bytes of payload */
#define CMD_BUF_SIZE            268
#define EVT_BUF_SIZE            268
#define ACL_BUF_SIZE            264
#define ACL_MAX_LEN             (ACL_BUF_SIZE - PKT_HDR_SIZE - 1)
#define CS_BUF_SIZE             16

#define SYS_EVT_ASYNC           0xff
#define SYS_SUBEVT_C2_READY     0x9200
#define SYS_C2_READY_WS_RUNNING 0x00

#define HCI_EVT_CMD_COMPLETE    0x0e

struct mb_node {
    struct mb_node *next;
    struct mb_node *prev;
};

struct mb_device_info_table {
    uint32_t safeboot_version;
    uint32_t fus_version;
    uint32_t fus_memory_size;
    uint32_t fus_info;
    uint32_t ws_version;
    uint32_t ws_memory_size;
    uint32_t ws_info_stack;
    uint32_t ws_reserved;
};

struct mb_ble_table {
    uint8_t *cmd_buffer;
    uint8_t *cs_buffer;
    struct mb_node *evt_queue;
    uint8_t *acl_buffer;
};

struct mb_sys_table {
    uint8_t *cmd_buffer;
    struct mb_node *evt_queue;
};

struct mb_mem_manager_table {
    uint8_t *ble_spare_buffer;
    uint8_t *sys_spare_buffer;
    uint8_t *evt_pool;
    uint32_t evt_pool_size;
    struct mb_node *evt_free_queue;
    uint8_t *traces_evt_pool;
    uint32_t traces_evt_pool_size;
};

struct mb_ref_table {
    struct mb_device_info_table *device_info_table;
    struct mb_ble_table *ble_table;
    void *thread_table;
    struct mb_sys_table *sys_table;
    struct mb_mem_manager_table *mem_manager_table;
    void *traces_table;
    void *mac_802_15_4_table;
    void *zigbee_table;
    void *lld_tests_table;
    void *ble_lld_table;
};

#define MB_REF_TABLE __attribute__((section(".mb_ref_table"), used))
#define MB_MEM       __attribute__((section(".mb_mem"), aligned(4)))

static struct mb_ref_table ref_table MB_REF_TABLE;
static struct mb_device_info_table device_info_table MB_MEM;
static struct mb_ble_table ble_table MB_MEM;
static struct mb_sys_table sys_table MB_MEM;
static struct mb_mem_manager_table mem_manager_table MB_MEM;
static struct mb_node ble_evt_queue MB_MEM;
static struct mb_node sys_evt_queue MB_MEM;
static struct mb_node evt_free_queue MB_MEM;
static uint8_t ble_cmd_buffer[CMD_BUF_SIZE] MB_MEM;
static uint8_t ble_cs_buffer[CS_BUF_SIZE] MB_MEM;
static uint8_t ble_acl_buffer[ACL_BUF_SIZE] MB_MEM;
static uint8_t sys_cmd_buffer[CMD_BUF_SIZE] MB_MEM;
static uint8_t ble_spare_buffer[EVT_BUF_SIZE] MB_MEM;
static uint8_t sys_spare_buffer[EVT_BUF_SIZE] MB_MEM;
static uint8_t evt_pool[MYNEWT_VAL(IPC_STM32WB_EVT_POOL_COUNT) * EVT_BUF_SIZE] MB_MEM;

/* CPU1 private lists */
static struct mb_node ble_rx_queue;
static struct mb_node free_pending_queue;

static struct os_sem ready_sem;
static struct os_sem sys_rsp_sem;
static volatile int8_t c2_ready_status = -1;

static ipc_stm32wb_cb *ble_rx_cb;
static ipc_stm32wb_cb *ble_tx_cb;
static void *ble_cb_arg;

static void
mb_list_init(struct mb_node *head)
{
    head->next = head;
    head->prev = head;
}

static void
mb_list_insert_tail(struct mb_node *head, struct mb_node *node)
{
    node->next = head;
    node->prev = head->prev;
    head->prev->next = node;
    head->prev = node;
}

static struct mb_node *
mb_list_remove_head(struct mb_node *head)
{
    struct mb_node *node = head->next;

    if (node == head) {
        return NULL;
    }
    head->next = node->next;
    node->next->prev = head;

    return node;
}

static void
ipcc_tx_set(int ch)
{
    IPCC->C1SCR = CH_BIT(ch) << IPCC_C1SCR_CH1S_Pos;
}

static bool
ipcc_tx_busy(int ch)
{
    return (IPCC->C1TOC2SR & CH_BIT(ch)) != 0;
}

static void
ipcc_tx_free_irq_enable(int ch)
{
    IPCC->C1MR &= ~(CH_BIT(ch) << IPCC_C1MR_CH1FM_Pos);
}

static void
ipcc_tx_free_irq_disable(int ch)
{
    IPCC->C1MR |= CH_BIT(ch) << IPCC_C1MR_CH1FM_Pos;
}

static void
ipcc_rx_irq_enable(int ch)
{
    IPCC->C1MR &= ~(CH_BIT(ch) << IPCC_C1MR_CH1OM_Pos);
}

static void
ipcc_rx_clear(int ch)
{
    IPCC->C1SCR = CH_BIT(ch) << IPCC_C1SCR_CH1C_Pos;
}

/* Moves released buffers to CPU2 queue; release channel must be free */
static void
mm_release_flush(void)
{
    struct mb_node *node;

    while ((node = mb_list_remove_head(&free_pending_queue)) != NULL) {
        mb_list_insert_tail(&evt_free_queue, node);
    }
    ipcc_tx_set(CH_MM_RELEASE);
}

static void
mm_release(struct mb_node *node)
{
    os_sr_t sr;

    OS_ENTER_CRITICAL(sr);
    mb_list_insert_tail(&free_pending_queue, node);
    if (ipcc_tx_busy(CH_MM_RELEASE)) {
        ipcc_tx_free_irq_enable(CH_MM_RELEASE);
    } else {
        mm_release_flush();
    }
    OS_EXIT_CRITICAL(sr);
}

static void
sys_evt_handle(const uint8_t *pkt)
{
    /* pkt points to packet type */
    if (pkt[0] == IPC_STM32WB_PKT_SYS_EVT && pkt[1] == SYS_EVT_ASYNC &&
        get_le16(&pkt[3]) == SYS_SUBEVT_C2_READY) {
        c2_ready_status = pkt[5];
        os_sem_release(&ready_sem);
    }
}

static void
ipcc_rx_isr(void)
{
    struct mb_node *node;
    uint32_t pending;

    os_trace_isr_enter();

    pending = IPCC->C2TOC1SR & ~IPCC->C1MR & CH_ALL;

    if (pending & CH_BIT(CH_SYS)) {
        while ((node = mb_list_remove_head(&sys_evt_queue)) != NULL) {
            sys_evt_handle((uint8_t *)node + PKT_HDR_SIZE);
            mm_release(node);
        }
        ipcc_rx_clear(CH_SYS);
    }

    if (pending & CH_BIT(CH_BLE)) {
        while ((node = mb_list_remove_head(&ble_evt_queue)) != NULL) {
            mb_list_insert_tail(&ble_rx_queue, node);
        }
        ipcc_rx_clear(CH_BLE);
        if (ble_rx_cb) {
            ble_rx_cb(ble_cb_arg);
        }
    }

    os_trace_isr_exit();
}

static void
ipcc_tx_isr(void)
{
    uint32_t pending;

    os_trace_isr_enter();

    /* Channels which are free and have free interrupt unmasked */
    pending = ~IPCC->C1TOC2SR & ~(IPCC->C1MR >> IPCC_C1MR_CH1FM_Pos) & CH_ALL;

    if (pending & CH_BIT(CH_SYS)) {
        ipcc_tx_free_irq_disable(CH_SYS);
        os_sem_release(&sys_rsp_sem);
    }

    if (pending & CH_BIT(CH_MM_RELEASE)) {
        ipcc_tx_free_irq_disable(CH_MM_RELEASE);
        mm_release_flush();
    }

    if (pending & (CH_BIT(CH_BLE) | CH_BIT(CH_BLE_ACL))) {
        if (pending & CH_BIT(CH_BLE)) {
            ipcc_tx_free_irq_disable(CH_BLE);
        }
        if (pending & CH_BIT(CH_BLE_ACL)) {
            ipcc_tx_free_irq_disable(CH_BLE_ACL);
        }
        if (ble_tx_cb) {
            ble_tx_cb(ble_cb_arg);
        }
    }

    os_trace_isr_exit();
}

int
ipc_stm32wb_wait_ready(uint32_t timeout_ms)
{
    os_time_t ticks;
    int rc;

    if (c2_ready_status < 0) {
        os_time_ms_to_ticks(timeout_ms, &ticks);
        rc = os_sem_pend(&ready_sem, ticks);
        if (rc) {
            return SYS_ETIMEOUT;
        }
        /* Let other waiters (if any) through as well */
        os_sem_release(&ready_sem);
    }

    return c2_ready_status == SYS_C2_READY_WS_RUNNING ? 0 : SYS_ENOENT;
}

void
ipc_stm32wb_fw_info_get(struct ipc_stm32wb_fw_info *info)
{
    info->safeboot_version = device_info_table.safeboot_version;
    info->fus_version = device_info_table.fus_version;
    info->fus_memory_size = device_info_table.fus_memory_size;
    info->ws_version = device_info_table.ws_version;
    info->ws_memory_size = device_info_table.ws_memory_size;
    info->ws_info_stack = device_info_table.ws_info_stack;
}

int
ipc_stm32wb_sys_cmd(uint16_t opcode, const void *params, uint8_t len, uint8_t *status)
{
    uint8_t *pkt = sys_cmd_buffer + PKT_HDR_SIZE;
    os_time_t ticks;
    os_sr_t sr;
    int rc;

    assert(!ipcc_tx_busy(CH_SYS));

    pkt[0] = IPC_STM32WB_PKT_SYS_CMD;
    put_le16(&pkt[1], opcode);
    pkt[3] = len;
    memcpy(&pkt[4], params, len);

    OS_ENTER_CRITICAL(sr);
    ipcc_tx_set(CH_SYS);
    ipcc_tx_free_irq_enable(CH_SYS);
    OS_EXIT_CRITICAL(sr);

    os_time_ms_to_ticks(MYNEWT_VAL(IPC_STM32WB_SYS_CMD_TIMEOUT_MS), &ticks);
    rc = os_sem_pend(&sys_rsp_sem, ticks);
    if (rc) {
        return SYS_ETIMEOUT;
    }

    /*
     * CPU2 writes command complete event in place of command:
     * type, event code, length, num cmd, opcode, status
     */
    if (pkt[1] != HCI_EVT_CMD_COMPLETE || get_le16(&pkt[4]) != opcode) {
        return SYS_EUNKNOWN;
    }
    if (status) {
        *status = pkt[6];
    }

    return 0;
}

void
ipc_stm32wb_ble_cb_set(ipc_stm32wb_cb *rx_cb, ipc_stm32wb_cb *tx_cb, void *arg)
{
    ble_cb_arg = arg;
    ble_rx_cb = rx_cb;
    ble_tx_cb = tx_cb;
}

void
ipc_stm32wb_ble_enable(void)
{
    os_sr_t sr;

    OS_ENTER_CRITICAL(sr);
    ipcc_rx_irq_enable(CH_BLE);
    OS_EXIT_CRITICAL(sr);
}

int
ipc_stm32wb_ble_cmd_send(const uint8_t *cmd)
{
    uint8_t *pkt = ble_cmd_buffer + PKT_HDR_SIZE;
    os_sr_t sr;
    int rc = 0;

    OS_ENTER_CRITICAL(sr);
    if (ipcc_tx_busy(CH_BLE)) {
        ipcc_tx_free_irq_enable(CH_BLE);
        rc = SYS_EBUSY;
    }
    OS_EXIT_CRITICAL(sr);

    if (rc == 0) {
        pkt[0] = IPC_STM32WB_PKT_HCI_CMD;
        memcpy(&pkt[1], cmd, 3 + cmd[2]);
        ipcc_tx_set(CH_BLE);
    }

    return rc;
}

int
ipc_stm32wb_ble_acl_send(const struct os_mbuf *om)
{
    uint8_t *pkt = ble_acl_buffer + PKT_HDR_SIZE;
    uint16_t len = OS_MBUF_PKTLEN(om);
    os_sr_t sr;
    int rc = 0;

    if (len > ACL_MAX_LEN) {
        return SYS_EINVAL;
    }

    OS_ENTER_CRITICAL(sr);
    if (ipcc_tx_busy(CH_BLE_ACL)) {
        rc = SYS_EBUSY;
    }
    OS_EXIT_CRITICAL(sr);

    if (rc == 0) {
        pkt[0] = IPC_STM32WB_PKT_HCI_ACL;
        os_mbuf_copydata(om, 0, len, &pkt[1]);
        OS_ENTER_CRITICAL(sr);
        ipcc_tx_set(CH_BLE_ACL);
        /* CPU2 frees the channel when data is consumed */
        ipcc_tx_free_irq_enable(CH_BLE_ACL);
        OS_EXIT_CRITICAL(sr);
    }

    return rc;
}

uint8_t *
ipc_stm32wb_ble_rx_get(void)
{
    struct mb_node *node;
    os_sr_t sr;

    OS_ENTER_CRITICAL(sr);
    node = mb_list_remove_head(&ble_rx_queue);
    OS_EXIT_CRITICAL(sr);

    return node ? (uint8_t *)node + PKT_HDR_SIZE : NULL;
}

void
ipc_stm32wb_ble_rx_release(uint8_t *pkt)
{
    mm_release((struct mb_node *)(pkt - PKT_HDR_SIZE));
}

static void
mailbox_init(void)
{
    memset(&ref_table, 0, sizeof(ref_table));
    memset(&device_info_table, 0, sizeof(device_info_table));

    mb_list_init(&ble_evt_queue);
    mb_list_init(&sys_evt_queue);
    mb_list_init(&evt_free_queue);
    mb_list_init(&ble_rx_queue);
    mb_list_init(&free_pending_queue);

    ble_table.cmd_buffer = ble_cmd_buffer;
    ble_table.cs_buffer = ble_cs_buffer;
    ble_table.evt_queue = &ble_evt_queue;
    ble_table.acl_buffer = ble_acl_buffer;

    sys_table.cmd_buffer = sys_cmd_buffer;
    sys_table.evt_queue = &sys_evt_queue;

    mem_manager_table.ble_spare_buffer = ble_spare_buffer;
    mem_manager_table.sys_spare_buffer = sys_spare_buffer;
    mem_manager_table.evt_pool = evt_pool;
    mem_manager_table.evt_pool_size = sizeof(evt_pool);
    mem_manager_table.evt_free_queue = &evt_free_queue;
    mem_manager_table.traces_evt_pool = NULL;
    mem_manager_table.traces_evt_pool_size = 0;

    ref_table.device_info_table = &device_info_table;
    ref_table.ble_table = &ble_table;
    ref_table.sys_table = &sys_table;
    ref_table.mem_manager_table = &mem_manager_table;
}

static void
ipcc_init(void)
{
    RCC->AHB3ENR |= RCC_AHB3ENR_IPCCEN | RCC_AHB3ENR_HSEMEN;
    RCC->C2AHB3ENR |= RCC_C2AHB3ENR_IPCCEN;
    (void)RCC->C2AHB3ENR;

    /* Clear all flags and mask all channel interrupts */
    IPCC->C1SCR = CH_ALL << IPCC_C1SCR_CH1C_Pos;
    IPCC->C2SCR = CH_ALL << IPCC_C2SCR_CH1C_Pos;
    IPCC->C1MR = (CH_ALL << IPCC_C1MR_CH1OM_Pos) | (CH_ALL << IPCC_C1MR_CH1FM_Pos);
    IPCC->C1CR = IPCC_C1CR_RXOIE | IPCC_C1CR_TXFIE;

    NVIC_SetVector(IPCC_C1_RX_IRQn, (uint32_t)ipcc_rx_isr);
    NVIC_SetVector(IPCC_C1_TX_IRQn, (uint32_t)ipcc_tx_isr);
    NVIC_EnableIRQ(IPCC_C1_RX_IRQn);
    NVIC_EnableIRQ(IPCC_C1_TX_IRQn);

    ipcc_rx_irq_enable(CH_SYS);
}

static void
cpu2_boot(void)
{
    /* CPU2 enters shutdown when it has nothing to do until told otherwise */
    LL_C2_PWR_SetPowerMode(LL_PWR_MODE_SHUTDOWN);

    /* CPU1 SEV is routed to CPU2 through EXTI line 41 */
    EXTI->C2EMR2 |= EXTI_C2EMR2_EM41;
    EXTI->RTSR2 |= EXTI_RTSR2_RT41;
    __SEV();
    __WFE();

    PWR->CR4 |= PWR_CR4_C2BOOT;
}

void
ipc_stm32wb_init(void)
{
    int rc;

    SYSINIT_ASSERT_ACTIVE();

    rc = os_sem_init(&ready_sem, 0);
    SYSINIT_PANIC_ASSERT(rc == 0);
    rc = os_sem_init(&sys_rsp_sem, 0);
    SYSINIT_PANIC_ASSERT(rc == 0);

    mailbox_init();
    ipcc_init();
    cpu2_boot();
}
