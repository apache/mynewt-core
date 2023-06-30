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

#include <stdint.h>
#include <string.h>
#include "syscfg/syscfg.h"
#include "hal/hal_system.h"
#include "CMAC.h"
#if MYNEWT_VAL(CMAC_DEBUG_COREDUMP_ENABLE)
#include "cmac_driver/cmac_shared.h"
#endif

#ifndef max
#define max(a, b) ((a)>(b)?(a):(b))
#endif

struct exception_frame {
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;
    uint32_t pc;
    uint32_t psr;
};

struct trap_frame {
    struct exception_frame *ef;
    uint32_t r4;
    uint32_t r5;
    uint32_t r6;
    uint32_t r7;
    uint32_t r8;
    uint32_t r9;
    uint32_t r10;
    uint32_t r11;
    uint32_t lr;    /* this LR holds EXC_RETURN */
};

#if MYNEWT_VAL(MCU_DEBUG_HCI_EVENT_ON_ASSERT)
static void
os_fault_send_hci_assert_event(const char *file, int line)
{
    struct {
        uint8_t pkt_type;
        uint8_t opcode;
        uint8_t length;
        uint8_t id;
    }  __attribute__((packed)) ev_hdr;
    char line_str[7];
    int line_str_idx;
    int digit;
    int i;

    /* Let's assume there is always full path in a file */
    file = strrchr(file, '/');
    file++;

    /* Let's assume there are no more than 99999 lines in a file */
    line_str[0] = ':';
    line_str_idx = 1;
    for (i = 10000; i; i /= 10) {
        digit = line / i;
        line_str[line_str_idx] = '0' + digit;
        if (digit || (line_str_idx > 1)) {
            line_str_idx++;
        }
        line -= digit * i;
    }
    line_str[line_str_idx] = 0;

    ev_hdr.pkt_type = 0x04;
    ev_hdr.opcode = 0xff;
    ev_hdr.length = 1 + strlen(file) + strlen(line_str);
    ev_hdr.id = 0;

    cmac_mbox_write(&ev_hdr, sizeof(ev_hdr));
    cmac_mbox_write(file, strlen(file));
    cmac_mbox_write(line_str, strlen(line_str));
}
#endif

#if MYNEWT_VAL(MCU_DEBUG_HCI_EVENT_ON_FAULT)
static void
put_formatted_hex(char *s, uint32_t val)
{
    static const char *hexd = "0123456789ABCDEF";
    int i;

    for (i = 7; i >= 0; i--, val >>= 4) {
        s[i] = hexd[val & 0x0f];
    }
}

static void
os_fault_send_hci_fault_event(const struct trap_frame *tf)
{

    static const char *template1 = "pc=XXXXXXXX lr=XXXXXXXX cm_error_reg=XXXXXXXX cm_exc_stat_reg=XXXXXXXX";
    static const char *template2 = "r0-r7=XXXXXXXX,XXXXXXXX,XXXXXXXX,XXXXXXXX,XXXXXXXX,XXXXXXXX,XXXXXXXX,XXXXXXXX";
    const size_t payload_len = max(strlen(template1), strlen(template2));
    struct {
        uint8_t pkt_type;
        uint8_t opcode;
        uint8_t length;
        uint8_t id;
    }  __attribute__((packed)) ev_hdr;
    char payload[payload_len];

    ev_hdr.pkt_type = 0x04;
    ev_hdr.opcode = 0xff;
    ev_hdr.id = 0;

    ev_hdr.length = 1 + strlen(template1);
    memcpy(payload, template1, strlen(template1));
    put_formatted_hex(payload + 3, tf->ef->pc);
    put_formatted_hex(payload + 15, tf->ef->lr);
    put_formatted_hex(payload + 37, CMAC->CM_ERROR_REG);
    put_formatted_hex(payload + 62, CMAC->CM_EXC_STAT_REG);
    cmac_mbox_write(&ev_hdr, sizeof(ev_hdr));
    cmac_mbox_write(payload, strlen(template1));

    ev_hdr.length = 1 + strlen(template2);
    memcpy(payload, template2, strlen(template2));
    put_formatted_hex(payload + 6, tf->ef->r0);
    put_formatted_hex(payload + 15, tf->ef->r1);
    put_formatted_hex(payload + 24, tf->ef->r2);
    put_formatted_hex(payload + 33, tf->ef->r3);
    put_formatted_hex(payload + 42, tf->r4);
    put_formatted_hex(payload + 51, tf->r5);
    put_formatted_hex(payload + 60, tf->r6);
    put_formatted_hex(payload + 69, tf->r7);
    cmac_mbox_write(&ev_hdr, sizeof(ev_hdr));
    cmac_mbox_write(payload, strlen(template2));
}
#endif

void
__assert_func(const char *file, int line, const char *func, const char *e)
{
#if MYNEWT_VAL(CMAC_DEBUG_COREDUMP_ENABLE)
    volatile struct cmac_coredump *cd = &g_cmac_shared_data.coredump;

    cd->assert = (uint32_t)__builtin_return_address(0);
    cd->assert_file = file;
    cd->assert_line = line;
#endif

#if MYNEWT_VAL(MCU_DEBUG_HCI_EVENT_ON_ASSERT)
    os_fault_send_hci_assert_event(file, line);
#endif

    hal_system_reset();
}

void
os_default_irq(struct trap_frame *tf)
{
#if MYNEWT_VAL(CMAC_DEBUG_COREDUMP_ENABLE)
    volatile struct cmac_coredump *cd = &g_cmac_shared_data.coredump;
#endif

    if (((SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk) == (NMI_IRQn + 16)) &&
        (CMAC->CM_EXC_STAT_REG & CMAC_CM_EXC_STAT_REG_EXC_CPU_ERROR_Msk)) {
        /*
         * NMI triggered by cpu_on_error exception means this was a HardFault
         * and we have two exception frames pushed to stack - we need to use
         * the one for HF since it points to actual code location of HF, while
         * current exception frame points to HF handler so it's not really
         * useful.
         *
         * Note that this operation in fact modifies original trap frame on
         * stack but it does not really matter since we will reset anyway and
         * this function does not return.
         */
        tf->ef++;
    }

#if MYNEWT_VAL(CMAC_DEBUG_COREDUMP_ENABLE)
    cd->lr = tf->ef->lr;
    cd->pc = tf->ef->pc;

    cd->CM_STAT_REG = CMAC->CM_STAT_REG;
    cd->CM_LL_TIMER1_36_10_REG = CMAC->CM_LL_TIMER1_36_10_REG;
    cd->CM_LL_TIMER1_9_0_REG = CMAC->CM_LL_TIMER1_9_0_REG;
    cd->CM_ERROR_REG = CMAC->CM_ERROR_REG;
    cd->CM_EXC_STAT_REG = CMAC->CM_EXC_STAT_REG;
    cd->CM_LL_INT_STAT_REG = CMAC->CM_LL_INT_STAT_REG;
    cd->CM_LL_TIMER1_EQ_X_HI_REG = CMAC->CM_LL_TIMER1_EQ_X_HI_REG;
    cd->CM_LL_TIMER1_EQ_X_LO_REG = CMAC->CM_LL_TIMER1_EQ_X_LO_REG;
    cd->CM_LL_TIMER1_EQ_Y_HI_REG = CMAC->CM_LL_TIMER1_EQ_Y_HI_REG;
    cd->CM_LL_TIMER1_EQ_Y_LO_REG = CMAC->CM_LL_TIMER1_EQ_Y_LO_REG;
#endif

#if MYNEWT_VAL(MCU_DEBUG_HCI_EVENT_ON_FAULT)
    os_fault_send_hci_fault_event(tf);
#endif

    hal_system_reset();
}

