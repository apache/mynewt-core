#
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

# awk script run AFTER uncrustify reformats code to format
# STATS_NAME and STATS_SECT block to have indents removed
# by uncrustify

# detect begining of STATS_NAME section
/^(static )?STATS_NAME_START[(]/ { stats_name = 1 }
# detect end of STATN_NAME section
/^STATS_NAME_END/ { stats_name = 0 }
# for each line in sectin add 4 spaces eaten by uncrustify
/^STATS_NAME[(]/ && stats_name == 1 { printf "    " }

# detect begining of STATS_SECT section
/^STATS_SECT_START[(]/ { stats_sect = 1 }
# detect end of STATN_SECT section
/^STATS_SECT_END/ { stats_sect = 0 }
# for each line in sectin add 4 spaces eaten by uncrustify
/^STATS_SECT_ENTRY[(]/ && stats_sect == 1 { printf "    " }
{ print }
