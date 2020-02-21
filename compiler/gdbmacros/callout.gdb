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

define mn_callout_print
    set $cur = ($arg0)

    if $argc < 2 
        set $max_entries = $MN_MAX_ENTRIES_DFLT
    else
        set $max_entries = ($arg1)
    end
    if $max_entries <= 0
        set $max_entries = -1
    end

    set $count = 0
    set $co = (struct os_callout *)$cur->tqh_first
    while $co != 0 && $count != $max_entries
        set $c_next = $co->c_next.tqe_next
        set $c_ticks = $co->c_ticks
        printf "%u ", $count
        info symbol $co
        printf "[callout %p] [c_ticks %u] [c_next %p] expires in %u ticks\n", $co, $c_ticks, $c_next, $c_ticks - g_os_time
        set $co = $c_next
        set $count++
    end
end

document mn_callout_print
usage mn_callout_print <void *first-entry>

Prints the address of each callout, c_ticks and address of the next callout in
the specified callout list
    first-elem: an entry belonging to a callout;
    max-elems: the maximum number of entries to print; 0 means no limit;
               dflt=100.
end
