#ifndef H_OS_PRIV_
#define H_OS_PRIV_

TAILQ_HEAD(os_task_list, os_task);

extern struct os_task_list g_os_run_list;
extern struct os_task_list g_os_sleep_list;
extern struct os_task *g_current_task;

#endif
