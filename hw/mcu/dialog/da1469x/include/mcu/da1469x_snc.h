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

#ifndef __MCU_DA1469X_SNC_H_
#define __MCU_DA1469X_SNC_H_

#include <stdint.h>
#include "mcu/mcu.h"
#include "DA1469xAB.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The following two defintions are used by some instructions to determine if
 * the address used in the instruction refers to a peripheral register location
 * or is an address in system RAM.
 */
#define SNC_PERIPH_ADDR     (0x50000000)
#define SNC_SYSRAM_ADDR     (MCU_MEM_SYSRAM_START_ADDRESS)
#define SNC_REG_MASK        (1 << 19)
#define SNC_OP_IS_REG(op)   ((((uint32_t)op) & SNC_PERIPH_ADDR) ? SNC_REG_MASK : 0)
#define SNC_ADDR(addr)      (((uint32_t)addr) - SNC_SYSRAM_ADDR)
#define SNC_REG(addr)       (((uint32_t)addr) - SNC_PERIPH_ADDR)

/* Proto for isr callback function (for M33) */
typedef void (*snc_isr_cb_t)(void *arg);

/*
 * For certain commands which use direct or indirect addresses. A direct address
 * specifies a memory location. An indirect address (a pointer) means that the
 * address contains the address of the desired memory location.
 *
 * Please refer to the specific command to see if these definitions should be
 * used for that command!
 */
#define SNC_ADDR_MODE_DIRECT    (0)
#define SNC_ADDR_MODE_INDIRECT  (1)

/*
 * No operation instruction.
 *
 * There are no operands for this instruction.
 *
 *      -----------------------
 *      | opcode (0) |   N/C  |
 *      -----------------------
 *      | 31  -  28  | 27 - 0 |
 *      -----------------------
 */
#define SNC_OPCODE_NOP      (0) /* No operands */
#define SNC_CMD_NOOP()      (uint32_t)(SNC_OPCODE_NOP << 28)

/*
 * Store Contents
 *
 * Stores the contents of addr2 in addr1. Addr1 and/or Addr2 can be addresses
 * or pointers and can reference either system RAM or a register
 *
 *      am1: Addressing mode for addr1
 *          SNC_WADAD_AM1_INDIRECT: Indirect addressing mode.
 *          SNC_WADAD_AM1_DIRECT:   Direct addressing mode
 *
 *      am2: Addressing mode for addr2
 *          SNC_WADAD_AM2_INDIRECT: Indirect addressing mode.
 *          SNC_WADAD_AM2_DIRECT:   Direct addressing mode
 *      ----------------------------------------------
 *      | opcode (1) | am1 | am2 |   N/C   |  addr1  |
 *      ----------------------------------------------
 *      | 31  -  28  | 27 |  26 | 25 - 20 | 19 - 0  |
 *      ----------------------------------------------
 *
 *      ----------
 *      |  addr2 |
 *      ----------
 *      | 31 - 0 |
 *      ----------
 *
 *  NOTE: The nomenclature used is addr2 first then addr1.
 *
 *  Ex. RAM2RAM: addr2 and addr1 are both in system RAM.
 *      RAM2REG: addr2 is in system RAM and addr1 is in register space
 *      REG2RAM: addr2 is a register and addr1 is in system RAM.
 */
#define SNC_OPCODE_WADAD    (1)
#define SNC_CMD_WADAD_RAM2RAM(addr1, am1, am2, addr2)   \
        (uint32_t)((SNC_OPCODE_WADAD << 28) + (am1 << 27) + (am2 << 26) +   \
                    SNC_ADDR(addr1)),                   \
        (uint32_t)(SNC_ADDR(addr2))
#define SNC_CMD_WADAD_RAM2REG(addr1, am1, am2, addr2)   \
        (uint32_t)((SNC_OPCODE_WADAD << 28) + (am1 << 27) + (am2 << 26) +   \
                    SNC_REG_MASK + SNC_REG(addr1)),                \
        (uint32_t)(SNC_ADDR(addr2))
#define SNC_CMD_WADAD_REG2RAM(addr1, am1, am2, addr2)   \
        (uint32_t)((SNC_OPCODE_WADAD << 28) + (am1 << 27) + (am2 << 26) +   \
                    SNC_ADDR(addr1)),                \
        (uint32_t)(SNC_REG_MASK + SNC_REG(addr2))
#define SNC_CMD_WADAD_REG2REG(addr1, am1, am2, addr2)   \
        (uint32_t)((SNC_OPCODE_WADAD << 28) + (am1 << 27) + (am2 << 26) +   \
                    SNC_REG_MASK + SNC_REG(addr1)),                \
        (uint32_t)(SNC_REG_MASK + SNC_REG(addr2))

#define SNC_WADAD_AM1_INDIRECT          (0)
#define SNC_WADAD_AM1_DIRECT            (1)
#define SNC_WADAD_AM2_DIRECT            (0)
#define SNC_WADAD_AM2_INDIRECT          (1)

/*
 * Store Value
 *
 * Stores immediate value (32-bits) in either addr (direct) or the address
 * pointed to by addr (indirect)
 *
 *  addr_mode:
 *      SNC_WADVA_AM_IND: Indirect address      Ex. *addr = value
 *      SNC_WADVA_AM_DIR: Direct address mode   Ex. addr = value
 *
 *      ----------------------------------------------
 *      | opcode (2) | addr_mode |   N/C   |  addr   |
 *      ----------------------------------------------
 *      | 31  -  28  |    27     | 26 - 20 | 19 - 0  |
 *      ----------------------------------------------
 *
 *      ----------
 *      |  value |
 *      ----------
 *      | 31 - 0 |
 *      ----------
 */
#define SNC_OPCODE_WADVA    (2)
#define SNC_CMD_WADVA_REG(addr, addr_mode, value)   \
        (uint32_t)((SNC_OPCODE_WADVA << 28) + (addr_mode << 27) +   \
                    SNC_REG_MASK + SNC_REG(addr)),  \
        (uint32_t)(value)

#define SNC_CMD_WADVA_RAM(addr, addr_mode, value)   \
        (uint32_t)((SNC_OPCODE_WADVA << 28) + (addr_mode << 27) +   \
                    SNC_ADDR(addr)), \
        (uint32_t)(value)

#define SNC_WADVA_AM_IND    (0)
#define SNC_WADVA_AM_DIR    (1)

#define SNC_CMD_WADVA_IND_RAM(addr, val)    \
    SNC_CMD_WADVA_RAM(addr, SNC_WADVA_AM_IND, val)
#define SNC_CMD_WADVA_IND_REG(addr, val)    \
    SNC_CMD_WADVA_REG(addr, SNC_WADVA_AM_IND, val)
#define SNC_CMD_WADVA_DIR_RAM(addr, val)    \
    SNC_CMD_WADVA_RAM(addr, SNC_WADVA_AM_DIR, val)
#define SNC_CMD_WADVA_DIR_REG(addr, val)    \
    SNC_CMD_WADVA_REG(addr, SNC_WADVA_AM_DIR, val)

/*
 * XOR
 *
 * Performs an XOR operation with the contents of addr and mask; stores contents
 * in addr. Note that addr can be a register or RAM address,
 *
 *  Ex: x = x ^ mask
 *
 *      -------------------------
 *      | opcode (3) |   addr   |
 *      -------------------------
 *      | 31  -  28  |  19 - 0  |
 *      -------------------------
 *
 *      ----------
 *      |  mask  |
 *      ----------
 *      | 31 - 0 |
 *      ----------
 */
#define SNC_OPCODE_TOBRE    (3)
#define SNC_CMD_TOBRE_RAM(addr, mask)    \
        (uint32_t)((SNC_OPCODE_TOBRE << 28) + SNC_ADDR(addr)), \
        (uint32_t)(mask)
#define SNC_CMD_TOBRE_REG(addr, mask)    \
        (uint32_t)((SNC_OPCODE_TOBRE << 28) + SNC_REG_MASK + SNC_REG(addr)), \
        (uint32_t)(mask)

/*
 * Compare Bit in Address
 *
 * Compares the contents of addr and sets the EQUALHIGH_FLAG if the bit at
 * position 'bitpos' is set to 1. NOTE: addr can be either RAM address
 * or a register. Note that bitpos is a bit position (0 to 31).
 *
 *  Ex: if (addr & (1 << bitpos)) {
 *          EQUALHIGH_FLAG = 1;
 *      } else {
 *          EQUALHIGH_FLAG = 0;
 *      }
 *
 *      --------------------------------------------
 *      | opcode (4) |  bitpos  |  N/C    |  addr  |
 *      --------------------------------------------
 *      | 31  -  28  |  27 - 23 | 22 - 20 | 19 - 0 |
 *      --------------------------------------------
 */
#define SNC_OPCODE_RDCBI    (4)
#define SNC_CMD_RDCBI_REG(addr, bitpos)    \
        (uint32_t)((SNC_OPCODE_RDCBI << 28) + ((bitpos & 0x1F) << 23) + \
                    SNC_REG_MASK + SNC_REG(addr))
#define SNC_CMD_RDCBI_RAM(addr, bitpos)    \
        (uint32_t)((SNC_OPCODE_RDCBI << 28) + ((bitpos & 0x1F) << 23) + \
                    SNC_ADDR(addr))

/*
 * Compare Address Contents
 *
 * Compares the contents of addr1 and addr2 and sets the GREATERVAL_FLAG if the
 * contents of addr1 are greater than the contents of addr2. Note that addr1
 * and addr2 can be either RAM addresses or a register.
 *
 *      ---------------------------------
 *      | opcode (5) |   N/C   | addr1  |
 *      ---------------------------------
 *      | 31  -  28  | 27 - 20 | 19 - 0 |
 *      ---------------------------------
 *
 *      --------------------
 *      |    N/C  |  addr2 |
 *      --------------------
 *      | 31 - 20 | 19 - 0 |
 *      --------------------
 *
 *  NOTE: the nomenclature here is addr1 first, then addr2
 *      EX: RAMRAM: both addr1 and addr2 are in system RAM
 *          RAMREG: addr1 is in system RAM, addr2 is a register
 *          REGRAM: addr1 is a register, addr2 is in system RAM
 *          REGREG: both addr1 and addr2 are registers
 */
#define SNC_OPCODE_RDCGR    (5)
#define SNC_CMD_RDCGR_RAMRAM(addr1, addr2)     \
        (uint32_t)((SNC_OPCODE_RDCGR << 28) + SNC_ADDR(addr1)),  \
        (uint32_t)(SNC_ADDR(addr2))
#define SNC_CMD_RDCGR_RAMREG(addr1, addr2)     \
        (uint32_t)((SNC_OPCODE_RDCGR << 28) + SNC_ADDR(addr1)),  \
        (uint32_t)(SNC_REG_MASK + SNC_REG(addr2))
#define SNC_CMD_RDCGR_REGRAM(addr1, addr2)     \
        (uint32_t)((SNC_OPCODE_RDCGR << 28) + SNC_REG_MASK + SNC_REG(addr1)), \
        (uint32_t)(SNC_ADDR(addr2))
#define SNC_CMD_RDCGR_REGREG(addr1, addr2)     \
        (uint32_t)((SNC_OPCODE_RDCGR << 28) + SNC_REG_MASK + SNC_REG(addr1)), \
        (uint32_t)(SNC_REG_MASK + SNC_REG(addr2))

/*
 * Conditional branch instruction
 *
 * Perform a conditional branch to a direct or indirect memory
 * address (RAM). There are three types of branches:
 *
 *  EQUALHIGH_FLAG: branch if flag is true. (direct or indirect)
 *      0x0A => branch to direct address
 *      0x1A => branch to indirect address
 *  GREATERVAL_FLAG: branch if flag is true. (direct or indirect)
 *      0x05 => branch to direct address
 *      0x15 => branch to indirect address
 *  LOOP: perform branch up to 128 times. (direct only)
 *      0b1yyyyyyy where yyyyyyy is loop count.
 *
 *      ----------------------------------------
 *      | opcode (6) |   flags   | N/C | ADDR  |
 *      ----------------------------------------
 *      | 31  -  28  |  27 - 20  | 19 | 18 - 0 |
 *      ----------------------------------------
 *
 *   addr: Either a direct address (specifies an address in RAM) or an indirect
 *         memory address (addr contains the RAM address to branch to).
 *   loops: Number of times to loop (max 127).
 */
#define SNC_OPCODE_COBR     (6)
#define SNC_CMD_COBR_EQ_DIR(addr)       \
    (uint32_t)((SNC_OPCODE_COBR << 28) + (0x0A << 20) + SNC_ADDR(addr))
#define SNC_CMD_COBR_EQ_IND(addr)       \
    (uint32_t)((SNC_OPCODE_COBR << 28) + (0x1A << 20) + SNC_ADDR(addr))
#define SNC_CMD_COBR_GT_DIR(addr)       \
    (uint32_t)((SNC_OPCODE_COBR << 28) + (0x05 << 20) + SNC_ADDR(addr))
#define SNC_CMD_COBR_GT_IND(addr)       \
    (uint32_t)((SNC_OPCODE_COBR << 28) + (0x15 << 20) + SNC_ADDR(addr))
#define SNC_CMD_COBR_LOOP(addr, loops)  \
    (uint32_t)((SNC_OPCODE_COBR << 28) + ((0x80 + (loops & 0x7f)) << 20) + \
               SNC_ADDR(addr))

/*
 * Increment instruction
 *
 * Increments the contents of a memory address (RAM address) by
 * either 1 or 4.
 *
 *  INC bit value of 0: increment by 1
 *  INC bit value of 1: increment by 4
 *
 *      -----------------------------------
 *      | opcode (7) |  N/C  | INC | ADDR |
 *      -----------------------------------
 *      | 31  -  28  | 27-20 | 19  | 18-0 |
 *      -----------------------------------
 */
#define SNC_OPCODE_INC      (7)
#define SNC_CMD_INC(addr, inc_by_4)     \
    (uint32_t)((SNC_OPCODE_INC << 28) + (inc_by_4 << 19) + SNC_ADDR(addr))
#define SNC_CMD_INC_BY_1(addr)  SNC_CMD_INC(addr, 0)
#define SNC_CMD_INC_BY_4(addr)  SNC_CMD_INC(addr, 1)

/*
 * Delay Instruction
 *
 * Delay for up to 255 LP clock ticks.
 *
 *      -------------------------------
 *      | opcode (8) |   N/C  | Delay |
 *      -------------------------------
 *      |  31 - 28   | 27 - 8 | 7 - 0 |
 *      -------------------------------
 */
#define SNC_OPCODE_DEL      (8)
#define SNC_CMD_DEL(ticks)  (uint32_t)((SNC_OPCODE_DEL << 28) | (ticks & 0xff))

/*
 * Sleep instruction
 *
 * This instruction is used to halt program execution. Will
 * generate a signal pulse to PDC and power down the
 * sensor node controller.
 *
 * There are no operands for this instruction.
 *
 *      -----------------------
 *      | opcode (9) |   N/C  |
 *      -----------------------
 *      | 31  -  28  | 27 - 0 |
 *      -----------------------
 */
#define SNC_OPCODE_SLP      (9) /* No operands */
#define SNC_CMD_SLEEP()     (uint32_t)(SNC_OPCODE_SLP << 28)

/*
 * API NOTES:
 *
 * 1) The SNC API are not protected by critical sections. If any of these
 * API are called by more than one task or inside an ISR they need to be
 * protected.
 *
 * 2) API with _sw_ are intended to be used when the host processor has control
 * of the SNC (as opposed to the PDC). Typically these API are for debugging as
 * the PDC usually controls the SNC.
 */

/**
 * da1469x snc sw init
 *
 * initialize the SNC for software control
 *
 * Called when the host processor wants control of the SNC (PDC
 * no longer controls SNC). The SNC must be stopped or this function will return
 * an error.
 *
 * NOTE: this function will acquire the COM power domain.
 *
 * @return int 0: success, -1: error (SNC not currently stopped).
 */
int da1469x_snc_sw_init(void);

/**
 * da1469x snc sw deinit
 *
 * Takes the SNC out of software control. The SNC must be
 * stopped and in software control or an error will be returned
 *
 * NOTE: this function releases the COM power domain when
 * called.
 *
 * @return int 0: success, -1: error
 */
int da1469x_snc_sw_deinit(void);

/**
 * da1469x snc sw start
 *
 * Starts the SNC. Note that the user should have called
 * snc_sw_load prior to starting the SNC via software control.
 *
 * @return int 0: success -1: error
 */
int da1469x_snc_sw_start(void);

/**
 * da1469x snc sw stop
 *
 * Stops the SNC from running a program. This should only be
 * called when the SNC is under software control.
 *
 * @return int 0: success -1: error
 */
int da1469x_snc_sw_stop(void);

/**
 * da1469x snc program is done
 *
 * Checks if the SNC program has finished
 *
 * @return int 0: not finished 1: finished
 */
int da1469x_snc_program_is_done(void);

/**
 * da1469x snc irq config
 *
 * Configures the SNC to interrupt the host processor and/or PDC
 * when SNC generates an interrupt.
 *
 * @param mask One or more of the following:
 *  SNC_IRQ_MASK_NONE: Do not interrupt host or PDC
 *  SNC_IRQ_MASK_HOST: Interrupt host processor (CM33)
 *  SNC_IRQ_MASK_PDC: Interrupt PDC
 * @param isr_cb    Callback function for M33 irq handler. Can
 *                  be NULL.
 * @param isr_arg   Argument to isr callback function.
 *
 * @return int 0: success -1: invalid mask parameter
 *
 * NOTES:
 *
 * 1) The IRQ configuration cannot be changed if there is a
 * pending IRQ. The IRQ must be cleared in that case. This
 * function will automatically clear the IRQ if that is the case
 * and will not report an error.
 */
int da1469x_snc_irq_config(uint8_t mask, snc_isr_cb_t isr_cb, void *arg);

#define SNC_IRQ_MASK_NONE   (0x00)  /* Do not interrupt host or PDC */
#define SNC_IRQ_MASK_HOST   (0x01)  /* Interrupt host processor (CM33) */
#define SNC_IRQ_MASK_PDC    (0x02)  /* Interrupt PDC */

/**
 * da1469x snc irq clear
 *
 * Clears the IRQ from the SNC to the PDC and/or Host processor.
 */
static inline void
da1469x_snc_irq_clear(void)
{
    SNC->SNC_CTRL_REG |= SNC_SNC_CTRL_REG_SNC_IRQ_ACK_Msk;
}

/**
 * da1469x snc error status
 *
 * Returns error status for the SNC
 *
 * @return uint8_t Error status.
 *  Returns one or more of the following errors:
 *   SNC_BUS_ERROR: Bus Fault error (invalid memory access)
 *   SNC_HARD_FAULT_ERROR: Hard Fault error (invalid
 *   instruction)
 */
uint8_t da1469x_snc_error_status(void);

#define SNC_BUS_ERROR            (0x01)
#define SNC_HARD_FAULT_ERROR     (0x02)

static inline void
da1469x_snc_enable_bus_err_detect(void)
{
    SNC->SNC_CTRL_REG |= SNC_SNC_CTRL_REG_BUS_ERROR_DETECT_EN_Msk;
}

/**
 * da1469x snc config
 *
 * Configures the starting program address of the SNC in the
 * memory controller and sets SNC clock divider.
 *
 * @param prog_addr This is the starting address in system RAM
 *                  of program
 *
 * @param clk_div Clock divider. One of the following:
 *  SNC_CLK_DIV_1: Divide low power clock by 1
 *  SNC_CLK_DIV_2: Divide low power clock by 2
 *  SNC_CLK_DIV_4: Divide low power clock by 4
 *  SNC_CLK_DIV_8: Divide low power clock by 8
 *
 * @return int 0: success, -1 otherwise.
 */
int da1469x_snc_config(void *prog_addr, int clk_div);

#define SNC_CLK_DIV_1       (0)
#define SNC_CLK_DIV_2       (1)
#define SNC_CLK_DIV_4       (2)
#define SNC_CLK_DIV_8       (3)

#ifdef __cplusplus
}
#endif

#endif /* __MCU_DA1469X_SNC_H_ */
