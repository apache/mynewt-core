#include <errno.h>
#include <unistd.h>

#include "testutil/testutil.h"

int
tu_parse_args(int argc, char **argv)
{
    int ch;

    while ((ch = getopt(argc, argv, "s")) != -1) {
        switch (ch) {
        case 's':
            tu_config.tc_system_assert = 1;
            break;

        default:
            return EINVAL;
        }
    }

    return 0;
}
