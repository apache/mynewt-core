#include <assert.h>
#include <stddef.h>
#include "testutil/testutil.h"
#include "os/os_test.h"
#include "os_test_priv.h"

int
os_test_all(void)
{
    os_mempool_test_suite();
    os_mutex_test_suite();
    os_sem_test_suite();

    return tu_case_failed;
}

#ifdef PKG_TEST

int
main(int argc, char **argv)
{
    tu_config.tc_base_path = NULL;
    tu_config.tc_verbose = 1;
    tu_init();

    os_test_all();

    return tu_any_failed;
}

#endif
