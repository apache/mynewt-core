/*
 * Copyright 2022 Jesus Ipanienko
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <os/mynewt.h>
#include <console/console.h>
#include <shell/shell.h>
#include <ds3231/ds3231.h>
#include <parse/parse.h>

#if MYNEWT_VAL(DS3231_CLI)

static int
ds3231_shell_err_unknown_arg(char *cmd_name)
{
    console_printf("Error: unknown argument \"%s\"\n",
                   cmd_name);
    return EINVAL;
}

static int
ds3231_shell_help(void)
{
    console_printf("ds3231 cmd [flags...]\n");
    console_printf("cmd:\n");
    console_printf("\tdate [date-time]\n");
    console_printf("\ttemp\n");
    console_printf("\tr addr\n");
    console_printf("\tw addr value\n");

    return 0;
}

static int
ds3231_shell_cmd_date(int argc, char **argv)
{
    struct ds3231_dev *ds3231 = (struct ds3231_dev *)os_dev_open(MYNEWT_VAL(DS3231_OS_DEV_NAME), 100, NULL);
    struct clocktime ct;
    struct os_timeval tv;
    struct os_timezone tz = {0};
    int rc;

    if (ds3231 == NULL) {
        console_printf("Can't open %s device\n", MYNEWT_VAL(DS3231_OS_DEV_NAME));
        return 0;
    }

    if (argc > 2) {
        rc = datetime_parse(argv[2], &tv, &tz);
        if (rc) {
            console_printf("Invalid time format\n");
        } else {
            timeval_to_clocktime(&tv, &tz, &ct);
            rc = ds3231_write_time(ds3231, &ct);
            if (rc) {
                console_printf("write time failed %d\n", rc);
            }
        }

    } else {
        rc = ds3231_read_time(ds3231, &ct);
        if (rc == 0) {
            console_printf("%04d-%02d-%02dT%02d:%02d:%02d\n",
                           ct.year, ct.mon, ct.day, ct.hour, ct.min, ct.sec);
        } else {
            console_printf("read time failed %d\n", rc);
        }
    }

    os_dev_close((struct os_dev *)ds3231);

    return 0;
}

static int
ds3231_shell_cmd_read(int argc, char **argv)
{
    struct ds3231_dev *ds3231 = (struct ds3231_dev *)os_dev_open(MYNEWT_VAL(DS3231_OS_DEV_NAME), 100, NULL);
    int rc;
    uint32_t rd_addr;
    uint8_t val;

    if (ds3231 == NULL) {
        console_printf("Can't open %s device\n", MYNEWT_VAL(DS3231_OS_DEV_NAME));
        return 0;
    }

    if (argc > 2) {
        rd_addr = (uint32_t)parse_ull(argv[2], &rc);
        rc = ds3231_read_regs(ds3231, rd_addr, &val, 1);
        if (rc == 0) {
            console_printf(" = 0x%02X\n", val);
        } else {
            console_printf("read failed %d\n", rc);
        }
    }

    os_dev_close((struct os_dev *)ds3231);

    return 0;
}

static int
ds3231_shell_cmd_temp(int argc, char **argv)
{
    struct ds3231_dev *ds3231 = (struct ds3231_dev *)os_dev_open(MYNEWT_VAL(DS3231_OS_DEV_NAME), 100, NULL);
    int rc;
    int16_t temp;

    if (ds3231 == NULL) {
        console_printf("Can't open %s device\n", MYNEWT_VAL(DS3231_OS_DEV_NAME));
        return 0;
    }

    rc = ds3231_read_temp(ds3231, &temp);
    if (rc == 0) {
        console_printf("temperature = %d.%02d\n", temp / 100, abs(temp) % 100);
    } else {
        console_printf("read failed %d\n", rc);
    }

    os_dev_close((struct os_dev *)ds3231);

    return 0;
}

static int
ds3231_shell_cmd_write(int argc, char **argv)
{
    struct ds3231_dev *ds3231 = (struct ds3231_dev *)os_dev_open(MYNEWT_VAL(DS3231_OS_DEV_NAME), 100, NULL);
    int rc;
    uint32_t addr;
    uint8_t val;

    if (ds3231 == NULL) {
        console_printf("Can't open %s device\n", MYNEWT_VAL(DS3231_OS_DEV_NAME));
        return 0;
    }

    if (argc > 3) {
        addr = (uint32_t)parse_ull(argv[2], &rc);
        val = (uint8_t)parse_ull(argv[3], &rc);
        rc = ds3231_write_regs(ds3231, addr, &val, 1);
        if (rc) {
            console_printf("write failed %d\n", rc);
        }
    }

    os_dev_close((struct os_dev *)ds3231);

    return 0;
}

static int
ds3231_shell_cmd_start(int argc, char **argv)
{
    struct ds3231_dev *ds3231 = (struct ds3231_dev *)os_dev_open(MYNEWT_VAL(DS3231_OS_DEV_NAME), 100, NULL);
    int rc;
    uint8_t reg;

    if (ds3231 == NULL) {
        console_printf("Can't open %s device\n", MYNEWT_VAL(DS3231_OS_DEV_NAME));
        return 0;
    }

    rc = ds3231_read_regs(ds3231, DS3231_CONTROL_STATUS_ADDR, &reg, 1);
    if (rc == 0) {
        reg &= 0x7F;
        ds3231_write_regs(ds3231, DS3231_CONTROL_STATUS_ADDR, &reg, 1);
    }

    os_dev_close((struct os_dev *)ds3231);

    return 0;
}

static int
ds3231_shell_cmd(int argc, char **argv)
{
    if (argc == 1) {
        return ds3231_shell_help();
    }

    /* Read command (get a new data sample) */
    if (argc > 1 && strcmp(argv[1], "r") == 0) {
        return ds3231_shell_cmd_read(argc, argv);
    } else if (argc > 1 && strcmp(argv[1], "w") == 0) {
        return ds3231_shell_cmd_write(argc, argv);
    } else if (argc > 1 && strcmp(argv[1], "date") == 0) {
        return ds3231_shell_cmd_date(argc, argv);
    } else if (argc > 1 && strcmp(argv[1], "start") == 0) {
        return ds3231_shell_cmd_start(argc, argv);
    } else if (argc > 1 && strcmp(argv[1], "temp") == 0) {
        return ds3231_shell_cmd_temp(argc, argv);
    }

    return ds3231_shell_err_unknown_arg(argv[1]);
}

MAKE_SHELL_CMD(ds3231, ds3231_shell_cmd, NULL)

#endif
