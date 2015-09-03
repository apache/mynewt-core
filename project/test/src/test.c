#include "os/os_test.h"
#include "ffs/ffs_test.h"
#include "bootutil/bootutil_test.h"
#include "testutil/testutil.h"

int
main(void)
{
    tu_config.tc_base_path = NULL;
    tu_config.tc_verbose = 1;
    tu_init();

    os_test_all();
    ffs_test_all();
    boot_test_all();
}
