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

define os_tasks
	set $task = g_os_task_list.stqh_first
	printf " %4s %5s %10s %6s ", "prio", "state", "stack", "stksz"
	printf "%10s %s\n", "task", "name"
	while $task != 0
		if $task == g_current_task
			set $marker = "*"
		else
			set $marker = " "
		end
		printf "%s%4d %5p ", $marker, $task->t_prio, $task->t_state
		printf "%10p %6d ", $task->t_stacktop, $task->t_stacksize
		printf "%10p %s\n", $task, $task->t_name
		set $task = $task->t_os_task_list.stqe_next
	end
end

document os_tasks
usage: os_tasks

Displays os tasks
