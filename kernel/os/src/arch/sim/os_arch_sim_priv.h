#ifndef H_OS_ARCH_SIM_PRIV_
#define H_OS_ARCH_SIM_PRIV_

#include <setjmp.h>
#include <os/os.h>

struct stack_frame {
    int sf_mainsp;              /* stack on which main() is executing */
    sigjmp_buf sf_jb;
    struct os_task *sf_task;
};

/*
 * Assert that 'sf_mainsp' and 'sf_jb' are at the specific offsets where
 * os_arch_frame_init() expects them to be.
 */
CTASSERT(offsetof(struct stack_frame, sf_mainsp) == 0);
CTASSERT(offsetof(struct stack_frame, sf_jb) == 4);

#define OS_USEC_PER_TICK    (1000000 / OS_TICKS_PER_SEC)

void os_arch_sim_ctx_sw(void);
void os_arch_sim_tick(void);
void os_arch_sim_signals_init(void);
void os_arch_sim_signals_cleanup(void);

extern pid_t os_arch_sim_pid;

#endif
