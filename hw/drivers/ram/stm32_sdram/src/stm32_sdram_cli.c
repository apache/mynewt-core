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

#include <string.h>
#include <stdlib.h>
#include <os/mynewt.h>
#include <mcu/stm32_hal.h>
#include <console/console.h>
#include <shell/shell.h>
#include <stm32_sdram/stm32_sdram.h>

extern uint8_t __sdram_start__[] __attribute__((weak));
extern uint8_t __sdram_end__[] __attribute__((weak));

/* First SDRAM address not used by .sdram section */
static uint32_t
sdram_free_start(void)
{
    uint32_t start = STM32_SDRAM_BASE;

    if (__sdram_end__ != NULL && (uint32_t)__sdram_end__ > start) {
        start = ((uint32_t)__sdram_end__ + 31) & ~31U;
    }
    return start;
}

static uint32_t
sdram_end(void)
{
    return STM32_SDRAM_BASE + stm32_sdram_size();
}

#define MB (1024 * 1024)

/* Frequency in MHz, with up to 2 decimals when not integer */
static char *
mhz_str(uint32_t freq, char *buf)
{
    uint32_t mhz = freq / 1000000;
    uint32_t frac = (freq % 1000000 + 5000) / 10000;

    if (frac >= 100) {
        mhz++;
        frac = 0;
    }
    if (frac) {
        sprintf(buf, "%lu.%02lu MHz", (unsigned long)mhz, (unsigned long)frac);
    } else {
        sprintf(buf, "%lu MHz", (unsigned long)mhz);
    }
    return buf;
}

static int
sdram_cmd_info(int argc, char **argv)
{
    char buf[16];

    console_printf("base 0x%08lx size %u MB SDCLK %s ready %d\n",
                   (unsigned long)STM32_SDRAM_BASE,
                   (unsigned)(stm32_sdram_size() / MB),
                   mhz_str(stm32_sdram_clock(), buf), stm32_sdram_ready());
    console_printf(".sdram 0x%08lx-0x%08lx, free from 0x%08lx\n",
                   (unsigned long)(uintptr_t)__sdram_start__,
                   (unsigned long)(uintptr_t)__sdram_end__,
                   (unsigned long)sdram_free_start());
    return 0;
}

static int
sdram_cmd_test(int argc, char **argv)
{
    uint32_t start = sdram_free_start();
    uint32_t size = sdram_end() - start;
    uint32_t fail = 0;
    uint64_t t0;
    uint64_t t1;
    int rc;

    if (argc > 1) {
        start = strtoul(argv[1], NULL, 0);
        size = sdram_end() - start;
    }
    if (argc > 2) {
        size = strtoul(argv[2], NULL, 0);
    }
    if (start < sdram_free_start() || start >= sdram_end() ||
        size > sdram_end() - start) {
        console_printf("range outside free SDRAM\n");
        return 0;
    }
    console_printf("testing 0x%08lx..0x%08lx\n", (unsigned long)start,
                   (unsigned long)(start + size));
    t0 = os_get_uptime_usec();
    rc = stm32_sdram_test(start, size & ~3U, &fail);
    t1 = os_get_uptime_usec();
    if (rc) {
        console_printf("FAILED at 0x%08lx\n", (unsigned long)fail);
    } else {
        console_printf("OK (%lu ms)\n", (unsigned long)((t1 - t0) / 1000));
    }
    return 0;
}

/*
 * Speed is measured with DWT cycle counter, os_get_uptime_usec() has
 * only OS tick resolution.
 */
static void
sdram_cycle_counter_start(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
#if (__CORTEX_M == 7)
    /* Cortex-M7 DWT is locked after reset */
    DWT->LAR = 0xC5ACCE55;
#endif
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/* Each test processes this many bytes, smaller buffers are reused */
#define SPEED_TOTAL   (2 * MB)
/* Internal RAM buffer size (bigger than D-cache), halved if malloc fails */
#define SPEED_RAM_MAX (64 * 1024)
#define SPEED_RAM_MIN (4 * 1024)

/* MB/s (MB = 2^20 bytes) with one decimal, cycles at SystemCoreClock */
static void
speed_print(const char *name, uint32_t cycles)
{
    uint64_t x10 =
        cycles ? (uint64_t)SPEED_TOTAL * SystemCoreClock * 10 / MB / cycles : 0;

    console_printf("  %-14s %lu.%lu MB/s\n", name, (unsigned long)(x10 / 10),
                   (unsigned long)(x10 % 10));
}

static void
speed_memset(const char *name, uint8_t *dst, uint32_t size)
{
    uint32_t t0 = DWT->CYCCNT;
    uint32_t done;

    for (done = 0; done < SPEED_TOTAL; done += size) {
        memset(dst, 0x5A, size);
    }
    speed_print(name, DWT->CYCCNT - t0);
}

static uint32_t
speed_read32(const char *name, const uint8_t *src, uint32_t size)
{
    const volatile uint32_t *p = (const volatile uint32_t *)src;
    uint32_t t0 = DWT->CYCCNT;
    uint32_t sum = 0;
    uint32_t done;
    uint32_t i;

    for (done = 0; done < SPEED_TOTAL; done += size) {
        for (i = 0; i < size / 4; ++i) {
            sum += p[i];
        }
    }
    speed_print(name, DWT->CYCCNT - t0);

    return sum;
}

/*
 * Copy SPEED_TOTAL bytes in chunks, source and destination advance by chunk
 * and wrap at their sizes (all sizes are powers of 2).
 */
static void
speed_memcpy(const char *name, uint8_t *dst, uint32_t dst_size,
             const uint8_t *src, uint32_t src_size, uint32_t chunk)
{
    uint32_t t0 = DWT->CYCCNT;
    uint32_t done;

    for (done = 0; done < SPEED_TOTAL; done += chunk) {
        memcpy(dst + done % dst_size, src + done % src_size, chunk);
    }
    speed_print(name, DWT->CYCCNT - t0);
}

static int
sdram_cmd_speed(int argc, char **argv)
{
    uint8_t *sdram = (uint8_t *)sdram_free_start();
    uint32_t ram_size;
    uint8_t *ram = NULL;
    uint32_t sum;
    char cpu_buf[16];
    char sdclk_buf[16];

    if (sdram_end() - (uint32_t)sdram < SPEED_TOTAL) {
        console_printf("not enough free SDRAM\n");
        return 0;
    }
    for (ram_size = SPEED_RAM_MAX; ram_size >= SPEED_RAM_MIN; ram_size /= 2) {
        ram = malloc(ram_size);
        if (ram) {
            break;
        }
    }
    if (ram == NULL) {
        console_printf("no memory for RAM buffer\n");
        return 0;
    }

    console_printf("CPU %s, SDCLK %s, %lu MB per test\n",
                   mhz_str(SystemCoreClock, cpu_buf),
                   mhz_str(stm32_sdram_clock(), sdclk_buf),
                   (unsigned long)(SPEED_TOTAL / MB));
    sdram_cycle_counter_start();

    console_printf("SDRAM 0x%08lx, %lu MB:\n", (unsigned long)(uintptr_t)sdram,
                   (unsigned long)(SPEED_TOTAL / MB));
    speed_memset("memset", sdram, SPEED_TOTAL);
    sum = speed_read32("read32", sdram, SPEED_TOTAL);
    speed_memcpy("memcpy", sdram + SPEED_TOTAL / 2, SPEED_TOTAL / 2, sdram,
                 SPEED_TOTAL / 2, SPEED_TOTAL / 2);

    console_printf("RAM 0x%08lx, %lu KB:\n", (unsigned long)(uintptr_t)ram,
                   (unsigned long)(ram_size / 1024));
    speed_memset("memset", ram, ram_size);
    sum += speed_read32("read32", ram, ram_size);
    speed_memcpy("memcpy", ram + ram_size / 2, ram_size / 2, ram, ram_size / 2,
                 ram_size / 2);

    console_printf("Transfers (%lu KB chunks):\n", (unsigned long)(ram_size / 1024));
    speed_memcpy("SDRAM -> RAM", ram, ram_size, sdram, SPEED_TOTAL, ram_size);
    speed_memcpy("RAM -> SDRAM", sdram, SPEED_TOTAL, ram, ram_size, ram_size);

    free(ram);
    /* Keep sum so read loops are not optimized out */
    console_printf("(sum %08lx)\n", (unsigned long)sum);

    return 0;
}

#if MYNEWT_VAL(SHELL_CMD_HELP)
static const struct shell_cmd_help sdram_info_help = {
    .summary = "SDRAM address, size, clock and .sdram section",
};

static const struct shell_cmd_help sdram_test_help = {
    .summary = "Destructive test of SDRAM not used by .sdram section",
    .usage = "test [start [size]]",
};

static const struct shell_cmd_help sdram_speed_help = {
    .summary = "memset/read/memcpy speed of SDRAM, internal RAM and "
               "transfers between them",
};
#endif

static const struct shell_cmd sdram_commands[] = {
    SHELL_CMD("info", sdram_cmd_info, &sdram_info_help),
    SHELL_CMD("test", sdram_cmd_test, &sdram_test_help),
    SHELL_CMD("speed", sdram_cmd_speed, &sdram_speed_help),
};

SHELL_MODULE_WITH_TABLE(sdram, sdram_commands)
