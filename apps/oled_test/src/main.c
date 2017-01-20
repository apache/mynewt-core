/**
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
#include <os/os.h>
#include <bsp/bsp.h>
#include <hal/hal_gpio.h>
#include <hal/hal_flash.h>
#include <console/console.h>
#include <shell/shell.h>
#include <config/config.h>
#include <hal/flash_map.h>
#include <hal/hal_system.h>
#ifdef NFFS_PRESENT
#include <fs/fs.h>
#include <nffs/nffs.h>
#include <config/config_file.h>
#elif FCB_PRESENT
#include <fcb/fcb.h>
#include <config/config_fcb.h>
#else
#error "Need NFFS or FCB for config storage"
#endif
#include <newtmgr/newtmgr.h>
#include <bootutil/image.h>
#include <bootutil/bootutil_misc.h>
#include <imgmgr/imgmgr.h>
#include <assert.h>
#include <string.h>
#include <json/json.h>
#include <flash_test/flash_test.h>
#include <reboot/log_reboot.h>
#include <os/os_time.h>
#include <id/id.h>

#include <mpu_3050/mpu_3050.h>

#ifdef ARCH_sim
#include <mcu/mcu_sim.h>
#endif

/* Init all tasks */
volatile int tasks_initialized;
int init_tasks(void);

/* Task 1 */
#define TASK1_PRIO (8)
#define TASK1_STACK_SIZE    OS_STACK_ALIGN(192)
struct os_task task1;
os_stack_t stack1[TASK1_STACK_SIZE];
static volatile int g_task1_loops;

/* Task 2 */
#define TASK2_PRIO (9)
#define TASK2_STACK_SIZE    OS_STACK_ALIGN(128)
struct os_task task2;
os_stack_t stack2[TASK2_STACK_SIZE];

#define SHELL_TASK_PRIO (3)
#define SHELL_MAX_INPUT_LEN     (256)
#define SHELL_TASK_STACK_SIZE (OS_STACK_ALIGN(384))
os_stack_t shell_stack[SHELL_TASK_STACK_SIZE];

#define NEWTMGR_TASK_PRIO (4)
#define NEWTMGR_TASK_STACK_SIZE (OS_STACK_ALIGN(896))
os_stack_t newtmgr_stack[NEWTMGR_TASK_STACK_SIZE];

static volatile int g_task2_loops;

/* Global test semaphore */
struct os_sem g_test_sem;

/* For LED toggling */
int g_led_pin;

#ifdef NFFS_PRESENT
/* configuration file */
#define MY_CONFIG_DIR  "/cfg"
#define MY_CONFIG_FILE "/cfg/run"
#define MY_CONFIG_MAX_LINES  32

static struct conf_file my_conf = {
    .cf_name = MY_CONFIG_FILE,
    .cf_maxlines = MY_CONFIG_MAX_LINES
};
#elif FCB_PRESENT
struct flash_area conf_fcb_area[NFFS_AREA_MAX + 1];

static struct conf_fcb my_conf = {
    .cf_fcb.f_magic = 0xc09f6e5e,
    .cf_fcb.f_sectors = conf_fcb_area
};
#endif

#define DEFAULT_MBUF_MPOOL_BUF_LEN (256)
#define DEFAULT_MBUF_MPOOL_NBUFS (10)

uint8_t default_mbuf_mpool_data[DEFAULT_MBUF_MPOOL_BUF_LEN *
    DEFAULT_MBUF_MPOOL_NBUFS];

struct os_mbuf_pool default_mbuf_pool;
struct os_mempool default_mbuf_mpool;

static char *test_conf_get(int argc, char **argv, char *val, int max_len);
static int test_conf_set(int argc, char **argv, char *val);
static int test_conf_commit(void);
static int test_conf_export(void (*export_func)(char *name, char *val),
  enum conf_export_tgt tgt);

static struct conf_handler test_conf_handler = {
    .ch_name = "test",
    .ch_get = test_conf_get,
    .ch_set = test_conf_set,
    .ch_commit = test_conf_commit,
    .ch_export = test_conf_export
};

static uint8_t test8;
static uint8_t test8_shadow;
static char test_str[32];

static char *
test_conf_get(int argc, char **argv, char *buf, int max_len)
{
    if (argc == 1) {
        if (!strcmp(argv[0], "8")) {
            return conf_str_from_value(CONF_INT8, &test8, buf, max_len);
        } else if (!strcmp(argv[0], "str")) {
            return test_str;
        }
    }
    return NULL;
}

static int
test_conf_set(int argc, char **argv, char *val)
{
    if (argc == 1) {
        if (!strcmp(argv[0], "8")) {
            return CONF_VALUE_SET(val, CONF_INT8, test8_shadow);
        } else if (!strcmp(argv[0], "str")) {
            return CONF_VALUE_SET(val, CONF_STRING, test_str);
        }
    }
    return OS_ENOENT;
}

static int
test_conf_commit(void)
{
    test8 = test8_shadow;

    return 0;
}

static int
test_conf_export(void (*func)(char *name, char *val), enum conf_export_tgt tgt)
{
    char buf[4];

    conf_str_from_value(CONF_INT8, &test8, buf, sizeof(buf));
    func("test/8", buf);
    func("test/str", test_str);
    return 0;
}

void
task1_handler(void *arg)
{
    struct os_task *t;

    /* Set the led pin for the E407 devboard */
    g_led_pin = LED_BLINK_PIN;
    hal_gpio_init_out(g_led_pin, 1);

    while (1) {
        t = os_sched_get_current_task();
        assert(t->t_func == task1_handler);

        ++g_task1_loops;

        /* Wait one second */
        os_time_delay(1000);

        /* Toggle the LED */
        hal_gpio_toggle(g_led_pin);

        /* Release semaphore to task 2 */
        os_sem_release(&g_test_sem);
    }
}

#if 0
#define SSD1306_DEV ARDUINO_ZERO_I2C

static int
ssd1306_cli(int argc, char **argv)
{
    int rc;

    if (argc < 1) {
        console_printf("too few args\n");
        return 0;
    }
    if (!strcmp(argv[1], "init")) {
        rc = ssd1306_init(SSD1306_DEV);
        console_printf("ssd1306_init() = %d\n", rc);
        if (rc) {
            return 0;
        }
        rc = ssd1306_enable();
        console_printf("ssd1306_enable() = %d\n", rc);
    } else if (!strcmp(argv[1], "cls")) {
        rc = ssd1306_fill(0);
        console_printf("ssd1306_fill(0) = %d\n", rc);
    } else if (!strcmp(argv[1], "fill")) {
        rc = ssd1306_fill(255);
        console_printf("ssd1306_fill(255) = %d\n", rc);
    }
    return 0;
}

struct shell_cmd ssd1306_cli_cmd = {
    .sc_cmd = "oled",
    .sc_cmd_func = ssd1306_cli
};
#endif

#include <hal/hal_i2c.h>
#define GYRO_DEV 5

static int
mpu3050_cli(int argc, char **argv)
{
    int rc;
    static struct mpu3050 gyro;
    uint16_t x, y, z;
    uint8_t reg, val;

    if (argc < 2) {
        console_printf("too few args\n");
        return 0;
    }
    if (!strcmp(argv[1], "init")) {
        rc = mpu3050_init(&gyro, GYRO_DEV, MPU3050_I2C_ADDR);
        console_printf("mpu3050_init() = %d\n", rc);
        if (rc) {
            return 0;
        }
    } else if (!strcmp(argv[1], "raw")) {
        rc = mpu3050_read_xyz(&gyro, &x, &y, &z);
        console_printf("mpu3050_read_raw() = %d\n", rc);
        if (rc == 0) {
            console_printf("x=%d y=%d x=%d\n", x, y, z);
        }
    } else if (!strcmp(argv[1], "reg")) {
        if (argc < 3) {
            return 0;
        }

        reg = strtoul(argv[2], 0, 0);
        rc = mpu3050_read_reg(&gyro, reg, &val);
        console_printf("mpu3050_read_reg(%d) = %d\n", reg, rc);
        if (rc == 0) {
            console_printf("val=%x\n", val);
        }
    } else if (!strcmp(argv[1], "probe")) {
        uint32_t now, then;
        if (argc < 3) {
            console_printf("more args needed\n");
            return 0;
        }

        val = strtoul(argv[2], 0, 0);
        then = os_time_get();
        rc = hal_i2c_master_probe(0, 0x40, val);
        now = os_time_get();
        console_printf("probe=%d %ld->%ld\n", rc, then, now);
    } else {
        console_printf("unknown cmd %s\n", argv[1]);
    }
    return 0;
}

struct shell_cmd mpu3050_cli_cmd = {
    .sc_cmd = "gyro",
    .sc_cmd_func = mpu3050_cli
};

/**
 * init_tasks
 *
 * Called by main.c after os_init(). This function performs initializations
 * that are required before tasks are running.
 *
 * @return int 0 success; error otherwise.
 */
int
init_tasks(void)
{
    /* Initialize global test semaphore */
    os_sem_init(&g_test_sem, 0);

    os_task_init(&task1, "task1", task1_handler, NULL,
            TASK1_PRIO, OS_WAIT_FOREVER, stack1, TASK1_STACK_SIZE);

    tasks_initialized = 1;
    return 0;
}

#ifdef NFFS_PRESENT
static void
setup_for_nffs(void)
{
    /* NFFS_AREA_MAX is defined in the BSP-specified bsp.h header file. */
    struct nffs_area_desc descs[NFFS_AREA_MAX + 1];
    int cnt;
    int rc;

    /* Initialize nffs's internal state. */
    rc = nffs_init();
    assert(rc == 0);

    /* Convert the set of flash blocks we intend to use for nffs into an array
     * of nffs area descriptors.
     */
    cnt = NFFS_AREA_MAX;
    rc = flash_area_to_nffs_desc(FLASH_AREA_NFFS, &cnt, descs);
    assert(rc == 0);

    /* Attempt to restore an existing nffs file system from flash. */
    if (nffs_detect(descs) == FS_ECORRUPT) {
        /* No valid nffs instance detected; format a new one. */
        rc = nffs_format(descs);
        assert(rc == 0);
    }

    fs_mkdir(MY_CONFIG_DIR);
    rc = conf_file_src(&my_conf);
    assert(rc == 0);
    rc = conf_file_dst(&my_conf);
    assert(rc == 0);
}

#elif FCB_PRESENT

static void
setup_for_fcb(void)
{
    int cnt;
    int rc;

    rc = flash_area_to_sectors(FLASH_AREA_NFFS, &cnt, NULL);
    assert(rc == 0);
    assert(cnt <= sizeof(conf_fcb_area) / sizeof(conf_fcb_area[0]));
    flash_area_to_sectors(FLASH_AREA_NFFS, &cnt, conf_fcb_area);

    my_conf.cf_fcb.f_sector_cnt = cnt;

    rc = conf_fcb_src(&my_conf);
    if (rc) {
        for (cnt = 0; cnt < my_conf.cf_fcb.f_sector_cnt; cnt++) {
            flash_area_erase(&conf_fcb_area[cnt], 0,
              conf_fcb_area[cnt].fa_size);
        }
        rc = conf_fcb_src(&my_conf);
    }
    assert(rc == 0);
    rc = conf_fcb_dst(&my_conf);
    assert(rc == 0);
}

#endif

/**
 * main
 *
 * The main function for the project. This function initializes the os, calls
 * init_tasks to initialize tasks (and possibly other objects), then starts the
 * OS. We should not return from os start.
 *
 * @return int NOTE: this function should never return!
 */
int
main(int argc, char **argv)
{
    int rc;
    struct image_version ver;

#ifdef ARCH_sim
    mcu_sim_parse_args(argc, argv);
#endif

    conf_init();
    rc = conf_register(&test_conf_handler);
    assert(rc == 0);

    os_init();

    rc = os_mempool_init(&default_mbuf_mpool, DEFAULT_MBUF_MPOOL_NBUFS,
            DEFAULT_MBUF_MPOOL_BUF_LEN, default_mbuf_mpool_data,
            "default_mbuf_data");
    assert(rc == 0);

    rc = os_mbuf_pool_init(&default_mbuf_pool, &default_mbuf_mpool,
            DEFAULT_MBUF_MPOOL_BUF_LEN, DEFAULT_MBUF_MPOOL_NBUFS);
    assert(rc == 0);

    rc = os_msys_register(&default_mbuf_pool);
    assert(rc == 0);

    rc = hal_flash_init();
    assert(rc == 0);

#ifdef NFFS_PRESENT
    setup_for_nffs();
#elif FCB_PRESENT
    setup_for_fcb();
#endif

    id_init();

    shell_task_init(SHELL_TASK_PRIO, shell_stack, SHELL_TASK_STACK_SIZE,
                    SHELL_MAX_INPUT_LEN);

    nmgr_task_init(NEWTMGR_TASK_PRIO, newtmgr_stack, NEWTMGR_TASK_STACK_SIZE);
    imgmgr_module_init();

    if (imgr_my_version(&ver) == 0) {
        console_printf("\nSlinky %u.%u.%u.%u\n",
          ver.iv_major, ver.iv_minor, ver.iv_revision,
          (unsigned int)ver.iv_build_num);
    } else {
        console_printf("\nSlinky\n");
    }

#if 0
    shell_cmd_register(&ssd1306_cli_cmd);
#endif
    shell_cmd_register(&mpu3050_cli_cmd);

    conf_load();

    rc = init_tasks();

    os_start();

    /* os start should never return. If it does, this should be an error */
    assert(0);

    return rc;
}

