#ifndef H_SIM_PRIV_
#define H_SIM_PRIV_

#include <os/os.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OS_USEC_PER_TICK    (1000000 / OS_TICKS_PER_SEC)

void sim_switch_tasks(void);
void sim_tick(void);
void sim_signals_init(void);
void sim_signals_cleanup(void);

extern pid_t sim_pid;

#ifdef __cplusplus
}
#endif

#endif
