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

# Print the linked list of nffs hash entries starting with the input argument
#       (gdb) hash-list <struct nffs_hash_entry *>
#
define nffs-hash-list
 set $next = $arg0
 while $next
  p $next
  if nffs_hash_id_is_inode($next.nhe_id)
    p *(struct nffs_inode_entry*)$next
  else
    p *$next
  end
  set $next = $next.nhe_next.sle_next
  if $next
   echo ---v\n
  end
 end
end

# Dump the nffs hash table starting with the first element
# The linked list of hash entries is printed out for each
# element in the hash array
#
define nffs-hash-table
 printf "nffs hash table:\n"
 set $hashp = nffs_hash->slh_first
 set $i = 0
 while $i < 256
  if $hashp
   echo Hash table offset\ \  
   output/d $i
   echo \n
   nffs-hash-list $hashp
   echo ====\n
  end
  set $i = $i + 1
  set $hashp = (nffs_hash + $i)->slh_first
 end
end

# print out the nffs pool information
#
define nffs-print-config
 printf "nffs_config:\n"
 printf "inodes: alloc %d free %d\n", nffs_config.nc_num_inodes, nffs_inode_entry_pool.mp_num_free
 printf "blocks: alloc %d free %d\n", nffs_config.nc_num_blocks, nffs_block_entry_pool.mp_num_free
 printf "files: alloc %d free %d\n", nffs_config.nc_num_files, nffs_file_pool.mp_num_free
 printf "dirs: alloc %d free %d\n", nffs_config.nc_num_dirs, nffs_dir_pool.mp_num_free
 printf "cache_inodes: alloc %d free %d\n", nffs_config.nc_num_cache_inodes, nffs_cache_inode_pool.mp_num_free
 printf "cache_blocks: alloc %d free %d\n", nffs_config.nc_num_cache_blocks, nffs_cache_block_pool.mp_num_free
end

# print out the nffs counters
#
define nffs-print-stats
 printf "nffs stats:\n"
 printf "Objects inserted in hash table %d\n", nffs_stats.snffs_hashcnt_ins
 printf "Objects deleted in hash table %d\n", nffs_stats.snffs_hashcnt_rm
 printf "Object restored on initialization %d\n", nffs_stats.snffs_object_count
 printf "Total count of flash reads %d\n", nffs_stats.snffs_iocnt_read
 printf "Total count of flash writes %d\n", nffs_stats.snffs_iocnt_write
 printf "Data block reads %d\n", nffs_stats.snffs_readcnt_data
 printf "Block struct reads %d\n", nffs_stats.snffs_readcnt_block
 printf "Per-object crc reads %d\n", nffs_stats.snffs_readcnt_crc
 printf "Copy to/from flash ops %d\n", nffs_stats.snffs_readcnt_copy
 printf "Format scratch area ops %d\n", nffs_stats.snffs_readcnt_format
 printf "Coalesce block data ops %d\n", nffs_stats.snffs_readcnt_gccollate
 printf "Inodes read %d\n", nffs_stats.snffs_readcnt_inode
 printf "Inode reads from hash entry %d\n", nffs_stats.snffs_readcnt_inodeent
 printf "File renames %d\n", nffs_stats.snffs_readcnt_rename
 printf "Inode updates %d\n", nffs_stats.snffs_readcnt_update
 printf "Filename reads %d\n", nffs_stats.snffs_readcnt_filename
 printf "Flash objects read %d\n", nffs_stats.snffs_readcnt_object
 printf "Flash areas read %d\n", nffs_stats.snffs_readcnt_detect
end

#
# Platform dependent macros below

# Copy the nffs partition out to a file.
# Addresses are specific to arduino-zero
#
define scratch-dump-arduino0
 dump binary memory $arg0 0x3c000 0x3dc00
end

# Copy the nffs partition out to a file.
# Addresses are specific to arduino-zero
#
define nffs-dump-arduino0
 dump binary memory $arg0 0x3e000 0x40000
end

# Erase the nffs partition - arduino-zero specific addresses
#
define nffs-clear-arduino0
  monitor flash erase_address 0xc000 0x34000
end

# dump the nffs stats and data structures
# Logs the output in the file "gdb.txt"
# Creates the file nffs-dump_tmp.bin in the current directory which can be
# used with ffs2native to print out the contents of the nffs flash areas.
# Use command syntax: "ffs2native -s -f nffs-dump.bin"
#
define nffs-snapshot
 set logging on
 set pagination off
 nffs-print-config
 printf "\n"
 nffs-print-stats
 printf "\n"
 nffs-hash-table
 set logging off
 nffs-dump-arduino0 nffs-dump.bin
end
