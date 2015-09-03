#include <stdio.h>
#include <setjmp.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include "os/os.h"

extern sigset_t g_sigset;  
static jmp_buf g_mutex_test_jb;

void
os_test_arch_start(void)
{
    int rc;

    rc = setjmp(g_mutex_test_jb);
    if (rc == 0) {
        os_start();
    }
}

void
os_test_arch_stop(void)
{
    struct sigaction sa;
    struct itimerval it;
    int rc;

    g_os_started = 0;

    memset(&sa, 0, sizeof sa);
    //sigemptyset(&sa.sa_mask);
    sa.sa_handler = SIG_IGN;

    sigaction(SIGALRM, &sa, NULL);
    sigaction(SIGVTALRM, &sa, NULL);

    memset(&it, 0, sizeof(it));
    rc = setitimer(ITIMER_VIRTUAL, &it, NULL);
    if (rc != 0) {
        perror("Cannot set itimer");
        abort();
    }

    longjmp(g_mutex_test_jb, 1);
}
