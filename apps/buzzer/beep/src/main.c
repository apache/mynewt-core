#include <assert.h>

#include "sysinit/sysinit.h"
#include "os/os.h"

#include "buzzer/buzzer.h"

int count = 0;

int
main(int argc, char **argv)
{
    sysinit();

    while (1) {

        /* quarter of second */
        os_time_delay(OS_TICKS_PER_SEC / 4);

        /* each quarter of second */
        switch (count++ % 4) {

        case 0:
            /* activate buzzer tone at 2 KHz frequency */
            buzzer_tone_on(2000);
            break;

        case 1:
            /* disable buzzer */
            buzzer_tone_off();
            break;

        default:
            /* empty */
            break;
        }
    }

    assert(0);
}

