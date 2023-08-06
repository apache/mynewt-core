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
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "syscfg/syscfg.h"
#include "sysflash/sysflash.h"
#include "os/os.h"
#include "mcu/mcu.h"
#include "mcu/cmsis_nvic.h"
#include "mcu/da1469x_hal.h"
#include "mcu/da1469x_lpclk.h"
#include "mcu/da1469x_clock.h"
#include "mcu/da1469x_trimv.h"
#include "mcu/da1469x_pdc.h"
#include <ipc_cmac/shm.h>
#include <ipc_cmac/mbox.h>
#include <ipc_cmac/rand.h>
#include "trng/trng.h"
#include "console/console.h"
#include "mcu/da1469x_sleep.h"

extern char _binary_cmac_img_bin_start[];
extern char _binary_cmac_img_bin_end;
extern char _binary_cmac_ram_bin_start[];
extern char _binary_cmac_ram_bin_end;

struct cmac_img_info {
    uint32_t magic;
    uint32_t img_size;
    uint32_t ram_size;
    uint32_t data_offset;
    uint32_t shared_offset;
    uint32_t shared_addr;
};

struct cmac_img_hdr {
    uint32_t isr[32];
    struct cmac_img_info ii;
    struct cmac_shm_config *shm_config;
    struct cmac_shm_ctrl *shm_ctrl;
    struct cmac_shm_mbox *shm_mbox_s2c;
    struct cmac_shm_mbox *shm_mbox_c2s;
    struct cmac_shm_trim *shm_trim;
    struct cmac_shm_rand *shm_rand;
    struct cmac_shm_dcdc *shm_dcdc;
    struct cmac_shm_crashinfo *shm_crashinfo;
    struct cmac_shm_debugdata *shm_debugdata;
};

/* Note: this can be only used after image was copied to RAM area */
#define CMAC_RAM_HDR        ((struct cmac_img_hdr *)_binary_cmac_ram_bin_start)

#define CMAC_CODE_PTR(_ptr)     ((void *)((uint32_t)(_ptr) + \
                                          MCU_MEM_SYSRAM_START_ADDRESS + \
                                          MEMCTRL->CMI_CODE_BASE_REG))
#define CMAC_SHARED_PTR(_ptr)   ((void *)((uint32_t)(_ptr) - \
                                          CMAC_RAM_HDR->ii.shared_addr + \
                                          MCU_MEM_SYSRAM_START_ADDRESS + \
                                          MEMCTRL->CMI_SHARED_BASE_REG))

volatile struct cmac_shm_config *g_cmac_shm_config;
volatile struct cmac_shm_ctrl *g_cmac_shm_ctrl;
volatile struct cmac_shm_mbox *g_cmac_shm_mbox_s2c;
volatile struct cmac_shm_mbox *g_cmac_shm_mbox_c2s;
volatile struct cmac_shm_trim *g_cmac_shm_trim;
volatile struct cmac_shm_rand *g_cmac_shm_rand;
volatile struct cmac_shm_dcdc *g_cmac_shm_dcdc;
volatile struct cmac_shm_crashinfo *g_cmac_shm_crashinfo;
volatile struct cmac_shm_debugdata *g_cmac_shm_debugdata;

/* PDC entry for waking up CMAC */
static int8_t g_cmac_host_pdc_sys2cmac;
/* PDC entry for waking up M33 */
static int8_t g_cmac_host_pdc_cmac2sys;

static void cmac_host_rand_fill(struct os_event *ev);
static struct os_event g_cmac_host_rand_ev = {
    .ev_cb = cmac_host_rand_fill
};

static void cmac_host_rand_chk_fill(void);

#if MYNEWT_VAL_CHOICE(BLE_TRANSPORT_HS, uart)
static void cmac_host_error_w4flush(struct os_event *ev);
static struct os_event g_cmac_host_error_ev = {
    .ev_cb = cmac_host_error_w4flush
};
#endif

static void
cmac2sys_isr(void)
{
    volatile struct cmac_shm_crashinfo *ci = g_cmac_shm_crashinfo;

    os_trace_isr_enter();

    /* Clear CMAC2SYS interrupt */
    *(volatile uint32_t *)0x40002000 = 2;

    cmac_mbox_read();

    if (*(volatile uint32_t *)0x40002000 & 0x1c00) {
        if (ci) {
            console_blocking_mode();
            console_printf("CMAC error (0x%08lx)\n",
                           *(volatile uint32_t *)0x40002000);
            console_printf("  lr:0x%08lx  pc:0x%08lx\n", ci->lr, ci->pc);
            if (ci->assert) {
                console_printf("  assert:0x%08lx\n", ci->assert);
                if (ci->assert_file) {
                    /* Need to translate pointer from M0 code segment to M33 data */
                    ci->assert_file += MCU_MEM_SYSRAM_START_ADDRESS +
                                       MEMCTRL->CMI_CODE_BASE_REG;
                    console_printf("         %s:%d\n",
                                   ci->assert_file, (unsigned)ci->assert_line);
                }
            }
            console_printf("  0x%08lx CM_ERROR_REG\n", ci->CM_ERROR_REG);
            console_printf("  0x%08lx CM_EXC_STAT_REG\n", ci->CM_EXC_STAT_REG);
            console_printf("  0x%08lx CM_LL_TIMER1_36_10_REG\n",
                           ci->CM_LL_TIMER1_36_10_REG);
            console_printf("  0x%08lx CM_LL_TIMER1_9_0_REG\n",
                           ci->CM_LL_TIMER1_9_0_REG);

            /* Spin if debugger is connected to CMAC to avoid resetting it */
            if (ci->CM_STAT_REG & 0x20) {
                while (1) {
                }
            }
        }

#if MYNEWT_VAL_CHOICE(BLE_TRANSPORT_HS, uart)
        NVIC_DisableIRQ(CMAC2SYS_IRQn);
        /* Wait until UART is flushed and then assert */
        cmac_host_error_w4flush(NULL);
        return;
#endif

        /* XXX CMAC is in error state, need to recover */
        assert(0);
        return;
    }

    cmac_host_rand_chk_fill();

    os_trace_isr_exit();
}

static void
cmac_host_rand_fill(struct os_event *ev)
{
    size_t num_bytes;
    struct trng_dev *trng;
    uint32_t *rnum;
    uint32_t rnums[g_cmac_shm_config->rand_size];

    /* Check if full */
    if (!cmac_rand_is_active() || cmac_rand_is_full()) {
        return;
    }

    assert(ev->ev_arg != NULL);

    /* Fill buffer with random numbers even though we may not use all of them */
    trng = ev->ev_arg;
    rnum = &rnums[0];
    num_bytes = trng_read(trng, rnum, g_cmac_shm_config->rand_size * sizeof(uint32_t));

    cmac_rand_fill(rnum, num_bytes / 4);
    cmac_host_signal2cmac();
}

static void
cmac_host_rand_chk_fill(void)
{
    if (cmac_rand_is_active() && !cmac_rand_is_full()) {
        os_eventq_put(os_eventq_dflt_get(), &g_cmac_host_rand_ev);
    }
}

static bool
shm_synced(void)
{
    return g_cmac_shm_ctrl && (g_cmac_shm_ctrl->magic == CMAC_SHM_CB_MAGIC);
}

static void
shm_init(void)
{
    struct cmac_img_hdr *ih = CMAC_RAM_HDR;

    g_cmac_shm_config = CMAC_CODE_PTR(ih->shm_config);
    g_cmac_shm_ctrl = CMAC_SHARED_PTR(ih->shm_ctrl);
    g_cmac_shm_mbox_s2c = CMAC_SHARED_PTR(ih->shm_mbox_s2c);
    g_cmac_shm_mbox_c2s = CMAC_SHARED_PTR(ih->shm_mbox_c2s);
    g_cmac_shm_trim = CMAC_SHARED_PTR(ih->shm_trim);
    g_cmac_shm_rand = CMAC_SHARED_PTR(ih->shm_rand);
    g_cmac_shm_dcdc = CMAC_SHARED_PTR(ih->shm_dcdc);
    g_cmac_shm_crashinfo = CMAC_SHARED_PTR(ih->shm_crashinfo);
    g_cmac_shm_debugdata = CMAC_SHARED_PTR(ih->shm_debugdata);
}

static void
shm_configure(void)
{
    struct cmac_shm_trim *trim;
    uint32_t *trim_data;

    g_cmac_shm_ctrl->lp_clock_freq = 0;
    g_cmac_shm_ctrl->wakeup_lpclk_ticks = 0;

    trim = (struct cmac_shm_trim *)g_cmac_shm_trim;
    trim_data = trim->data;

    trim->rfcu_len =
        da1469x_trimv_group_read(6, trim_data,
                                 g_cmac_shm_config->trim_rfcu_size);
    trim_data += g_cmac_shm_config->trim_rfcu_size;
    trim->rfcu_mode1_len =
        da1469x_trimv_group_read(8, trim_data,
                                 g_cmac_shm_config->trim_rfcu_mode1_size);
    trim_data += g_cmac_shm_config->trim_rfcu_mode1_size;
    trim->rfcu_mode2_len =
        da1469x_trimv_group_read(10, trim_data,
                                 g_cmac_shm_config->trim_rfcu_mode2_size);
    trim_data += g_cmac_shm_config->trim_rfcu_mode2_size;
    trim->synth_len =
        da1469x_trimv_group_read(7, trim_data,
                                 g_cmac_shm_config->trim_synth_size);

#if MYNEWT_VAL(CMAC_DEBUG_HOST_PRINT_ENABLE)
    cmac_host_print_trim("rfcu", trim->rfcu, trim->rfcu_len);
    cmac_host_print_trim("rfcu_mode1", trim->rfcu_mode1, trim->rfcu_mode1_len);
    cmac_host_print_trim("rfcu_mode2", trim->rfcu_mode2, trim->rfcu_mode2_len);
    cmac_host_print_trim("synth", trim->synth, trim->synth_len);
#endif

    g_cmac_shm_dcdc->enabled = DCDC->DCDC_CTRL1_REG & DCDC_DCDC_CTRL1_REG_DCDC_ENABLE_Msk;
    if (g_cmac_shm_dcdc->enabled) {
        g_cmac_shm_dcdc->v18 = DCDC->DCDC_V18_REG;
        g_cmac_shm_dcdc->v18p = DCDC->DCDC_V18P_REG;
        g_cmac_shm_dcdc->vdd = DCDC->DCDC_VDD_REG;
        g_cmac_shm_dcdc->v14 = DCDC->DCDC_V14_REG;
        g_cmac_shm_dcdc->ctrl1 = DCDC->DCDC_CTRL1_REG;
    }
}

#if MYNEWT_VAL_CHOICE(BLE_TRANSPORT_HS, uart)
#if MYNEWT_VAL(BLE_TRANSPORT_UART_PORT) < 0 || MYNEWT_VAL(BLE_TRANSPORT_UART_PORT) > 2
#error Invalid BLE_HCI_UART_PORT
#endif
static void
cmac_host_error_w4flush(struct os_event *ev)
{
    static UART_Type * const regs[] = {
        (void *)UART,
        (void *)UART2,
        (void *)UART3
    };

    if (!ev) {
        /* Move to task context, we do not want to spin in interrupt */
        os_eventq_put(os_eventq_dflt_get(), &g_cmac_host_error_ev);
        return;
    }

    do {
        cmac_mbox_read();
        while ((regs[MYNEWT_VAL(BLE_TRANSPORT_UART_PORT)]->UART_LSR_REG &
                UART_UART_LSR_REG_UART_TEMT_Msk) == 0) {
            /* Wait until both FIFO and shift registers are empty */
        }
    } while (cmac_mbox_has_data());

    /* Reset CMAC */
    CRG_TOP->CLK_RADIO_REG |= CRG_TOP_CLK_RADIO_REG_CMAC_SYNCH_RESET_Msk;

    assert(0);
}
#endif

#if MYNEWT_VAL(CMAC_DEBUG_HOST_PRINT_ENABLE)
static void
cmac_host_print_trim(const char *name, const uint32_t *tv, unsigned len)
{
    console_printf("[CMAC] Trim values for '%s'\n", name);

    while (len) {
        console_printf("       0x%08x = 0x%08x\n", (unsigned)tv[0], (unsigned)tv[1]);
        len -= 2;
        tv += 2;
    }
}
#endif

static void
cmac_load_image(void)
{
    struct cmac_img_hdr *ih = (struct cmac_img_hdr *)_binary_cmac_img_bin_start;
    struct cmac_img_info ii;
    uint32_t img_size;
    uint32_t ram_size;
#if !MYNEWT_VAL(CMAC_IMAGE_SINGLE)
    const struct flash_area *fa;
    int rc;
#endif

    /* Calculate size of image and RAM area */
    img_size = &_binary_cmac_img_bin_end - &_binary_cmac_img_bin_start[0];
    ram_size = &_binary_cmac_ram_bin_end - &_binary_cmac_ram_bin_start[0];

    /* Load image header and check if image can be loaded */
#if MYNEWT_VAL(CMAC_IMAGE_SINGLE)
    memcpy(&ii, &ih->ii, sizeof(ii));
#else
    rc = flash_area_open(FLASH_AREA_IMAGE_1, &fa);
    assert(rc == 0);
    rc = flash_area_read(fa, 128, &ii, sizeof(ii));
    assert(rc == 0);
#endif

    assert(ii.magic == 0xC3AC0001);
    assert(ii.img_size == img_size);
    assert(ii.ram_size <= ram_size);

    /* Setup CMAC memory addresses */
    MEMCTRL->CMI_CODE_BASE_REG = (uint32_t)&_binary_cmac_ram_bin_start;
    MEMCTRL->CMI_DATA_BASE_REG = MEMCTRL->CMI_CODE_BASE_REG + ii.data_offset;
    MEMCTRL->CMI_SHARED_BASE_REG = MEMCTRL->CMI_CODE_BASE_REG + ii.shared_offset;
    MEMCTRL->CMI_END_REG = MEMCTRL->CMI_CODE_BASE_REG + ii.ram_size - 1;

    /* Clear RAM area then copy image */
    memset(&_binary_cmac_ram_bin_start, 0, ram_size);
#if MYNEWT_VAL(CMAC_IMAGE_SINGLE)
    memcpy(&_binary_cmac_ram_bin_start, &_binary_cmac_img_bin_start, img_size);
#else
    rc = flash_area_read(fa, 0, &_binary_cmac_ram_bin_start, img_size);
    assert(rc == 0);
#endif
}

static void
cmac_configure(void)
{
    /* Add PDC entry to wake up CMAC from M33 */
    g_cmac_host_pdc_sys2cmac = da1469x_pdc_add(MCU_PDC_TRIGGER_MAC_TIMER,
                                               MCU_PDC_MASTER_CMAC,
                                               MCU_PDC_EN_XTAL);
    da1469x_pdc_set(g_cmac_host_pdc_sys2cmac);
    da1469x_pdc_ack(g_cmac_host_pdc_sys2cmac);

    /* Add PDC entry to wake up M33 from CMAC, if does not exist yet */
    g_cmac_host_pdc_cmac2sys = da1469x_pdc_find(MCU_PDC_TRIGGER_COMBO,
                                                MCU_PDC_MASTER_M33, 0);
    if (g_cmac_host_pdc_cmac2sys < 0) {
        g_cmac_host_pdc_cmac2sys = da1469x_pdc_add(MCU_PDC_TRIGGER_COMBO,
                                                   MCU_PDC_MASTER_M33,
                                                   MCU_PDC_EN_XTAL);
        da1469x_pdc_set(g_cmac_host_pdc_cmac2sys);
        da1469x_pdc_ack(g_cmac_host_pdc_cmac2sys);
    }

    /* Setup CMAC2SYS interrupt */
    NVIC_SetVector(CMAC2SYS_IRQn, (uint32_t)cmac2sys_isr);
    NVIC_SetPriority(CMAC2SYS_IRQn, MYNEWT_VAL(CMAC_CMAC2SYS_IRQ_PRIORITY));
    NVIC_DisableIRQ(CMAC2SYS_IRQn);
}

static void
cmac_start(void)
{
    /* Enable Radio LDO */
    CRG_TOP->POWER_CTRL_REG |= CRG_TOP_POWER_CTRL_REG_LDO_RADIO_ENABLE_Msk;

    /* Enable CMAC, but keep it in reset */
    CRG_TOP->CLK_RADIO_REG = (1 << CRG_TOP_CLK_RADIO_REG_RFCU_ENABLE_Pos) |
                             (1 << CRG_TOP_CLK_RADIO_REG_CMAC_SYNCH_RESET_Pos) |
                             (0 << CRG_TOP_CLK_RADIO_REG_CMAC_CLK_SEL_Pos) |
                             (1 << CRG_TOP_CLK_RADIO_REG_CMAC_CLK_ENABLE_Pos) |
                             (0 << CRG_TOP_CLK_RADIO_REG_CMAC_DIV_Pos);

#if MYNEWT_VAL(CMAC_DEBUG_SWD_ENABLE)
    /* Enable CMAC debugger */
    CRG_TOP->SYS_CTRL_REG |= 0x40; /* CRG_TOP_SYS_CTRL_REG_CMAC_DEBUGGER_ENABLE_Msk */
#endif

    /* Release CMAC from reset and sync */
    CRG_TOP->CLK_RADIO_REG &= ~CRG_TOP_CLK_RADIO_REG_CMAC_SYNCH_RESET_Msk;

    while (!shm_synced()) {
        /* Wait for CMAC to initialize */
    }
    NVIC_EnableIRQ(CMAC2SYS_IRQn);
}

void
cmac_host_init(void)
{
    struct trng_dev *trng;

    /* Get trng os device */
    trng = (struct trng_dev *) os_dev_open("trng", OS_TIMEOUT_NEVER, NULL);
    assert(trng);
    g_cmac_host_rand_ev.ev_arg = trng;

#if MYNEWT_VAL(CMAC_DEBUG_DIAG_ENABLE)
    cmac_diag_setup_host();
#endif

    cmac_configure();
    cmac_load_image();

    shm_init();
    shm_configure();

    cmac_start();

    cmac_host_req_sleep_update();

#if MYNEWT_VAL(CMAC_DEBUG_HOST_PRINT_ENABLE) && MYNEWT_VAL(CMAC_DEBUG_DATA_ENABLE)
    /* Trim values are calculated on RF init, so are valid after synced with CMAC */
    console_printf("[CMAC] Calculated trim_val1: 1=0x%08x 2=0x%08x\n",
                   (unsigned)g_cmac_shared_data->debug.trim_val1_tx_1,
                   (unsigned)g_cmac_shared_data->debug.trim_val1_tx_2);
    console_printf("[CMAC] Calculated trim_val2: tx=0x%08x rx=0x%08x\n",
                   (unsigned)g_cmac_shared_data->debug.trim_val2_tx,
                   (unsigned)g_cmac_shared_data->debug.trim_val2_rx);
#endif
}

void
cmac_host_signal2cmac(void)
{
    da1469x_pdc_set(g_cmac_host_pdc_sys2cmac);
}

void
cmac_host_req_sleep_update(void)
{
    uint16_t lpclk_freq;
    uint32_t wakeup_lpclk_ticks;

    if (!shm_synced()) {
        return;
    }

    lpclk_freq = da1469x_lpclk_freq_get();
    wakeup_lpclk_ticks = da1469x_sleep_wakeup_ticks_get();

    if ((g_cmac_shm_ctrl->lp_clock_freq == lpclk_freq) &&
        (g_cmac_shm_ctrl->wakeup_lpclk_ticks == wakeup_lpclk_ticks)) {
        return;
    }

    cmac_shm_lock();
    g_cmac_shm_ctrl->lp_clock_freq = lpclk_freq;
    g_cmac_shm_ctrl->wakeup_lpclk_ticks = wakeup_lpclk_ticks;
    g_cmac_shm_ctrl->pending_ops |= CMAC_SHM_CB_PENDING_OP_SLEEP_UPDATE;
    cmac_shm_unlock();

    cmac_host_signal2cmac();
}

void
cmac_host_rf_calibrate(void)
{
    if (!shm_synced()) {
        return;
    }

    cmac_shm_lock();
    g_cmac_shm_ctrl->pending_ops |= CMAC_SHM_CB_PENDING_OP_RF_CAL;
    cmac_shm_unlock();

    cmac_host_signal2cmac();
}
