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
end

define os_callouts
	printf "Callouts:\n"
	printf " %8s %10s %10s\n", "tick", "callout", "func"
	while $c != 0
		printf " %8d %10p %10p\n", $c->c_ticks, $c, $c->c_ev.ev_cb
		set $c = $c->c_next.tqe_next
	end
end

define os_sleep_list
	printf "Tasks:\n"
	printf " %8s %10s %10s\n", "tick", "task", "taskname"
	set $t = g_os_sleep_list.tqh_first
	while $t != 0
		set $no_timo = $t->t_flags & 0x1
		if $no_timo == 0
			printf " %8d %10p ", $t->t_next_wakeup, $t
			printf "%10s\n", $t->t_name
		end
		set $t = $t->t_os_list.tqe_next
	end
end

define os_wakeups
	set $c = g_callout_list.tqh_first
	printf " Now is %d\n", g_os_time
	os_callouts
	os_sleep_list
end

document os_wakeups
usage: os_wakeups
Displays scheduled OS callouts, and next scheduled task wakeups
end
