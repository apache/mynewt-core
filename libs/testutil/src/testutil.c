#include <assert.h>
#include "testutil/testutil.h"
#include "testutil_priv.h"

struct tu_config tu_config;

/* This gets optimized out without the initialization. */
int tu_any_failed = 0;

int
tu_init(void)
{
    int rc;

    if (tu_config.tc_base_path == NULL) {
        return -1;
    }

    rc = tu_report_mkdir_results();
    assert(rc == 0);

    tu_any_failed = 0;

    return 0;
}

