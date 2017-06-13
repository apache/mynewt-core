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

set $MN_MAX_ENTRIES_DFLT = 100

define mn_gen_queue_print
    set $cur = ($arg0)
    set $next_off = ($arg1)

    if $argc < 3
        set $max_entries = $MN_MAX_ENTRIES_DFLT
    else
        set $max_entries = ($arg2)
    end
    if $max_entries <= 0
        set $max_entries = -1
    end

    set $count = 0
    while $cur != 0 && $count != $max_entries
        printf "[entry %p]\n", $cur
        set $cur = *((uint32_t *)(((uint8_t *)$cur) + ($arg1)))

        set $count++
    end
end

document mn_gen_queue_print
usage mn_gen_queue_print <void *first-entry> <int next-offset> <int max-entries>

Prints the address of each entry in the specified queue.
    first-elem: an entry belonging to a queue; contains a 'next' pointer.
    next-offset: the offset of the next-pointer field within first-elem.
    max-elems: the maximum number of entries to print; 0 means no limit;
               dflt=100.
end

define mn_slist_print
    set $head = ($arg0)->slh_first
    if $argc < 2
        set $max_entries = $MN_MAX_ENTRIES_DFLT
    else
        set $max_entries = ($arg2)
    end

    mn_gen_queue_print $head ($arg1) $max_entries
end

document mn_slist_print
usage mn_slist_print <SLIST_HEAD *slist> <int next-offset> <int max-entries>

Prints the address of each entry in the specified slist.
    slist: an slist struct created with the SLIST_HEAD macro.
    next-offset: the offset of the next-pointer field within each entry.
    max-elems: the maximum number of entries to print; 0 means no limit;
               dflt=100.
end
