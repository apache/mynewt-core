
#include <assert.h>
#include <string.h>
#include <config/config.h>
#include <split/split.h>
#include <split_priv.h>

#define LOADER_IMAGE_SLOT    0
#define SPLIT_IMAGE_SLOT    1
#define SPLIT_TOTAL_IMAGES  2

static char *split_conf_get(int argc, char **argv, char *buf, int max_len);
static int split_conf_set(int argc, char **argv, char *val);
static int split_conf_commit(void);
static int split_conf_export(void (*func)(char *name, char *val), enum conf_export_tgt tgt);

static struct conf_handler split_conf_handler = {
    .ch_name = "split",
    .ch_get = split_conf_get,
    .ch_set = split_conf_set,
    .ch_commit = split_conf_commit,
    .ch_export = split_conf_export
};

int
split_conf_init(void)
{
    int rc;

    rc = conf_register(&split_conf_handler);

    return rc;
}


static char *
split_conf_get(int argc, char **argv, char *buf, int max_len)
{
    boot_split_mode_t split_mode;

    if (argc == 1) {
        if (!strcmp(argv[0], "status")) {
            split_mode = boot_split_mode_get();
            return conf_str_from_value(CONF_INT8, &split_mode, buf, max_len);
        }
    }
    return NULL;
}

static int
split_conf_set(int argc, char **argv, char *val)
{
    boot_split_mode_t split_mode;
    int rc;

    if (argc == 1) {
        if (!strcmp(argv[0], "status")) {
            rc = CONF_VALUE_SET(val, CONF_INT8, split_mode);
            if (rc != 0) {
                return rc;
            }

            rc = boot_split_mode_set(split_mode);
            if (rc != 0) {
                return rc;
            }

            return 0;
        }
    }
    return -1;
}

static int
split_conf_commit(void)
{
    return 0;
}

static int
split_conf_export(void (*func)(char *name, char *val), enum conf_export_tgt tgt)
{
    boot_split_mode_t split_mode;
    char buf[4];

    split_mode = boot_split_mode_get();
    conf_str_from_value(CONF_INT8, &split_mode, buf, sizeof(buf));
    func("split/status", buf);
    return 0;
}

int
split_write_split(boot_split_mode_t split_mode)
{
    char str[CONF_STR_FROM_BYTES_LEN(sizeof(boot_split_mode_t))];
    int rc;

    rc = boot_split_mode_set(split_mode);
    if (rc != 0) {
        return rc;
    }

    if (!conf_str_from_value(CONF_INT8, &split_mode, str, sizeof(str))) {
        return -1;
    }
    return conf_save_one("split/status", str);
}
