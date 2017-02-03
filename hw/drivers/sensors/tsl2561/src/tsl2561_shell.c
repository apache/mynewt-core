/**************************************************************************/
/*!
    @file     tsl2561_shell.c
    @author   ktown (Adafruit Industries)

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2016, Adafruit Industries (adafruit.com)
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holders nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
    THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/**************************************************************************/

#include <string.h>
#include <errno.h>
#include "sysinit/sysinit.h"
#include "console/console.h"
#include "shell/shell.h"
#include "hal/hal_gpio.h"
#include "tsl2561/tsl2561.h"
#include "tsl2561_priv.h"

#if MYNEWT_VAL(TSL2561_CLI)
extern uint8_t g_tsl2561_integration_time;
extern uint8_t g_tsl2561_gain;

static int tsl2561_shell_cmd(int argc, char **argv);

static struct shell_cmd tsl2561_shell_cmd_struct = {
    .sc_cmd = "tsl2561",
    .sc_cmd_func = tsl2561_shell_cmd
};

/*
static int
tsl2561_shell_err_too_few_args(char *cmd_name)
{
    console_printf("Error: too few arguments for command \"%s\"\n",
                   cmd_name);
    return -1;
}
*/

static int
tsl2561_shell_stol(char *param_val, long min, long max, long *output)
{
    char *endptr;
    long lval;

    lval = strtol(param_val, &endptr, 10); /* Base 10 */
    if (param_val != '\0' && *endptr == '\0' &&
        lval >= min && lval <= max) {
            *output = lval;
    } else {
        return EINVAL;
    }

    return 0;
}

static int
tsl2561_shell_err_too_many_args(char *cmd_name)
{
    console_printf("Error: too many arguments for command \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int
tsl2561_shell_err_unknown_arg(char *cmd_name)
{
    console_printf("Error: unknown argument \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int
tsl2561_shell_err_invalid_arg(char *cmd_name)
{
    console_printf("Error: invalid argument \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int
tsl2561_shell_help(void)
{
    console_printf("%s cmd [flags...]\n", tsl2561_shell_cmd_struct.sc_cmd);
    console_printf("cmd:\n");
    console_printf("\tr    [n_samples]\n");
    console_printf("\tgain [1|16]\n");
    console_printf("\ttime [13|101|402]\n");
    console_printf("\ten   [0|1]\n");
    console_printf("\tint  pin [p_num(0..255)]\n");
    console_printf("\tint  on|off|clr\n");
    console_printf("\tint  set [rate(0..15)] [lower(0..65535)] [upper(0..65535)]\n");
    console_printf("\tdump\n");

    return 0;
}

static int
tsl2561_shell_cmd_read(int argc, char **argv)
{
    uint16_t full;
    uint16_t ir;
    uint16_t samples = 1;
    long val;
    int rc;
    struct tsl2561 tsl2561;

    if (argc > 3) {
        return tsl2561_shell_err_too_many_args(argv[1]);
    }

    /* Check if more than one sample requested */
    if (argc == 3) {
        if (tsl2561_shell_stol(argv[2], 1, UINT16_MAX, &val)) {
            return tsl2561_shell_err_invalid_arg(argv[2]);
        }
        samples = (uint16_t)val;
    }

    while(samples--) {

        tsl2561.cfg.gain = g_tsl2561_gain;
        tsl2561.cfg.integration_time = g_tsl2561_integration_time;

        rc = tsl2561_get_data(&full, &ir, &tsl2561);
        if (rc != 0) {
            console_printf("Read failed: %d\n", rc);
            return rc;
        }
        console_printf("Full:  %u\n", full);
        console_printf("IR:    %u\n", ir);
    }

    return 0;
}

static int
tsl2561_shell_cmd_gain(int argc, char **argv)
{
    long val;

    if (argc > 3) {
        return tsl2561_shell_err_too_many_args(argv[1]);
    }

    /* Display the gain */
    if (argc == 2) {
        uint8_t gain = tsl2561_get_gain();
        console_printf("%u\n", gain ? 16u : 1u);
    }

    /* Update the gain */
    if (argc == 3) {
        if (tsl2561_shell_stol(argv[2], 1, 16, &val)) {
            return tsl2561_shell_err_invalid_arg(argv[2]);
        }
        /* Make sure gain is 1 ot 16 */
        if ((val != 1) && (val != 16)) {
            return tsl2561_shell_err_invalid_arg(argv[2]);
        }
        tsl2561_set_gain(val ? TSL2561_LIGHT_GAIN_16X : TSL2561_LIGHT_GAIN_1X);
    }

    return 0;
}

static int
tsl2561_shell_cmd_time(int argc, char **argv)
{
    uint8_t time;
    long val;

    if (argc > 3) {
        return tsl2561_shell_err_too_many_args(argv[1]);
    }

    /* Display the integration time */
    if (argc == 2) {
        time = tsl2561_get_integration_time();
        switch (time) {
            case TSL2561_LIGHT_ITIME_13MS:
                console_printf("13\n");
            break;
            case TSL2561_LIGHT_ITIME_101MS:
                console_printf("101\n");
            break;
            case TSL2561_LIGHT_ITIME_402MS:
                console_printf("402\n");
            break;
        }
    }

    /* Set the integration time */
    if (argc == 3) {
        if (tsl2561_shell_stol(argv[2], 13, 402, &val)) {
            return tsl2561_shell_err_invalid_arg(argv[2]);
        }
        /* Make sure val is 13, 102 or 402 */
        if ((val != 13) && (val != 101) && (val != 402)) {
            return tsl2561_shell_err_invalid_arg(argv[2]);
        }
        switch(val) {
            case 13:
                tsl2561_set_integration_time(TSL2561_LIGHT_ITIME_13MS);
            break;
            case 101:
                tsl2561_set_integration_time(TSL2561_LIGHT_ITIME_101MS);
            break;
            case 402:
                tsl2561_set_integration_time(TSL2561_LIGHT_ITIME_402MS);
            break;
        }
    }

    return 0;
}

static int
tsl2561_shell_cmd_int(int argc, char **argv)
{
    int rc;
    int pin;
    long val;
    uint8_t rate;
    uint16_t lower;
    uint16_t upper;

    if (argc > 6) {
        return tsl2561_shell_err_too_many_args(argv[1]);
    }

    if (argc == 2) {
        console_printf("ToDo: Display int details\n");
        return 0;
    }

    /* Enable the interrupt */
    if (argc == 3 && strcmp(argv[2], "on") == 0) {
        return tsl2561_enable_interrupt(1);
    }

    /* Disable the interrupt */
    if (argc == 3 && strcmp(argv[2], "off") == 0) {
        return tsl2561_enable_interrupt(0);
    }

    /* Clear the interrupt on 'clr' */
    if (argc == 3 && strcmp(argv[2], "clr") == 0) {
        return tsl2561_clear_interrupt();
    }

    /* Configure the interrupt on 'set' */
    if (argc == 6 && strcmp(argv[2], "set") == 0) {
        /* Get rate */
        if (tsl2561_shell_stol(argv[3], 0, 15, &val)) {
            return tsl2561_shell_err_invalid_arg(argv[3]);
        }
        rate = (uint8_t)val;
        /* Get lower threshold */
        if (tsl2561_shell_stol(argv[4], 0, UINT16_MAX, &val)) {
            return tsl2561_shell_err_invalid_arg(argv[4]);
        }
        lower = (uint16_t)val;
        /* Get upper threshold */
        if (tsl2561_shell_stol(argv[5], 0, UINT16_MAX, &val)) {
            return tsl2561_shell_err_invalid_arg(argv[5]);
        }
        upper = (uint16_t)val;
        /* Set the values */
        rc = tsl2561_setup_interrupt(rate, lower, upper);
        console_printf("Configured interrupt as:\n");
        console_printf("\trate: %u\n", rate);
        console_printf("\tlower: %u\n", lower);
        console_printf("\tupper: %u\n", upper);
        return rc;
    }

    /* Setup INT pin on 'pin' */
    if (argc == 4 && strcmp(argv[2], "pin") == 0) {
        /* Get the pin number */
        if (tsl2561_shell_stol(argv[3], 0, 0xFF, &val)) {
            return tsl2561_shell_err_invalid_arg(argv[3]);
        }
        pin = (int)val;
        /* INT is open drain, pullup is required */
        rc = hal_gpio_init_in(pin, HAL_GPIO_PULL_UP);
        assert(rc == 0);
        console_printf("Set pin \"%d\" to INPUT with pull up enabled\n", pin);
        return 0;
    }

    /* Unknown command */
    return tsl2561_shell_err_invalid_arg(argv[2]);
}

static int
tsl2561_shell_cmd_en(int argc, char **argv)
{
    char *endptr;
    long lval;

    if (argc > 3) {
        return tsl2561_shell_err_too_many_args(argv[1]);
    }

    /* Display current enable state */
    if (argc == 2) {
        console_printf("%u\n", tsl2561_get_enable());
    }

    /* Update the enable state */
    if (argc == 3) {
        lval = strtol(argv[2], &endptr, 10); /* Base 10 */
        if (argv[2] != '\0' && *endptr == '\0' &&
            lval >= 0 && lval <= 1) {
                tsl2561_enable(lval);
        } else {
            return tsl2561_shell_err_invalid_arg(argv[2]);
        }
    }

    return 0;
}

static int
tsl2561_shell_cmd_dump(int argc, char **argv)
{
  uint8_t val;

  if (argc > 3) {
      return tsl2561_shell_err_too_many_args(argv[1]);
  }

  /* Dump all the register values for debug purposes */
  val = 0;
  assert(0 == tsl2561_read8(TSL2561_COMMAND_BIT | TSL2561_REGISTER_CONTROL, &val));
  console_printf("0x%02X (CONTROL): 0x%02X\n", TSL2561_REGISTER_CONTROL, val);
  assert(0 == tsl2561_read8(TSL2561_COMMAND_BIT | TSL2561_REGISTER_TIMING, &val));
  console_printf("0x%02X (TIMING):  0x%02X\n", TSL2561_REGISTER_TIMING, val);
  assert(0 == tsl2561_read8(TSL2561_COMMAND_BIT | TSL2561_REGISTER_THRESHHOLDL_LOW, &val));
  console_printf("0x%02X (THRLL):   0x%02X\n", TSL2561_REGISTER_THRESHHOLDL_LOW, val);
  assert(0 == tsl2561_read8(TSL2561_COMMAND_BIT | TSL2561_REGISTER_THRESHHOLDL_HIGH, &val));
  console_printf("0x%02X (THRLH):   0x%02X\n", TSL2561_REGISTER_THRESHHOLDL_HIGH, val);
  assert(0 == tsl2561_read8(TSL2561_COMMAND_BIT | TSL2561_REGISTER_THRESHHOLDH_LOW, &val));
  console_printf("0x%02X (THRHL):   0x%02X\n", TSL2561_REGISTER_THRESHHOLDH_LOW, val);
  assert(0 == tsl2561_read8(TSL2561_COMMAND_BIT | TSL2561_REGISTER_THRESHHOLDH_HIGH, &val));
  console_printf("0x%02X (THRHH):   0x%02X\n", TSL2561_REGISTER_THRESHHOLDH_HIGH, val);
  assert(0 == tsl2561_read8(TSL2561_COMMAND_BIT | TSL2561_REGISTER_INTERRUPT, &val));
  console_printf("0x%02X (INTER):   0x%02X\n", TSL2561_REGISTER_INTERRUPT, val);
  assert(0 == tsl2561_read8(TSL2561_COMMAND_BIT | TSL2561_REGISTER_ID, &val));
  console_printf("0x%02X (ID):      0x%02X\n", TSL2561_REGISTER_ID, val);

  return 0;
}

static int
tsl2561_shell_cmd(int argc, char **argv)
{
    if (argc == 1) {
        return tsl2561_shell_help();
    }

    /* Read command (get a new data sample) */
    if (argc > 1 && strcmp(argv[1], "r") == 0) {
        return tsl2561_shell_cmd_read(argc, argv);
    }

    /* Gain command */
    if (argc > 1 && strcmp(argv[1], "gain") == 0) {
        return tsl2561_shell_cmd_gain(argc, argv);
    }

    /* Integration time command */
    if (argc > 1 && strcmp(argv[1], "time") == 0) {
        return tsl2561_shell_cmd_time(argc, argv);
    }

    /* Enable */
    if (argc > 1 && strcmp(argv[1], "en") == 0) {
        return tsl2561_shell_cmd_en(argc, argv);
    }

    /* Interrupt */
    if (argc > 1 && strcmp(argv[1], "int") == 0) {
        return tsl2561_shell_cmd_int(argc, argv);
    }

    /* Debug */
    if (argc > 1 && strcmp(argv[1], "dump") == 0) {
        return tsl2561_shell_cmd_dump(argc, argv);
    }

    return tsl2561_shell_err_unknown_arg(argv[1]);
}

int
tsl2561_shell_init(void)
{
    int rc;

    rc = shell_cmd_register(&tsl2561_shell_cmd_struct);
    SYSINIT_PANIC_ASSERT(rc == 0);

    return rc;
}

#endif
