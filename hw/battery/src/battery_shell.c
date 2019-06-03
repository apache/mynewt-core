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

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <console/console.h>
#include <shell/shell.h>
#include <bsp/bsp.h>
#include <parse/parse.h>
#include <battery/battery.h>
#include <battery/battery_prop.h>

static int bat_compat_cmd(int argc, char **argv);

#if MYNEWT_VAL(SHELL_CMD_HELP)
#define HELP(a) &(a)

static const struct shell_param bat_read_params[] =
{
        { "all" },
        { NULL }
};

static const struct shell_param bat_monitor_params[] =
{
        { NULL }
};

static const struct shell_cmd_help bat_read_help =
{
        .summary = "read battery properties",
        .usage = "read <prop>",
        .params = bat_read_params,
};

static const struct shell_cmd_help bat_write_help =
{
        .summary = "write battery properties",
        .usage = "read <prop> <value>",
        .params = NULL,
};

static const struct shell_cmd_help bat_list_help =
{
        .summary = "list battery properties",
        .usage = "list",
        .params = NULL,
};

static const struct shell_cmd_help bat_poll_rate_help =
{
        .summary = "set battery polling rate",
        .usage = "pollrate <time_in_s>",
        .params = NULL,
};

static const struct shell_cmd_help bat_monitor_help =
{
        .summary = "start battery property monitoring",
        .usage = "monitor <prop> [off]",
        .params = bat_monitor_params,
};

#else
#define HELP(a) NULL
#endif

static const struct shell_cmd bat_cli_cmd =
{
    .sc_cmd = "bat",
    .sc_cmd_func = bat_compat_cmd,
};

/**
 * cmd bat help
 *
 * Help for the bat command.
 *
 */
static void cmd_bat_help(void)
{
    console_printf("Usage: bat <cmd> [options]\n");
    console_printf("Available bat commands:\n");
    console_printf("  pollrate <time_in_s>\n");
    console_printf("  monitor [<prop>] [off]\n");
    console_printf("  list\n");
    console_printf("  read [<prop>] | all\n");
    console_printf("  write <prop> <value>\n");

    console_printf("Examples:\n");
    console_printf("  list\n");
    console_printf("  monitor VoltageADC\n");
    console_printf("  monitor off\n");
    console_printf("  read Voltage\n");
    console_printf("  read all\n");
    console_printf("  write VoltageLoAlarmSet\n");
}

static const char *bat_status[] = {
        "???",
        "charging",
        "discharging",
        "connected not charging",
        "battery full",
};

static const char *bat_level[] = {
        "???",
        "battery level critical",
        "battery level low",
        "battery level normal",
        "battery level high",
        "battery level full",
};

static void print_property(const struct battery_property *prop)
{
    char name[20];

    battery_prop_get_name(prop, name, 20);

    switch (prop->bp_type) {
    case BATTERY_PROP_VOLTAGE_NOW:
    case BATTERY_PROP_VOLTAGE_AVG:
    case BATTERY_PROP_VOLTAGE_MAX:
    case BATTERY_PROP_VOLTAGE_MAX_DESIGN:
    case BATTERY_PROP_VOLTAGE_MIN:
    case BATTERY_PROP_VOLTAGE_MIN_DESIGN:
        console_printf(" %s %ld mV\n", name, prop->bp_value.bpv_voltage);
        break;
    case BATTERY_PROP_TEMP_NOW:
    case BATTERY_PROP_TEMP_AMBIENT:
        console_printf(" %s %f deg C\n", name, prop->bp_value.bpv_temperature);
        break;
    case BATTERY_PROP_CURRENT_NOW:
    case BATTERY_PROP_CURRENT_MAX:
    case BATTERY_PROP_CURRENT_AVG:
        console_printf(" %s %ld mA\n", name, prop->bp_value.bpv_current);
        break;
    case BATTERY_PROP_TIME_TO_EMPTY_NOW:
    case BATTERY_PROP_TIME_TO_FULL_NOW:
        console_printf(" %s %ld s\n", name, prop->bp_value.bpv_time_in_s);
        break;
    case BATTERY_PROP_SOC:
    case BATTERY_PROP_SOH:
        console_printf(" %s %d %%\n", name, prop->bp_value.bpv_u8);
        break;
    case BATTERY_PROP_STATUS:
        console_printf(" %s %s\n", name,
                bat_status[prop->bp_value.bpv_status]);
        break;
    case BATTERY_PROP_CAPACITY:
    case BATTERY_PROP_CAPACITY_FULL:
        console_printf(" %s %lu mAh\n", name, prop->bp_value.bpv_capacity);
        break;
    case BATTERY_PROP_CAPACITY_LEVEL:
        console_printf(" %s %s\n", name,
                bat_level[prop->bp_value.bpv_capacity_level]);
        break;
    case BATTERY_PROP_CYCLE_COUNT:
        console_printf(" %s %d\n", name, prop->bp_value.bpv_cycle_count);
        break;
    default:
        break;
    }
}

static struct os_dev *
battery_shell_open_dev(void)
{
    struct os_dev *bat;

    bat = os_dev_open("battery_0", 0, NULL);
    if (bat == NULL) {
        console_printf("Failed to open battery device\n");
    }

    return bat;
}

static int cmd_bat_read(int argc, char **argv)
{
    int rc;
    int maxp;
    int i;
    struct battery_property *prop;
    struct os_dev *bat;

    bat = NULL;

    if (argc < 2) {
        console_printf("Invalid number of arguments, use read <prop>\n");
        rc = SYS_EINVAL;
        goto err;
    }

    bat = battery_shell_open_dev();
    if (bat == NULL) {
        rc = SYS_ENODEV;
        goto err;
    }

    if (argc == 2 && strcmp("all", argv[1]) == 0) {
        maxp = battery_get_property_count(bat, NULL);
        for (i = 0; i < maxp; ++i) {
            prop = battery_enum_property(bat, NULL, i);
            battery_prop_get_value(prop);
            if (!prop->bp_valid) {
                console_printf("Error reading property\n");
                rc = SYS_EIO;
                goto err;
            }
            print_property(prop);
        }
    } else {
        for (i = 1; i < argc; ++i) {
            prop = battery_find_property_by_name(bat, argv[i]);
            if (prop == NULL) {
                console_printf("Invalid property name %s\n", argv[i]);
                rc = SYS_EINVAL;
                goto err;
            }
            battery_prop_get_value(prop);
            if (!prop->bp_valid) {
                console_printf("Error reading property\n");
                rc = SYS_EIO;
                goto err;
            }
            print_property(prop);
        }
    }

    rc = 0;

err:
    if (bat != NULL) {
        os_dev_close(bat);
    }
    return rc;
}

static int
get_min_max(const struct battery_property *prop, long long *min, long long *max)
{
    int rc = 0;

    if (prop->bp_type == BATTERY_PROP_VOLTAGE_NOW) {
        *min = 0;
        *max = 10000;
    } else if (prop->bp_type == BATTERY_PROP_TEMP_NOW) {
        *min = -128;
        *max = 127;
    } else {
        rc = -1;
    }
    return rc;
}

static int
cmd_bat_write(int argc, char ** argv)
{
    int rc;
    long long min;
    long long max;
    struct battery_property *prop;
    struct os_dev *bat;
    long long int val;

    bat = NULL;

    if (argc < 3) {
        console_printf("Invalid number of arguments, use write <prop> <value>\n");
        rc = SYS_EINVAL;
        goto err;
    }

    bat = battery_shell_open_dev();
    if (bat == NULL) {
        rc = SYS_ENODEV;
        goto err;
    }

    prop = battery_find_property_by_name(bat, argv[1]);
    if (prop == NULL) {
        console_printf("Invalid property name %s\n", argv[1]);
        rc = SYS_EINVAL;
        goto err;
    }

    if (get_min_max(prop, &min, &max)) {
        console_printf("Property %s can not be set\n", argv[1]);
        rc = SYS_EIO;
        goto err;
    }

    val = parse_ll_bounds(argv[2], min, max, &rc);
    if (rc) {
        console_printf("Property value not in range <%lld, %lld>\n", min, max);
        rc = 0;
        goto err;
    }

    if (prop->bp_type == BATTERY_PROP_VOLTAGE_NOW &&
            (prop->bp_flags & BATTERY_PROPERTY_FLAGS_ALARM_THREASH) != 0) {
        rc = battery_prop_set_value_uint32(prop, (uint32_t)val);
    } else if (prop->bp_type == BATTERY_PROP_TEMP_NOW &&
            (prop->bp_flags & BATTERY_PROPERTY_FLAGS_ALARM_THREASH) != 0) {
        rc = battery_prop_set_value_float(prop, val);
    } else {
        console_printf("Property %s can't be written!\n", argv[1]);
        rc = SYS_EINVAL;
        goto err;
    }
    if (!prop->bp_valid) {
        console_printf("Error writing property!\n");
        rc = SYS_EIO;
        goto err;
    }

    rc = 0;

err:
    if (bat != NULL) {
        os_dev_close(bat);
    }
    return rc;
}

static int cmd_bat_list(int argc, char **argv)
{
    int i;
    int max;
    struct battery_property *prop;
    struct os_dev *bat;
    char name[20];
    int rc;

    bat = NULL;

    bat = battery_shell_open_dev();
    if (bat == NULL) {
        rc = SYS_ENODEV;
        goto err;
    }

    max = battery_get_property_count(bat, NULL);
    for (i = 0; i < max; ++i) {
        prop = battery_enum_property(bat, NULL, i);
        if (prop) {
            console_printf(" %s\n", battery_prop_get_name(prop, name, 20));
        }
    }

    rc = 0;

err:
    if (bat != NULL) {
        os_dev_close(bat);
    }
    return rc;
}

static int cmd_bat_poll_rate(int argc, char **argv)
{
    int rc;
    uint32_t rate_in_s;
    struct os_dev *bat;

    bat = NULL;

    if (argc < 2) {
        console_printf("Missing poll rate argument\n");
        rc = SYS_EINVAL;
        goto err;
    }

    bat = battery_shell_open_dev();
    if (bat == NULL) {
        rc = SYS_ENODEV;
        goto err;
    }

    rate_in_s = (uint32_t)parse_ull_bounds(argv[1], 1, 255, &rc);
    if (rc) {
        console_printf("Invalid poll rate, use 1..255\n");
        goto err;
    }

    rc = battery_set_poll_rate_ms(bat, rate_in_s * 1000);

err:
    if (bat != NULL) {
        os_dev_close(bat);
    }
    return rc;
}

static int bat_property(struct battery_prop_listener *listener,
        const struct battery_property *prop)
{
    print_property(prop);
    return 0;
}

static struct battery_prop_listener listener = {
        .bpl_prop_changed = bat_property,
        .bpl_prop_read = bat_property,
};

static int cmd_bat_monitor(int argc, char **argv)
{
    int rc;
    struct battery_property *prop;
    struct os_dev *bat;

    bat = NULL;

    if (argc < 2) {
        console_printf("Invalid number of arguments, use monitor <prop_nam>\n");
        rc = SYS_EINVAL;
        goto err;
    }

    bat = battery_shell_open_dev();
    if (bat == NULL) {
        rc = SYS_ENODEV;
        goto err;
    }

    prop = battery_find_property_by_name(bat, argv[1]);
    if (prop == NULL) {
        if (strcmp(argv[1], "off") == 0) {
            rc = battery_prop_poll_unsubscribe(&listener, NULL);
            goto err;
        }
        console_printf("Invalid property name\n");
        rc = SYS_EINVAL;
        goto err;
    }

    if (argc == 3) {
        if (strcmp(argv[2], "off") == 0) {
            rc = battery_prop_poll_unsubscribe(&listener, prop);
            goto err;
        } else if (strcmp(argv[2], "on") != 0) {
            console_printf("Invalid parameter %s\n", argv[2]);
            rc = SYS_EINVAL;
            goto err;
        }
    }
    rc = battery_prop_poll_subscribe(&listener, prop);

err:
    if (bat != NULL) {
        os_dev_close(bat);
    }
    return rc;
}

static const struct shell_cmd bat_cli_commands[] =
{
    SHELL_CMD("read", cmd_bat_read, &bat_read_help),
    SHELL_CMD("write", cmd_bat_write, &bat_write_help),
    SHELL_CMD("list", cmd_bat_list, &bat_list_help),
    SHELL_CMD("pollrate", cmd_bat_poll_rate, &bat_poll_rate_help),
    SHELL_CMD("monitor", cmd_bat_monitor, &bat_monitor_help),
    { 0 },
};

/**
 * bat cmd
 *
 * Main processing function for K4 cli bat command
 *
 * @param argc Number of arguments
 * @param argv Argument list
 *
 * @return int 0: success, -1 error
 */
static int bat_compat_cmd(int argc, char **argv)
{
    int rc;
    int i;

    if (argc < 2)
    {
        rc = SYS_EINVAL;
    }
    else
    {
        for (i = 0; bat_cli_commands[i].sc_cmd; ++i)
        {
            if (strcmp(bat_cli_commands[i].sc_cmd, argv[1]) == 0)
            {
                rc = bat_cli_commands[i].sc_cmd_func(argc - 1, argv + 1);
                break;
            }
        }
        /* No command found */
        if (bat_cli_commands[i].sc_cmd == NULL)
        {
            console_printf("Invalid command.\n");
            rc = -1;
        }
    }

    /* Print help in case of error */
    if (rc)
    {
        cmd_bat_help();
    }

    return rc;
}

void
battery_shell_register(void)
{
    int rc;

    rc = shell_register("bat", bat_cli_commands);
    rc = shell_cmd_register(&bat_cli_cmd);
    SYSINIT_PANIC_ASSERT_MSG(rc == 0, "Failed to register battery shell");
}
