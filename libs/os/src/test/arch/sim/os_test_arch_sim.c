#include <stdio.h>
#include <setjmp.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include "os/os.h"
#include "os_test_priv.h"

void
os_test_restart(void)
{
    struct sigaction sa;
    struct itimerval it;
    int rc;

    g_os_started = 0;

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = SIG_IGN;

    sigaction(SIGALRM, &sa, NULL);
    sigaction(SIGVTALRM, &sa, NULL);

    memset(&it, 0, sizeof(it));
    rc = setitimer(ITIMER_VIRTUAL, &it, NULL);
    if (rc != 0) {
        perror("Cannot set itimer");
        abort();
    }
}
