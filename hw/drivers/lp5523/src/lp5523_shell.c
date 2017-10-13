/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <string.h>
#include <errno.h>
#include "sysinit/sysinit.h"
#include "console/console.h"
#include "sensor/sensor.h"
#include "shell/shell.h"
#include "hal/hal_gpio.h"
#include "lp5523/lp5523.h"
#include "parse/parse.h"

#if MYNEWT_VAL(LP5523_CLI)

static int lp5523_shell_cmd(int argc, char **argv);

static struct shell_cmd lp5523_shell_cmd_struct = {
    .sc_cmd = "lp5523",
    .sc_cmd_func = lp5523_shell_cmd
};

static struct sensor_itf lp5523_itf = {
    .si_type = MYNEWT_VAL(LP5523_SHELL_ITF_TYPE),
    .si_num = MYNEWT_VAL(LP5523_SHELL_ITF_NUM),
    .si_addr = MYNEWT_VAL(LP5523_SHELL_ITF_ADDR)
};

static int
lp5523_shell_err_too_few_args(char *cmd_name)
{
    console_printf("Error: too few arguments for command \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int
lp5523_shell_err_too_many_args(char *cmd_name)
{
    console_printf("Error: too many arguments for command \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int
lp5523_shell_err_unknown_arg(char *cmd_name)
{
    console_printf("Error: unknown argument \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int
lp5523_shell_err_invalid_arg(char *cmd_name)
{
    console_printf("Error: invalid argument \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int
lp5523_shell_help(void)
{
    console_printf("%s cmd [flags...]\n", lp5523_shell_cmd_struct.sc_cmd);
    console_printf("cmd:\n");
    console_printf("\treset\n");
    console_printf("\tenable\t[output]\n");
    console_printf("\tdisable\t[output]\n");
    console_printf("\tpwm\t[output | value]\n");
    console_printf("\tins\t[XXX]\n");
    return 0;
}

static int
lp5523_shell_cmd_reset(int argc, char **argv)
{
    return lp5523_reset(&lp5523_itf);
}

static int
lp5523_shell_cmd_enable(int argc, char **argv)
{
    return lp5523_set_enable(&lp5523_itf, 1);
}

static int
lp5523_shell_cmd_disable(int argc, char **argv)
{
    return lp5523_set_enable(&lp5523_itf, 0);
}

static int
lp5523_shell_cmd_pwm(int argc, char **argv)
{
    int rc;
    uint8_t output;
    uint8_t value;

    if (argc > 4) {
        return lp5523_shell_err_too_many_args(argv[1]);
    } else if (argc < 4) {
        return lp5523_shell_err_too_few_args(argv[1]);
    }

    output = parse_ll_bounds(argv[2], 1, 9, &rc);
    if (rc != 0) {
        return lp5523_shell_err_invalid_arg(argv[2]);
    }
    value = parse_ll_bounds(argv[3], 0, 255, &rc);
    if (rc != 0) {
        return lp5523_shell_err_invalid_arg(argv[3]);
    }

    return lp5523_set_output_reg(&lp5523_itf, LP5523_PWM, output, value);
}

static int
lp5523_shell_cmd_ins_ramp(int argc, char **argv, uint8_t addr)
{
    int rc;
    int snoi;
    uint8_t prescale;
    uint8_t step_time;
    uint8_t sign;
    uint8_t noi;

    if (argc > 8) {
        return lp5523_shell_err_too_many_args(argv[1]);
    } else if (argc < 8) {
        return lp5523_shell_err_too_few_args(argv[1]);
    }

    if (strcmp(argv[4], "im") == 0) {
        prescale = parse_ll_bounds(argv[5], 0, 1, &rc);
        if (rc != 0) {
            return lp5523_shell_err_invalid_arg(argv[5]);
        }

        step_time = parse_ll_bounds(argv[6], 0, 31, &rc);
        if (rc != 0) {
            return lp5523_shell_err_invalid_arg(argv[6]);
        }

        snoi = parse_ll_bounds(argv[7], -255, 255, &rc);
        if (rc != 0) {
            return lp5523_shell_err_invalid_arg(argv[7]);
        }

        if (snoi < 0) {
            sign = 1;
            noi = (uint8_t)-snoi;
        } else {
            sign = 0;
            noi = (uint8_t)snoi;
        }

        return lp5523_set_instruction(&lp5523_itf, addr,
            LP5523_INS_RAMP_IM(prescale, step_time, sign, noi));
    }

    prescale = parse_ll_bounds(argv[4], 0, 1, &rc);
    if (rc != 0) {
        return lp5523_shell_err_invalid_arg(argv[4]);
    }

    step_time = parse_ll_bounds(argv[5], 0, 3, &rc);
    if (rc != 0) {
        return lp5523_shell_err_invalid_arg(argv[5]);
    }

    sign = parse_ll_bounds(argv[6], 0, 1, &rc);
    if (rc != 0) {
        return lp5523_shell_err_invalid_arg(argv[6]);
    }

    noi = parse_ll_bounds(argv[7], 0, 3, &rc);
    if (rc != 0) {
        return lp5523_shell_err_invalid_arg(argv[7]);
    }

    return lp5523_set_instruction(&lp5523_itf, addr,
        LP5523_INS_RAMP(prescale, step_time, sign, noi));
}

static int
lp5523_shell_cmd_ins_pwm(int argc, char **argv, uint8_t addr)
{
    int rc;
    uint8_t pwm;

    if (argc > 6) {
        return lp5523_shell_err_too_many_args(argv[1]);
    } else if (argc < 5) {
        return lp5523_shell_err_too_few_args(argv[1]);
    }


    if (strcmp(argv[4], "im") == 0) {
        if (argc < 6) {
            return lp5523_shell_err_too_few_args(argv[1]);
        }

        pwm = parse_ll_bounds(argv[5], 0, 255, &rc);
        if (rc != 0) {
            return lp5523_shell_err_invalid_arg(argv[5]);
        }

        return lp5523_set_instruction(&lp5523_itf, addr, LP5523_INS_SET_PWM_IM(pwm));
    }
    if (argc > 5) {
        return lp5523_shell_err_too_many_args(argv[1]);
    }

    pwm = parse_ll_bounds(argv[4], 0, 3, &rc);
    if (rc != 0) {
        return lp5523_shell_err_invalid_arg(argv[4]);
    }

    return lp5523_set_instruction(&lp5523_itf, addr, LP5523_INS_SET_PWM(pwm));
}

static int
lp5523_shell_cmd_ins_wait(int argc, char **argv, uint8_t addr)
{
    int rc;
    uint8_t prescale;
    uint8_t step_time;

    if (argc > 6) {
        return lp5523_shell_err_too_many_args(argv[1]);
    } else if (argc < 6) {
        return lp5523_shell_err_too_few_args(argv[1]);
    }

    prescale = parse_ll_bounds(argv[4], 0, 1, &rc);
    if (rc != 0) {
        return lp5523_shell_err_invalid_arg(argv[4]);
    }

    step_time = parse_ll_bounds(argv[5], 0, 31, &rc);
    if (rc != 0) {
        return lp5523_shell_err_invalid_arg(argv[5]);
    }

    return lp5523_set_instruction(&lp5523_itf, addr, LP5523_INS_WAIT(prescale,
        step_time));
}

static int
lp5523_shell_cmd_ins_mux_addr(int argc, char **argv, uint8_t *addr)
{
    int rc;
    
    if (argc < 7) {
        return lp5523_shell_err_too_few_args(argv[1]);
    }

    *addr = parse_ll_bounds(argv[6], 0, 95, &rc);
    if (rc != 0) {
        return lp5523_shell_err_invalid_arg(argv[6]);
    }
    
    return 0;
}

static int
lp5523_shell_cmd_ins_mux(int argc, char **argv, uint8_t addr)
{
    int rc;

    if (argc > 7) {
        return lp5523_shell_err_too_many_args(argv[1]);
    } else if (argc < 5) {
        return lp5523_shell_err_too_few_args(argv[1]);
    }

    if (strcmp(argv[4], "ld") == 0) {
        if (argc < 6) {
            return lp5523_shell_err_too_few_args(argv[1]);
        }
        if (strcmp(argv[5], "start") == 0) {
            uint8_t addr;
            
            rc = lp5523_shell_cmd_ins_mux_addr(argc, argv, &addr);
            if (rc != 0) {
                return rc;
            }

            return lp5523_set_instruction(&lp5523_itf, addr,
                LP5523_INS_MUX_LD_START(addr));
        }
        if (strcmp(argv[5], "end") == 0) {
            uint8_t addr;

            rc = lp5523_shell_cmd_ins_mux_addr(argc, argv, &addr);
            if (rc != 0) {
                return rc;
            }

            return lp5523_set_instruction(&lp5523_itf, addr,
                LP5523_INS_MUX_LD_END(addr));
        }
        if (strcmp(argv[5], "addr") == 0) {
            uint8_t addr;

            rc = lp5523_shell_cmd_ins_mux_addr(argc, argv, &addr);
            if (rc != 0) {
                return rc;
            }

            return lp5523_set_instruction(&lp5523_itf, addr,
                LP5523_INS_MUX_LD_ADDR(addr));
        }
        if (strcmp(argv[5], "next") == 0) {
            if (argc > 6) {
                return lp5523_shell_err_too_many_args(argv[1]);
            }

            return lp5523_set_instruction(&lp5523_itf, addr,
                LP5523_INS_MUX_LD_NEXT());
        }
        if (strcmp(argv[5], "prev") == 0) {
            if (argc > 6) {
                return lp5523_shell_err_too_many_args(argv[1]);
            }

            return lp5523_set_instruction(&lp5523_itf, addr,
                LP5523_INS_MUX_LD_PREV());
        }
        return lp5523_shell_err_invalid_arg(argv[5]);
    }
    if (strcmp(argv[4], "map") == 0) {
        if (argc < 6) {
            return lp5523_shell_err_too_few_args(argv[1]);
        }
        if (strcmp(argv[5], "start") == 0) {
            uint8_t addr;

            rc = lp5523_shell_cmd_ins_mux_addr(argc, argv, &addr);
            if (rc != 0) {
                return rc;
            }

            return lp5523_set_instruction(&lp5523_itf, addr,
                LP5523_INS_MUX_MAP_START(addr));
        }
        if (strcmp(argv[5], "addr") == 0) {
            uint8_t addr;

            rc = lp5523_shell_cmd_ins_mux_addr(argc, argv, &addr);
            if (rc != 0) {
                return rc;
            }

            return lp5523_set_instruction(&lp5523_itf, addr,
                LP5523_INS_MUX_MAP_ADDR(addr));
        }
        if (strcmp(argv[5], "next") == 0) {
            if (argc > 6) {
                return lp5523_shell_err_too_many_args(argv[1]);
            }

            return lp5523_set_instruction(&lp5523_itf, addr,
                LP5523_INS_MUX_MAP_NEXT());
        }
        if (strcmp(argv[5], "prev") == 0) {
            if (argc > 6) {
                return lp5523_shell_err_too_many_args(argv[1]);
            }

            return lp5523_set_instruction(&lp5523_itf, addr,
                LP5523_INS_MUX_MAP_PREV());
        }
        return lp5523_shell_err_invalid_arg(argv[5]);
    }
    if (strcmp(argv[4], "sel") == 0) {
        uint8_t sel;

        if (argc > 6) {
            return lp5523_shell_err_too_many_args(argv[1]);
        } else if (argc < 6) {
            return lp5523_shell_err_too_few_args(argv[1]);
        }

        sel = parse_ll_bounds(argv[5], 0, 127, &rc);
        if (rc != 0) {
            return lp5523_shell_err_invalid_arg(argv[5]);
        }

        return lp5523_set_instruction(&lp5523_itf, addr,
            LP5523_INS_MUX_SEL(sel));
    }
    if (strcmp(argv[4], "clr") == 0) {
        if (argc > 5) {
            return lp5523_shell_err_too_many_args(argv[1]);
        }
        return lp5523_set_instruction(&lp5523_itf, addr, LP5523_INS_MUX_CLR());
    }

    return lp5523_shell_err_invalid_arg(argv[4]);
}

static int
lp5523_shell_cmd_ins_rst(int argc, char **argv, uint8_t addr)
{
    if (argc > 4) {
        return lp5523_shell_err_too_many_args(argv[1]);
    } else if (argc < 4) {
        return lp5523_shell_err_too_few_args(argv[1]);
    }

    return lp5523_set_instruction(&lp5523_itf, addr, LP5523_INS_RST());
}

static int
lp5523_shell_cmd_ins_branch(int argc, char **argv, uint8_t addr)
{
    int rc;
    uint8_t loop_count;
    uint8_t step_number;

    if (argc > 7) {
        return lp5523_shell_err_too_many_args(argv[1]);
    } else if (argc < 6) {
        return lp5523_shell_err_too_few_args(argv[1]);
    }

    if (strcmp(argv[4], "im") == 0) {
        if (argc < 7) {
            return lp5523_shell_err_too_few_args(argv[1]);
        }

        loop_count = parse_ll_bounds(argv[5], 0, 63, &rc);
        if (rc != 0) {
            return lp5523_shell_err_invalid_arg(argv[5]);
        }

        step_number = parse_ll_bounds(argv[6], 0, 95, &rc);
        if (rc != 0) {
            return lp5523_shell_err_invalid_arg(argv[6]);
        }

        return lp5523_set_instruction(&lp5523_itf, addr,
            LP5523_INS_BRANCH_IM(loop_count, step_number));
    }
    if (argc > 6) {
        return lp5523_shell_err_too_many_args(argv[1]);
    }

    step_number = parse_ll_bounds(argv[4], 0, 95, &rc);
    if (rc != 0) {
        return lp5523_shell_err_invalid_arg(argv[4]);
    }

    loop_count = parse_ll_bounds(argv[5], 0, 3, &rc);
    if (rc != 0) {
        return lp5523_shell_err_invalid_arg(argv[5]);
    }

    return lp5523_set_instruction(&lp5523_itf, addr,
        LP5523_INS_BRANCH(step_number, loop_count));
}

static int
lp5523_shell_cmd_ins_int(int argc, char **argv, uint8_t addr)
{
    if (argc > 3) {
        return lp5523_shell_err_too_many_args(argv[1]);
    } else if (argc < 3) {
        return lp5523_shell_err_too_few_args(argv[1]);
    }

    return lp5523_set_instruction(&lp5523_itf, addr, LP5523_INS_INT());
}

static int
lp5523_shell_cmd_ins_end(int argc, char **argv, uint8_t addr)
{
    int rc;
    uint8_t interrupt;
    uint8_t reset;

    if (argc > 6) {
        return lp5523_shell_err_too_many_args(argv[1]);
    } else if (argc < 6) {
        return lp5523_shell_err_too_few_args(argv[1]);
    }

    interrupt = parse_ll_bounds(argv[4], 0, 1, &rc);
    if (rc != 0) {
        return lp5523_shell_err_invalid_arg(argv[4]);
    }

    reset = parse_ll_bounds(argv[5], 0, 1, &rc);
    if (rc != 0) {
        return lp5523_shell_err_invalid_arg(argv[5]);
    }

    return lp5523_set_instruction(&lp5523_itf, addr,
        LP5523_INS_END(interrupt, reset));
}

static int
lp5523_shell_cmd_ins_trigger(int argc, char **argv, uint8_t addr)
{
    int rc;
    uint8_t wait_external;
    uint8_t wait_engines;
    uint8_t send_external;
    uint8_t send_engines;

    if (argc > 8) {
        return lp5523_shell_err_too_many_args(argv[1]);
    } else if (argc < 8) {
        return lp5523_shell_err_too_few_args(argv[1]);
    }

    wait_external = parse_ll_bounds(argv[4], 0, 1, &rc);
    if (rc != 0) {
        return lp5523_shell_err_invalid_arg(argv[4]);
    }

    wait_engines = parse_ll_bounds(argv[5], 0, 7, &rc);
    if (rc != 0) {
        return lp5523_shell_err_invalid_arg(argv[5]);
    }

    send_external = parse_ll_bounds(argv[6], 0, 1, &rc);
    if (rc != 0) {
        return lp5523_shell_err_invalid_arg(argv[6]);
    }

    send_engines = parse_ll_bounds(argv[7], 0, 7, &rc);
    if (rc != 0) {
        return lp5523_shell_err_invalid_arg(argv[7]);
    }

    return lp5523_set_instruction(&lp5523_itf, addr,
        LP5523_INS_TRIGGER(wait_external, wait_engines, send_external,
            send_engines));
}

static int
lp5523_shell_cmd_ins_j(int argc, char **argv, uint8_t addr)
{
    int rc;
    uint8_t skip;
    uint8_t variable1;
    uint8_t variable2;

    if (argc > 7) {
        return lp5523_shell_err_too_many_args(argv[1]);
    } else if (argc < 7) {
        return lp5523_shell_err_too_few_args(argv[1]);
    }

    skip = parse_ll_bounds(argv[4], 0, 31, &rc);
    if (rc != 0) {
        return lp5523_shell_err_invalid_arg(argv[4]);
    }

    variable1 = parse_ll_bounds(argv[5], 0, 3, &rc);
    if (rc != 0) {
        return lp5523_shell_err_invalid_arg(argv[5]);
    }

    variable2 = parse_ll_bounds(argv[6], 0, 3, &rc);
    if (rc != 0) {
        return lp5523_shell_err_invalid_arg(argv[6]);
    }

    if (strcmp(argv[3], "jne") == 0) {
        return lp5523_set_instruction(&lp5523_itf, addr,
            LP5523_INS_JNE(skip, variable1, variable2));
    }
    if (strcmp(argv[3], "jl") == 0) {
        return lp5523_set_instruction(&lp5523_itf, addr,
            LP5523_INS_JL(skip, variable1, variable2));
    }
    if (strcmp(argv[3], "jge") == 0) {
        return lp5523_set_instruction(&lp5523_itf, addr,
            LP5523_INS_JGE(skip, variable1, variable2));
    }
    if (strcmp(argv[3], "je") == 0) {
        return lp5523_set_instruction(&lp5523_itf, addr,
            LP5523_INS_JE(skip, variable1, variable2));
    }

    return lp5523_shell_err_invalid_arg(argv[3]);
}

static int
lp5523_shell_cmd_ins_ld(int argc, char **argv, uint8_t addr)
{
    int rc;
    uint8_t target;
    uint8_t value;

    if (argc > 6) {
        return lp5523_shell_err_too_many_args(argv[1]);
    } else if (argc < 6) {
        return lp5523_shell_err_too_few_args(argv[1]);
    }

    target = parse_ll_bounds(argv[4], 0, 3, &rc);
    if (rc != 0) {
        return lp5523_shell_err_invalid_arg(argv[4]);
    }

    value = parse_ll_bounds(argv[5], 0, 255, &rc);
    if (rc != 0) {
        return lp5523_shell_err_invalid_arg(argv[5]);
    }

    return lp5523_set_instruction(&lp5523_itf, addr,
        LP5523_INS_LD(target, value));
}

static int
lp5523_shell_cmd_ins_add_sub(int argc, char **argv, uint8_t addr, uint8_t sub)
{
    int rc;
    uint8_t target;
    uint8_t variable1;
    uint8_t variable2;

    if (argc > 7) {
        return lp5523_shell_err_too_many_args(argv[1]);
    } else if (argc < 7) {
        return lp5523_shell_err_too_few_args(argv[1]);
    }

    if (strcmp(argv[4], "im") == 0) {
        target = parse_ll_bounds(argv[5], 0, 3, &rc);
        if (rc != 0) {
            return lp5523_shell_err_invalid_arg(argv[5]);
        }

        variable1 = parse_ll_bounds(argv[6], 0, 255, &rc);
        if (rc != 0) {
            return lp5523_shell_err_invalid_arg(argv[6]);
        }

        return lp5523_set_instruction(&lp5523_itf, addr, (sub != 0) ?
            LP5523_INS_SUB_IM(target, variable1) :
            LP5523_INS_ADD_IM(target, variable1));
    }
    target = parse_ll_bounds(argv[4], 0, 3, &rc);
    if (rc != 0) {
        return lp5523_shell_err_invalid_arg(argv[4]);
    }

    variable1 = parse_ll_bounds(argv[5], 0, 3, &rc);
    if (rc != 0) {
        return lp5523_shell_err_invalid_arg(argv[5]);
    }

    variable2 = parse_ll_bounds(argv[6], 0, 3, &rc);
    if (rc != 0) {
        return lp5523_shell_err_invalid_arg(argv[6]);
    }

    return lp5523_set_instruction(&lp5523_itf, addr, (sub != 0) ?
        LP5523_INS_SUB(target, variable1, variable2) :
        LP5523_INS_ADD(target, variable1, variable2));
}

static int
lp5523_shell_cmd_ins_add(int argc, char **argv, uint8_t addr)
{
    return lp5523_shell_cmd_ins_add_sub(argc, argv, addr, 0);
}

static int
lp5523_shell_cmd_ins_sub(int argc, char **argv, uint8_t addr)
{
    return lp5523_shell_cmd_ins_add_sub(argc, argv, addr, 1);
}

static int
lp5523_shell_cmd_ins(int argc, char **argv)
{
    int rc;
    uint8_t addr;

    if (argc < 4) {
        return lp5523_shell_err_too_few_args(argv[1]);
    }

    /* get addr */
    addr = parse_ll_bounds(argv[2], 0, 95, &rc);
    if (rc != 0) {
        return lp5523_shell_err_invalid_arg(argv[2]);
    }

    if (strcmp(argv[3], "ramp") == 0) {
        return lp5523_shell_cmd_ins_ramp(argc, argv, addr);
    }

    if (strcmp(argv[3], "pwm") == 0) {
        return lp5523_shell_cmd_ins_pwm(argc, argv, addr);
    }

    if (strcmp(argv[3], "wait") == 0) {
        return lp5523_shell_cmd_ins_wait(argc, argv, addr);
    }

    if (strcmp(argv[3], "mux") == 0) {
        return lp5523_shell_cmd_ins_mux(argc, argv, addr);
    }

    if (strcmp(argv[3], "rst") == 0) {
        return lp5523_shell_cmd_ins_rst(argc, argv, addr);
    }

    if (strcmp(argv[3], "branch") == 0) {
        return lp5523_shell_cmd_ins_branch(argc, argv, addr);
    }

    if (strcmp(argv[3], "int") == 0) {
        return lp5523_shell_cmd_ins_int(argc, argv, addr);
    }

    if (strcmp(argv[3], "end") == 0) {
        return lp5523_shell_cmd_ins_end(argc, argv, addr);
    }

    if (strcmp(argv[3], "trigger") == 0) {
        return lp5523_shell_cmd_ins_trigger(argc, argv, addr);
    }

    if (argv[3][0] == 'j') {
        return lp5523_shell_cmd_ins_j(argc, argv, addr);
    }

    if (strcmp(argv[3], "ld") == 0) {
        return lp5523_shell_cmd_ins_ld(argc, argv, addr);
    }

    if (strcmp(argv[3], "add") == 0) {
        return lp5523_shell_cmd_ins_add(argc, argv, addr);
    }

    if (strcmp(argv[3], "sub") == 0) {
        return lp5523_shell_cmd_ins_sub(argc, argv, addr);
    }

    return lp5523_shell_err_unknown_arg(argv[2]);
}

static int
lp5523_shell_cmd_start(int argc, char **argv)
{
    int rc;
    uint8_t engine;
    uint8_t addr;

    if (argc < 4) {
        return lp5523_shell_err_too_few_args(argv[1]);
    } else if (argc > 5) {
        return lp5523_shell_err_too_many_args(argv[1]);
    }

    /* set start address for engine */
    engine = parse_ll_bounds(argv[2], 1, 3, &rc);
    if (rc != 0) {
        return lp5523_shell_err_invalid_arg(argv[2]);
    }

    addr = parse_ll_bounds(argv[3], 0, 95, &rc);
    if (rc != 0) {
        return lp5523_shell_err_invalid_arg(argv[3]);
    }

    return lp5523_set_engine_reg(&lp5523_itf,
        LP5523_ENG_PROG_START_ADDR, engine, addr);
}

static int
lp5523_shell_cmd_load(int argc, char **argv)
{
    int rc;
    uint8_t engine;
    uint8_t engines;

    if (argc < 3) {
        return lp5523_shell_err_too_few_args(argv[1]);
    } else if (argc > 3) {
        return lp5523_shell_err_too_many_args(argv[1]);
    }

    /* engine to load mode */
    engine = parse_ll_bounds(argv[2], 1, 3, &rc);
    if (rc != 0) {
        return lp5523_shell_err_invalid_arg(argv[2]);
    }
    engines = 3 << ((3 - engine) << 1);

    /* disable engines */
    rc = lp5523_set_engine_control(&lp5523_itf, LP5523_ENGINE_CNTRL2, engines,
        LP5523_ENGINES_DISABLED);
    if (rc != 0) {
        return rc;
    }

    /* put engines into load program state */
    return lp5523_set_engine_control(&lp5523_itf, LP5523_ENGINE_CNTRL2, engines,
        LP5523_ENGINES_LOAD_PROGRAM);
}

static int
lp5523_shell_cmd_run(int argc, char **argv)
{
    int rc;
    uint8_t engine;

    if (argc < 3) {
        return lp5523_shell_err_too_few_args(argv[1]);
    } else if (argc > 3) {
        return lp5523_shell_err_too_many_args(argv[1]);
    }

    /* engine run */
    engine = parse_ll_bounds(argv[2], 1, 3, &rc);
    if (rc != 0) {
        return lp5523_shell_err_invalid_arg(argv[2]);
    }


    return lp5523_engines_run(&lp5523_itf, 3 << ((3 - engine) << 1));
}

static int
lp5523_shell_cmd_hold(int argc, char **argv)
{
    int rc;
    uint8_t engine;

    if (argc < 3) {
        return lp5523_shell_err_too_few_args(argv[1]);
    } else if (argc > 3) {
        return lp5523_shell_err_too_many_args(argv[1]);
    }

    /* engine hold */
    engine = parse_ll_bounds(argv[2], 1, 3, &rc);
    if (rc != 0) {
        return lp5523_shell_err_invalid_arg(argv[2]);
    }

    return lp5523_engines_hold(&lp5523_itf, 3 << ((3 - engine) << 1));
}

static int
lp5523_shell_cmd_dump(int argc, char **argv)
{
    int rc;
    uint8_t i;
    uint16_t ins;

    if (argc < 2) {
        return lp5523_shell_err_too_few_args(argv[1]);
    } else if (argc > 2) {
        return lp5523_shell_err_too_many_args(argv[1]);
    }

    i = 0;
    while (i < LP5523_MEMORY_SIZE) {
        rc = lp5523_get_instruction(&lp5523_itf, i, &ins);
        if (rc != 0) {
            return -1;
        }
        console_printf("%02x: %04x\r\n", i, ins);
        ++i;
    }

    return 0;
}

static int
lp5523_shell_cmd_reg(int argc, char **argv)
{
    int rc;
    uint8_t addr;
    uint8_t value;

    if (argc < 3) {
        return lp5523_shell_err_too_few_args(argv[1]);
    } else if (argc > 4) {
        return lp5523_shell_err_too_many_args(argv[1]);
    }

    addr = parse_ll_bounds(argv[2], 0, 255, &rc);
    if (rc != 0) {
        return lp5523_shell_err_invalid_arg(argv[2]);
    }

    if (strcmp(argv[2], "set") == 0) {
        if (argc < 4) {
            return lp5523_shell_err_too_few_args(argv[1]);
        }

        value = parse_ll_bounds(argv[3], 0, 255, &rc);
        if (rc != 0) {
            return lp5523_shell_err_invalid_arg(argv[3]);
        }

        return lp5523_set_reg(&lp5523_itf, addr, value);
    }

    if (argc > 3) {
        return lp5523_shell_err_too_many_args(argv[1]);
    }

    rc = lp5523_get_reg(&lp5523_itf, addr, &value);
    if (rc != 0) {
        return rc;
    }

    console_printf("%02x\r\n", value);
    return rc;
}

static int
lp5523_shell_cmd(int argc, char **argv)
{
    if (argc == 1) {
        return lp5523_shell_help();
    }

    if (argc < 1) {
        return lp5523_shell_err_too_few_args(argv[1]);
    }

    /* Reset */
    if (strcmp(argv[1], "reset") == 0) {
        return lp5523_shell_cmd_reset(argc, argv);
    }

    /* Enables the device or an output */
    if (strcmp(argv[1], "enable") == 0) {
        return lp5523_shell_cmd_enable(argc, argv);
    }

    /* Disables the device or an output */
    if (strcmp(argv[1], "disable") == 0) {
        return lp5523_shell_cmd_disable(argc, argv);
    }

    /* Sets the PWM duty cycle of an output LED */
    if (strcmp(argv[1], "pwm") == 0) {
        return lp5523_shell_cmd_pwm(argc, argv);
    }

    /* Writes an instruction */
    if (strcmp(argv[1], "ins") == 0) {
        return lp5523_shell_cmd_ins(argc, argv);
    }

    /* Sets program start */
    if (strcmp(argv[1], "start") == 0) {
        return lp5523_shell_cmd_start(argc, argv);
    }

    /* Sets engines into load program mode */
    if (strcmp(argv[1], "load") == 0) {
        return lp5523_shell_cmd_load(argc, argv);
    }

    /* Runs program */
    if (strcmp(argv[1], "run") == 0) {
        return lp5523_shell_cmd_run(argc, argv);
    }

    /* Holds program */
    if (strcmp(argv[1], "hold") == 0) {
        return lp5523_shell_cmd_hold(argc, argv);
    }

    /* dumps program */
    if (strcmp(argv[1], "dump") == 0) {
        return lp5523_shell_cmd_dump(argc, argv);
    }

    if (strcmp(argv[1], "reg") == 0) {
        return lp5523_shell_cmd_reg(argc, argv);
    }

    return lp5523_shell_err_unknown_arg(argv[1]);
}

int
lp5523_shell_init(void)
{
    int rc;

    rc = shell_cmd_register(&lp5523_shell_cmd_struct);

    return rc;
}

#endif
