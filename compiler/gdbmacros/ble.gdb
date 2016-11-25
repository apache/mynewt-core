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

define mn_ble_opcode_parse
    set $opcode = (uint16_t)($arg0)
    set $ogf = ($opcode & 0xff00) >> 10
    set $ocf = $opcode & 0x00ff

    printf "ogf=0x%02x ocf=0x%02x\n", $ogf, $ocf
end

document mn_ble_opcode_parse
usage: mn_ble_opcode_parse <uint16_t hci-opcode>

Parses a 16-bit HCI opcode, displaying its OGF and OCF components.  The opcode
must be in little endian format.
end

define mn_gattc_procs_print
    if $argc == 0
        set $proc_list = &ble_gattc_procs
    else
        set $proc_list = ($arg0)
    end

    set $cur = $proc_list->stqh_first
    while $cur != 0
        printf "proc (%p): op=%d\n", $cur, $cur->op
        set $cur = $cur->next.stqe_next
    end
end

document mn_gattc_procs_print
usage: mn_gattc_procs_print [struct ble_gattc_proc_list *proc-list]

Prints the currently-active GATT client procedures.  The proc-list parameter is
optional; by default, the standard list is read.
end
