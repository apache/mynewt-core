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

define mn_stat_print
    set $shdr = $arg0
    printf "<%s>\n", $shdr->s_name

    set $idx = 0
    while $idx < $shdr->s_cnt
        set $name_entry = &$shdr->s_map[$idx]
        set $val_addr = (uint8_t *)$shdr + $name_entry->snm_off

        # No "else if" chains in gdb; simplify to avoid nesting.
        if $shdr->s_size == 1
            set $val = *(uint8_t *)$val_addr
        end
        if $shdr->s_size == 2
            set $val = *(uint16_t *)$val_addr
        end
        if $shdr->s_size == 4
            set $val = *(uint32_t *)$val_addr
        end

        printf "%s: %llu\n", $name_entry->snm_name, (unsigned long long)$val

        set $idx++
    end
end

document mn_stat_print
usage: mn_stat_print <struct stats_hdr *>

Prints the stats group with the specified header.

Note: Requires MYNEWT_VAL(STATS_NAMES) to be enabled.
end

define mn_stat_print_all
    set $cur = g_stats_registry.stqh_first
    while $cur != 0
        mn_stat_print $cur
        set $cur = $cur->s_next.stqe_next

        if $cur != 0
            printf "\n"
        end
    end
end


document mn_stat_print_all
usage: mn_stat_print_all

Prints all registered stats groups.
end
