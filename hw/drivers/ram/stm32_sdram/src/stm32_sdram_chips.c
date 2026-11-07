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

#include <stm32_sdram/stm32_sdram.h>

/*
 * Timing presets. Each preset is a separate object in its own section
 * so unused ones are removed by the linker (-ffunction/data-sections).
 */

/* Winbond W9825G6KH-6: 4M x 16 x 4 banks (32 MB), 13 row / 9 column bits */
const struct stm32_sdram_timing stm32_sdram_timing_w9825g6kh_6 = {
    .t_mrd_clk = 2,
    .t_wr_clk = 2,
    .t_xsr_ns = 72,
    .t_ras_ns = 42,
    .t_rc_ns = 60,
    .t_rp_ns = 15,
    .t_rcd_ns = 15,
    .t_refresh_ms = 64,
};

/* Micron MT48LC4M32B2-6A: 1M x 32 x 4 banks (16 MB), 12 row / 8 column bits */
const struct stm32_sdram_timing stm32_sdram_timing_mt48lc4m32b2_6a = {
    .t_mrd_clk = 2,
    .t_wr_clk = 2,
    .t_xsr_ns = 70,
    .t_ras_ns = 42,
    .t_rc_ns = 60,
    .t_rp_ns = 18,
    .t_rcd_ns = 18,
    .t_refresh_ms = 64,
};
