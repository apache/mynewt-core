# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#  http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

#
# Source this script to gdb that has loaded DA1469x core file
#
# It will produce cmac_core.elf file that will have RAM data from CMAC Cortex-M0
# then use this cmac_core.elf along with cmac elf file produced during build to analyze core dump
#

define cmac_core_dump

    set $_rom_size = ((struct cmac_image_info *)(_binary_cmac_rom_bin_start+0x80))->offset_data
    set $_cmac_ram_start = ((struct cmac_image_info *)(_binary_cmac_rom_bin_start+0x80))->offset_data + _binary_cmac_ram_bin_start
    set $_cmac_ram_end = $_cmac_ram_start + ((struct cmac_image_info *)(_binary_cmac_rom_bin_start+0x80))->size_ram - ((struct cmac_image_info *)(_binary_cmac_rom_bin_start+0x80))->offset_data

    set $_regs_size = 0xA8
    set $_ram_size =  $_cmac_ram_end - $_cmac_ram_start

    set $_mem = $_cmac_ram_start + 0x100

    set $_lr = g_cmac_shared_data->coredump.lr
    set $_pc = g_cmac_shared_data->coredump.pc
    set $_tf = 0

    # Find exception frame by LR nad PC pair
    # NOTE: this could be avoided if Trap Frame pointer was also stored in shared data.
    find /w /1 $_cmac_ram_start, +$_ram_size, $_lr, $_pc
    # exception pointer from address of LR
    set $_ep = (uint32_t *)($_ - 20)
    # SP in cmac address space
    set $_cmac_sp = (uint32_t *)((int)$_ep - (int)$_cmac_ram_start + 0x20000000)

    # Exception in system mode, Trap Frame just before exception frame
    if $_ep[-1] == 0xFFFFFFF1 || $_ep[-1] == 0xFFFFFFF9
        set $_tf = $_ep - 10
    else
        # Exception in thread mode, try to find trap frame (may not work).
        find /w /1 $_ep + 8, $_cmac_ram_end, $_cmac_sp
        if $numfound > 0
            set $_tf = (uint32_t *)$_
        end
    end

    # Elf header
    dump binary value cmac_core.elf "\177ELF\001\001\001\000\000\000\000\000\000\000\000"
    # e_type, e_machine
    append binary value cmac_core.elf (short[2]) {0x04, 0x28}
    # e_version, e_entry, e_phoff, e_shoff, e_flags
    append binary value cmac_core.elf (int[5])   {0x01, 0x00, 0x34, 0, 0}
    # e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx
    append binary value cmac_core.elf (short[6]) {0x34, 0x20, 0x03, 0x28, 0, 0}

    # Program header
    # Note for regs
    append binary value cmac_core.elf (int[8])   {0x04, 0x34 + 0x20 + 0x20 + 0x20, 0, 0, $_regs_size, 0, 0, 4}
    # Program header
    # ROM load section
    append binary value cmac_core.elf (int[8])   {0x01, 0x34 + 0x20 + 0x20 + 0x20 + $_regs_size, 0x00000000, 0, $_rom_size, $_rom_size, 0, 4}
    # Program header
    # RAM load section
    append binary value cmac_core.elf (int[8])   {0x01, 0x34 + 0x20 + 0x20 + 0x20 + $_regs_size + $_rom_size, 0x20000000, 0, $_ram_size, $_ram_size, 0, 4}

    # Note data: registers
    append binary value cmac_core.elf (int[3]) {5, 0x94, 1}
    append binary value cmac_core.elf ".reg\0\0\0"
    append binary value cmac_core.elf (int[18]){ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }

    # regs
    if $_tf == 0
        append binary value cmac_core.elf (int[18]){ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, g_cmac_shared_data->coredump.lr, g_cmac_shared_data->coredump.pc, 0, 0 }
    else
        append binary value cmac_core.elf (int[18]){ $_ep[0], $_ep[1], $_ep[2], $_ep[3], $_tf[1], $_tf[2], $_tf[3], $_tf[4], $_tf[5], $_tf[6], $_tf[7], $_tf[8], $_ep[4], $_cmac_sp + 8, g_cmac_shared_data->coredump.lr, g_cmac_shared_data->coredump.pc, $_ep[7], 0 }
    end
    append binary value cmac_core.elf 0

    # ROM content
    append binary memory cmac_core.elf &_binary_cmac_ram_bin_start ((int)&_binary_cmac_ram_bin_start + $_rom_size)

    # RAM content
    append binary memory cmac_core.elf $_cmac_ram_start ($_cmac_ram_end)

end
