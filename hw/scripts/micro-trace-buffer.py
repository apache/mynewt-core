# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

try:
    import colorama
except ImportError:
    colorama = None
import re


class MicroTraceBuffer(gdb.Command):
    _arch = None
    _file = None
    _func = None
    _line = None
    _src_file = None
    _src_lines = None

    _mtb_base_addr = 0xe0043000

    _colorize = False
    _colors = {}

    def __init__(self):
        super(MicroTraceBuffer, self).__init__("mtb", gdb.COMMAND_STATUS, gdb.COMPLETE_NONE)
        try:
            mtb_state_at_crash = gdb.lookup_symbol("mtb_state_at_crash")[0]
            if mtb_state_at_crash is not None and int(gdb.parse_and_eval('mtb_state_at_crash.mtb_base_reg')) != 0:
                self._mtb_base_addr = int(gdb.parse_and_eval('mtb_state_at_crash').address)
        except gdb.error:
            pass
        if colorama:
            self._colors = {"A": colorama.Fore.BLUE,
                            "D": colorama.Fore.RESET,
                            "E": colorama.Fore.GREEN,
                            "F": colorama.Fore.YELLOW,
                            "L": colorama.Fore.RESET,
                            "S": colorama.Fore.RESET,
                            "X": colorama.Fore.RED}

    def _print(self, s: str):
        def repl(m):
            if m.group(1) in self._colors:
                return self._colors[m.group(1)]
            else:
                return colorama.Fore.RESET

        if not colorama or not self._colorize:
            s = re.sub(r"~([A-Z])~", "", s)
        else:
            s = re.sub(r"~([A-Z])~", repl, s) + colorama.Style.RESET_ALL
        print(s)

    def _get_src(self, file, line):
        if file != self._src_file:
            self._src_file = file
            try:
                f = open(file, "r")
                self._src_lines = f.readlines()
            except OSError:
                self._src_lines = None

        if not self._src_lines:
            return "<not found>"

        src = self._src_lines[line - 1].strip()
        return src

    def _describe_instr(self, addr: int, asm: str):
        blk = gdb.block_for_pc(addr)
        while blk and not blk.function:
            blk = blk.superblock
        func = str(blk.function) if blk else "<unknown>"

        pcl = gdb.find_pc_line(addr)
        file = pcl.symtab.filename if pcl.symtab else "<unknown>"
        line = pcl.line

        if (self._file != file) or (self._func != func):
            self._file = file
            self._func = func
            self._print(f"~F~{func} ~R~@ ~E~{file}")

        if self._line != line:
            self._line = line
            if pcl.symtab:
                fname = pcl.symtab.fullname()
                src = self._get_src(fname, line)
                self._print(f"~L~{line}\t~S~{src}")

        self._print(f"    ~A~0x{addr:08x}:    ~D~{asm}")

    def _disassemble(self, start: int, end: int):
        try:
            disasm = self._arch.disassemble(start, end)
            for instr in disasm:
                addr = instr["addr"]
                asm = instr["asm"].expandtabs(8)
                self._describe_instr(addr, asm)
        except gdb.MemoryError:
            print(f"Error disassembling 0x{start:08x}-0x{end:08x}")
        print()

    def _cmd_decode(self):
        print(f"NOTE: using 0x{self._mtb_base_addr:08x} as MTB base address")
        print()

        u32_ptr_t = gdb.lookup_type("uint32_t").pointer()
        mtb_reg_pos = int(gdb.Value(self._mtb_base_addr).cast(u32_ptr_t).dereference())
        mtb_reg_master = int(gdb.Value(self._mtb_base_addr + 0x04).cast(u32_ptr_t).dereference())
        mtb_reg_base = int(gdb.Value(self._mtb_base_addr + 0x0c).cast(u32_ptr_t).dereference())

        # size of trace buffer
        mtb_size = 1 << ((mtb_reg_master & 0x1f) + 4)

        # check buffer position
        mtb_wrap = bool(mtb_reg_pos & 0x04)
        mtb_reg_pos = mtb_reg_pos & 0xfffffff8

        # if pointer already wrapped, we start at current entry and process complete buffer,
        # otherwise we start at beginning and process up to current entry
        mtb_size = mtb_size if mtb_wrap else mtb_reg_pos
        mtb_reg_pos = mtb_reg_pos if mtb_wrap else 0

        # use current frame to get architecture for later disassembling
        self._arch = gdb.newest_frame().architecture()

        mtb_pkt_dst = None

        for pos in range(0, mtb_size, 8):
            exec_begin = mtb_pkt_dst

            mtb_pkt = mtb_reg_base + (mtb_reg_pos + pos) % mtb_size
            mtb_pkt_src = int(gdb.Value(mtb_pkt).cast(u32_ptr_t).dereference())
            mtb_pkt_dst = int(gdb.Value(mtb_pkt + 4).cast(u32_ptr_t).dereference())

            bit_a = mtb_pkt_src & 1
            bit_s = mtb_pkt_dst & 1

            mtb_pkt_src = mtb_pkt_src & 0xfffffffe
            mtb_pkt_dst = mtb_pkt_dst & 0xfffffffe

            # print(f"pkt> 0x{mtb_pkt_src:08x} -> 0x{mtb_pkt_dst:08x} A:{bit_a} S:{bit_s}")

            exec_end = mtb_pkt_src

            # exception entry/exit events are handled separately
            if bit_a != 0:
                if mtb_pkt_src & 0xff000000 == 0xff000000:
                    self._print(f"~X~Exception return (LR: 0x{mtb_pkt_src:08x}, "
                                f"ret address: 0x{mtb_pkt_dst:08x})")
                    print()
                    continue
                else:
                    self._print(f"~X~Exception entry (ret address: 0x{mtb_pkt_src:08x})")

            # on 1st entry in trace buffer, disassemble source of the branch
            if exec_begin is None:
                self._disassemble(mtb_pkt_src, mtb_pkt_src)
                continue

            # on 1st entry after MTB was enabled, disasssemble source of the branch
            if bit_s != 0:
                self._disassemble(mtb_pkt_src, mtb_pkt_src)
                continue

            # print(f"exe> 0x{exec_begin:08x} -> 0x{exec_end:08x}")

            self._disassemble(exec_begin, exec_end)

            if bit_a != 0:
                self._print(f"~X~Exception started")

        # disassemble target of last branch
        self._disassemble(mtb_pkt_dst, mtb_pkt_dst)

    def _cmd_colors(self, args):
        for arg in args:
            if arg == "on":
                self._colorize = True
            elif arg == "off":
                self._colorize = False
            else:
                # TODO: handle colors configuration
                pass

    def _cmd_enable(self):
        u32_ptr_t = gdb.lookup_type("uint32_t").pointer()
        addr = self._mtb_base_addr + 0x04
        mtb_reg_master = int(gdb.Value(addr).cast(u32_ptr_t).dereference())
        mtb_reg_master = mtb_reg_master | 0x80000000
        gdb.execute(f"set *0x{addr:08x} = 0x{mtb_reg_master:08x}")

    def _cmd_disable(self):
        u32_ptr_t = gdb.lookup_type("uint32_t").pointer()
        addr = self._mtb_base_addr + 0x04
        mtb_reg_master = int(gdb.Value(addr).cast(u32_ptr_t).dereference())
        mtb_reg_master = mtb_reg_master & 0x7fffffff
        gdb.execute(f"set *0x{addr:08x} = 0x{mtb_reg_master:08x}")

    def _cmd_base(self, arg):
        try:
            addr = int(arg, 0)
            self._mtb_base_addr = addr
            print(f"MTB base address set to 0x{self._mtb_base_addr:08x}")
        except ValueError:
            print(f"Invalid value for MTB base address")

    def invoke(self, arg: str, from_tty: bool):
        args = arg.split(" ")
        if len(args[0]) == 0 or args[0] == "decode":
            self._cmd_decode()
        elif args[0] == "colors":
            self._cmd_colors(args[1:])
        elif args[0] == "enable":
            self._cmd_enable()
        elif args[0] == "disable":
            self._cmd_disable()
        elif args[0] == "base":
            self._cmd_base(args[1])
        else:
            print("wut?")


MicroTraceBuffer()
