#include <assert.h>
#include "testutil/testutil.h"
#include "testutil_priv.h"

const char *tu_suite_name;

/* This gets optimized out without the initialization. */
int tu_suite_failed = 0;

static void
tu_suite_set_name(const char *name)
{
    tu_suite_name = name;
}

void
tu_suite_init(const char *name)
{
    int rc;

    tu_suite_failed = 0;

    tu_suite_set_name(name);
    rc = tu_report_mkdir_suite();
    assert(rc == 0);
}
