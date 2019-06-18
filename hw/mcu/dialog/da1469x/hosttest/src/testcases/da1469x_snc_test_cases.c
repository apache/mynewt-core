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
#include "syscfg/syscfg.h"
#include "da1469x_test/da1469x_test.h"

/* XXX: not tested
 - WADAD register addresses for op1 and op2
 - WADVA using registers.
 - RDCGR reigster for addr1 and addr2
*/

#if MYNEWT_VAL(TESTBENCH_DA1469X_SNC == 1)
#include <modlog/modlog.h>
#include "mcu/da1469x_snc.h"

#define SNC_TEST_XOR_MASK   (0x003C00F0)

/*
 * This is an ugly hack, but to use the 64-bit macros it requires hard-coded
 * addresses. These are defined at the bottom of the stack so highly unlikely
 * they will get used as the stack is extremely large.
 */
uint32_t da1469x_test_var0;
uint32_t da1469x_test_var1;
uint32_t da1469x_test_var2;
uint32_t da1469x_test_var3;
uint32_t da1469x_test_var4;
uint32_t da1469x_test_var5;
uint32_t da1469x_test_var6;
uint32_t da1469x_test_var7;
uint32_t da1469x_test_var8;
uint32_t da1469x_test_var9;
uint32_t da1469x_test_var10;
uint32_t da1469x_test_var11;
uint32_t da1469x_test_var12;
uint32_t da1469x_test_var13;
uint32_t da1469x_test_var14;
uint32_t da1469x_test_var15;
uint32_t da1469x_test_var16;

/*
 * The test program. Note that the X: in the comment refers to the "line"
 * number of the program (the offset into the program array). Some instructions
 * are 64-bits which is why one initializer occupies two "lines".
 */
uint32_t snc_program[] =
{
    /* 0: No operation */
    SNC_CMD_NOOP(),

    /* 1: Delay 10 ticks */
    SNC_CMD_DEL(10),

    /* 2: Increment test var 1 by 1 */
    SNC_CMD_INC_BY_1(&da1469x_test_var1),

    /* 3: Increment test var 2 by 4 */
    SNC_CMD_INC_BY_4(&da1469x_test_var2),

    /* 4:
     * This compares bit position 2 of test var 2 with 1. Sets the EQUALHIGH
     * flag if set (which it is in this case).
     */
    SNC_CMD_RDCBI_RAM(&da1469x_test_var2, 2),

    /* 5:
     * Branch if EQUALHIGH flag is true. This branch should move past the
     * next two instructions.
     */
    SNC_CMD_COBR_EQ_DIR(&snc_program[8]),

    /* 6, 7: These two instructions should get skipped*/
    SNC_CMD_INC_BY_4(&da1469x_test_var3),
    SNC_CMD_INC_BY_4(&da1469x_test_var3),

    /* 8: Just cause I feel like it */
    SNC_CMD_NOOP(),

    /* 9, 10: These two instructions should cause a loop here 10 times */
    SNC_CMD_INC_BY_1(&da1469x_test_var4),
    SNC_CMD_COBR_LOOP(&snc_program[9], 10),

    /* 11, 12:
     * These two instructions should cause a loop here 20 times.
     * Purpose here is to see if the loop counter is a decrementing counter
     * and after it gets exhausted it restarts.
     */
    SNC_CMD_INC_BY_1(&da1469x_test_var5),
    SNC_CMD_COBR_LOOP(&snc_program[11], 20),

    /* 13, 14: Move the contents of test var 7 to test var 8 */
    SNC_CMD_WADAD_RAM2RAM(&da1469x_test_var8, SNC_WADAD_AM1_DIRECT,
                          SNC_WADAD_AM2_DIRECT, &da1469x_test_var7),

    /* 15, 16:
     * var 9 is pointer; move contents of what var 9 points to, to what var
     * 10 points to (var 10 is a pointer).
     */
    SNC_CMD_WADAD_RAM2RAM(&da1469x_test_var10, SNC_WADAD_AM1_INDIRECT,
                          SNC_WADAD_AM2_INDIRECT, &da1469x_test_var9),

    /* 17, 18: XOR */
    SNC_CMD_TOBRE_RAM(&da1469x_test_var12, SNC_TEST_XOR_MASK),

    /* 19, 20: XOR register (SNC_CTRL_REG). Should toggle both IRQ config bits */
    SNC_CMD_TOBRE_REG(0x50020C00, 0xC0),

    /* 21, 22: Moves immediate (the address of var 12) into var 13 */
    SNC_CMD_WADVA_DIR_RAM(&da1469x_test_var13, &da1469x_test_var12),

    /* 23, 24: Moves immediate into address pointed to by var14 */
    SNC_CMD_WADVA_IND_RAM(&da1469x_test_var14, 0x33333333),

    /* 25, 26:
     * Compare the contents of test var 9 and 7. This instruction basically
     * does this: if (var9 > var7) set GREATERVAL_FLAG. In this case, var9
     * should be greater than var 7
     */
    SNC_CMD_RDCGR_RAMRAM(&da1469x_test_var9, &da1469x_test_var7),

    /* 27:
     * Branch if EQUALHIGH flag is true. This branch should move past the
     * next instruction
     */
    SNC_CMD_COBR_GT_DIR(&snc_program[30]),

    /* 28, 29: These two instructions should get skipped*/
    SNC_CMD_INC_BY_4(&da1469x_test_var0),
    SNC_CMD_INC_BY_4(&da1469x_test_var0),

    /* 30: Increment test var 0 by 1 */
    SNC_CMD_INC_BY_1(&da1469x_test_var0),

    /* 31: Check if SW contrl bit is set in SNC control register. It should! */
    SNC_CMD_RDCBI_REG(0x50020C00, SNC_SNC_CTRL_REG_SNC_SW_CTRL_Pos),

    /* 32: Branch past next instruction if EQUALHIGH_FLAG is set (should be!) */
    SNC_CMD_COBR_EQ_DIR(&snc_program[34]),
    SNC_CMD_INC_BY_4(&da1469x_test_var16),

    /* 34: Sleep (program ends) */
    SNC_CMD_SLEEP()
};

/* Number of words (32-bits) in the static program */
#define SNC_STATIC_PROGRAM_NUM_WORDS    (sizeof(snc_static_program) / 4)

TEST_CASE(da1469x_snc_test_case_1)
{
    int rc;

    MODLOG_INFO(LOG_MODULE_TEST, "DA1469x snc test 1");

    /*
     * Initialize to some non-zero number. The test program should increment
     * var1 by 1 and var2 by 4 using the increment instruction.
     */
    da1469x_test_var1 = 10;
    da1469x_test_var2 = 10;

    /* Initialize test var 7 with a value */
    da1469x_test_var7 = 0x12345678;

    /* Make test var 9 a pointer that points to test var 7 */
    da1469x_test_var9 = (uint32_t)&da1469x_test_var7;

    /* Make test var 10 a pointer that points to test var 11 */
    da1469x_test_var10 = (uint32_t)&da1469x_test_var11;

    /* test var 12 will test xor */
    da1469x_test_var12 = 0xC3A78F;

    /* Test var 14 is a pointer to var 15 */
    da1469x_test_var14 = (uint32_t)&da1469x_test_var15;

    /* Configure the SNC (base address and divider) */
    rc = da1469x_snc_config(&snc_program, SNC_CLK_DIV_1);
    if (rc) {
        MODLOG_INFO(LOG_MODULE_TEST, "snc config failed");
        TEST_ASSERT_FATAL(0);
    }

    /* Initialize the SNC */
    rc = da1469x_snc_sw_init();
    if (rc) {
        MODLOG_INFO(LOG_MODULE_TEST, "snc init failed");
        TEST_ASSERT_FATAL(0);
    }

    /*
     * Make sure IRQ config bits are 0. The init function clears these but
     * we do it here as well.
     */
    da1469x_snc_irq_config(SNC_IRQ_MASK_NONE, NULL, NULL);
    if ((SNC->SNC_CTRL_REG & SNC_SNC_CTRL_REG_SNC_IRQ_CONFIG_Msk) != 0) {
        MODLOG_INFO(LOG_MODULE_TEST, "snc irq config failed");
        TEST_ASSERT_FATAL(0);
    }

    /* Start the program */
    da1469x_snc_sw_start();

    /* Wait 1 second for program to finish. */
    os_time_delay(OS_TICKS_PER_SEC);
    if (!da1469x_snc_program_is_done()) {
        MODLOG_INFO(LOG_MODULE_TEST, "snc test failed (not done)");
        da1469x_snc_sw_stop();
        TEST_ASSERT_FATAL(0);
    }

    /* Check test var 1 and test var 2 have correct values */
    if (da1469x_test_var1 != 11) {
        MODLOG_INFO(LOG_MODULE_TEST, "snc test failed: inc by 1");
        TEST_ASSERT_FATAL(0);
    }
    if (da1469x_test_var2 != 14) {
        MODLOG_INFO(LOG_MODULE_TEST, "snc test failed: inc by 4");
        TEST_ASSERT_FATAL(0);
    }

    /* Test var 3 should be 0 */
    if (da1469x_test_var3 != 0) {
        MODLOG_INFO(LOG_MODULE_TEST, "snc test failed: RDCBI and/or COBR_EQ");
        TEST_ASSERT_FATAL(0);
    }

    /* Test var 4 should be 10 */
    if (da1469x_test_var4 != 11) {
        MODLOG_INFO(LOG_MODULE_TEST, "snc test failed: COBR loop. tv4=%lu",
                    da1469x_test_var4);
        TEST_ASSERT_FATAL(0);
    }

    /* Test var 5 should be 20 */
    if (da1469x_test_var5 != 21) {
        MODLOG_INFO(LOG_MODULE_TEST, "snc test failed: COBR loop 2. tv5=%lu",
                    da1469x_test_var5);
        TEST_ASSERT_FATAL(0);
    }

    /* Test var 8 should be equal to test var 7 */
    if (da1469x_test_var8 != da1469x_test_var7) {
        MODLOG_INFO(LOG_MODULE_TEST, "snc test failed: WADAD direct. tv7=%lx"
                                     " tv8=%lx",
                    da1469x_test_var7, da1469x_test_var8);
        TEST_ASSERT_FATAL(0);
    }

    /* Test var 11 should have value in test var 7 */
    if (da1469x_test_var11 != da1469x_test_var7) {
        MODLOG_INFO(LOG_MODULE_TEST, "snc test failed: WADAD indirect. tv7=%lx"
                                     " tv11=%lx",
                    da1469x_test_var7, da1469x_test_var11);
        TEST_ASSERT_FATAL(0);
    }


    /* Test var 12 should be those two values xor'd (should equal 0xFFA77F) */
    if (da1469x_test_var12 != (SNC_TEST_XOR_MASK ^ 0xC3A78F)) {
        MODLOG_INFO(LOG_MODULE_TEST, "snc test failed: TOBRE. tv12=%lx",
                    da1469x_test_var12);
        TEST_ASSERT_FATAL(0);
    }

    /* SNC control registers should have both IRQ bits set */
    if ((SNC->SNC_CTRL_REG & SNC_SNC_CTRL_REG_SNC_IRQ_CONFIG_Msk) !=
        SNC_SNC_CTRL_REG_SNC_IRQ_CONFIG_Msk) {
        MODLOG_INFO(LOG_MODULE_TEST, "snc test failed: TOBRE register %lx",
                    SNC->SNC_CTRL_REG);
        TEST_ASSERT_FATAL(0);
    }

    /* Contents of test var 13 should equal address of test var 12 */
    if (da1469x_test_var13 != (uint32_t)&da1469x_test_var12) {
        MODLOG_INFO(LOG_MODULE_TEST, "snc test failed: WADVA direct. &tv12=%lx"
                                     " tv13=%lx",
                    &da1469x_test_var12,
                    da1469x_test_var13);
        TEST_ASSERT_FATAL(0);
    }

    /* Test var 15 should be equal 0x33333333 */
    if (da1469x_test_var15 != 0x33333333) {
        MODLOG_INFO(LOG_MODULE_TEST, "snc test failed: WADVA indirect tv15=%lx",
                    da1469x_test_var15);
        TEST_ASSERT_FATAL(0);
    }

    /* Test var 0 should be equal to 1 */
    if (da1469x_test_var0 != 1) {
        MODLOG_INFO(LOG_MODULE_TEST, "snc test failed: RDCGR RAMRAM tv0=%lx",
                    da1469x_test_var0);
        TEST_ASSERT_FATAL(0);
    }

    /* Test var 16 should be equal to 0 */
    if (da1469x_test_var16 != 0) {
        MODLOG_INFO(LOG_MODULE_TEST, "snc test failed: RDCBI reg tv16=%lx",
                    da1469x_test_var16);
        TEST_ASSERT_FATAL(0);
    }

    /* Check for hard fault or bus status errors */
    if (SNC->SNC_STATUS_REG & (SNC_SNC_STATUS_REG_HARD_FAULT_STATUS_Msk |
                               SNC_SNC_STATUS_REG_BUS_ERROR_STATUS_Msk)) {
        MODLOG_INFO(LOG_MODULE_TEST, "snc test failed: ERR snc status %lx",
                    SNC->SNC_STATUS_REG);
        TEST_ASSERT_FATAL(0);
    }

    da1469x_snc_sw_stop();
    rc = da1469x_snc_sw_deinit();
    if (rc) {
        MODLOG_INFO(LOG_MODULE_TEST, "snc s/w deinit failed");
        TEST_ASSERT_FATAL(0);
    }

    MODLOG_INFO(LOG_MODULE_TEST, "snc test 1 success");
}

/*====================== TEST CASE 2 =====================================
The intent of this test case is to test the interrupt API.
========================================================================*/
uint32_t g_snc_tc2_cntr;
uint32_t snc_prog_test_case2[] =
{
    /* This should toggle the IRQ_EN bit, thus generating an interrupt */
    SNC_CMD_TOBRE_REG(0x50020C00, SNC_SNC_CTRL_REG_SNC_IRQ_EN_Msk),
    SNC_CMD_SLEEP()
};

void snc_tc2_irq_cb(void *arg)
{
    uint32_t *tmp;

    tmp = (uint32_t *)arg;
    if (tmp) {
        *tmp += 1;
    }
}

TEST_CASE(da1469x_snc_test_case_2)
{
    int rc;

    MODLOG_INFO(LOG_MODULE_TEST, "DA1469x snc test 2");

    /* Configure the SNC (base address and divider) */
    rc = da1469x_snc_config(&snc_prog_test_case2, SNC_CLK_DIV_1);
    if (rc) {
        MODLOG_INFO(LOG_MODULE_TEST, "snc config failed");
        TEST_ASSERT_FATAL(0);
    }

    /* Initialize the SNC */
    rc = da1469x_snc_sw_init();
    if (rc) {
        MODLOG_INFO(LOG_MODULE_TEST, "snc init failed");
        TEST_ASSERT_FATAL(0);
    }

    /*
     * Register an interrupt routine. Pass it an argument as well. This
     * argument points to a global counter that should get incremented once
     */
    da1469x_snc_irq_config(SNC_IRQ_MASK_HOST, snc_tc2_irq_cb, &g_snc_tc2_cntr);

    /* Start the program */
    da1469x_snc_sw_start();

    /* This program should finish very quickly */
    os_time_delay(OS_TICKS_PER_SEC / 10);
    if (!da1469x_snc_program_is_done()) {
        MODLOG_INFO(LOG_MODULE_TEST, "snc test 2 failed (not done)");
        da1469x_snc_sw_stop();
        TEST_ASSERT_FATAL(0);
    }

    /* Check that the counter got incremented */
    if (g_snc_tc2_cntr != 1) {
        MODLOG_INFO(LOG_MODULE_TEST, "snc test failed tc2=%u", g_snc_tc2_cntr);
        TEST_ASSERT_FATAL(0);
    }

    da1469x_snc_sw_stop();
    rc = da1469x_snc_sw_deinit();
    if (rc) {
        MODLOG_INFO(LOG_MODULE_TEST, "snc s/w deinit failed");
        TEST_ASSERT_FATAL(0);
    }

    MODLOG_INFO(LOG_MODULE_TEST, "snc test 2 success");
}

/*
 * This test case enables only the PDC interrupt. Should not get a SNC
 * interrupt to the M33 in this case
 */
TEST_CASE(da1469x_snc_test_case_3)
{
    int rc;

    MODLOG_INFO(LOG_MODULE_TEST, "DA1469x snc test 3");

    /* Configure the SNC (base address and divider) */
    rc = da1469x_snc_config(&snc_prog_test_case2, SNC_CLK_DIV_1);
    if (rc) {
        MODLOG_INFO(LOG_MODULE_TEST, "snc config failed");
        TEST_ASSERT_FATAL(0);
    }

    /* Initialize the SNC */
    rc = da1469x_snc_sw_init();
    if (rc) {
        MODLOG_INFO(LOG_MODULE_TEST, "snc init failed");
        TEST_ASSERT_FATAL(0);
    }

    /*
     * Register an interrupt routine. Pass it an argument as well. This
     * argument points to a global counter that should get incremented once
     */
    da1469x_snc_irq_config(SNC_IRQ_MASK_PDC, snc_tc2_irq_cb, &g_snc_tc2_cntr);

    /* Start the program */
    da1469x_snc_sw_start();

    /* This program should finish very quickly */
    os_time_delay(OS_TICKS_PER_SEC / 10);
    if (!da1469x_snc_program_is_done()) {
        MODLOG_INFO(LOG_MODULE_TEST, "snc test 3 failed (not done)");
        da1469x_snc_sw_stop();
        TEST_ASSERT_FATAL(0);
    }

    /* Check that the counter got incremented */
    if (g_snc_tc2_cntr != 1) {
        MODLOG_INFO(LOG_MODULE_TEST, "snc test failed tc2=%u", g_snc_tc2_cntr);
        TEST_ASSERT_FATAL(0);
    }

    da1469x_snc_sw_stop();
    rc = da1469x_snc_sw_deinit();
    if (rc) {
        MODLOG_INFO(LOG_MODULE_TEST, "snc s/w deinit failed");
        TEST_ASSERT_FATAL(0);
    }

    MODLOG_INFO(LOG_MODULE_TEST, "snc test 3 success");
}
#endif
