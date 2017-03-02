#include <ctype.h>

#include "os/os.h"
#include "rtt/SEGGER_RTT.h"

#include "console.h"

#define RTT_TASK_PRIO         (5)
#define RTT_STACK_SIZE        (512)
static struct os_task rtt_task;
static os_stack_t rtt_task_stack[RTT_STACK_SIZE];

extern void __stdout_hook_install(int (*hook)(int));

static const char CR = '\r';

static int
rtt_console_out(int character)
{
    char c = (char)character;

    if ('\n' == c) {
        SEGGER_RTT_WriteNoLock(0, &CR, 1);
    }

    SEGGER_RTT_WriteNoLock(0, &c, 1);

    return character;
}

void
rtt(void *arg)
{
    int sr;
    int key;
    int i = 0;

    while (1) {
        OS_ENTER_CRITICAL(sr);
        key = SEGGER_RTT_GetKey();
        if (key >= 0) {
            console_handle_char((char)key);
            i = 0;
        }
        OS_EXIT_CRITICAL(sr);
        /* These values were selected to keep the shell responsive
         * and at the same time reduce context switches.
         * Min sleep is 50ms and max is 250ms.
         */
        if (i < 5) {
            ++i;
        }
        os_time_delay((OS_TICKS_PER_SEC / 20) * i);
    }
}

static void
init_task(void)
{
    /* Initialize the task */
    os_task_init(&rtt_task, "rtt", rtt, NULL, RTT_TASK_PRIO,
                 OS_WAIT_FOREVER, rtt_task_stack, RTT_STACK_SIZE);
}

int
rtt_console_init()
{
    SEGGER_RTT_Init();
    __stdout_hook_install(rtt_console_out);
    init_task();
    return 0;
}
