
#include <assert.h>
#include <string.h>
#include <config/config.h>
#include <split/split.h>
#include <split/split_priv.h>

#define LOADER_IMAGE_SLOT    0
#define SPLIT_IMAGE_SLOT    1
#define SPLIT_TOTAL_IMAGES  2

#define SPLIT_NO_BOOT  (1)


static char *split_conf_get(int argc, char **argv, char *buf, int max_len);
static int split_conf_set(int argc, char **argv, char *val);
static int split_conf_commit(void);
static int split_conf_export(void (*func)(char *name, char *val), enum conf_export_tgt tgt);

static struct conf_handler split_conf_handler = {
    .ch_name = "split",
    .ch_get =split_conf_get,
    .ch_set = split_conf_set,
    .ch_commit = split_conf_commit,
    .ch_export = split_conf_export
};

int
split_conf_init(void) {
    int rc;
    rc = conf_register(&split_conf_handler);
    return rc;
}

static int8_t split_status;

static char *
split_conf_get(int argc, char **argv, char *buf, int max_len)
{
    if (argc == 1) {
        if (!strcmp(argv[0], "status")) {
            return conf_str_from_value(CONF_INT8, &split_status, buf, max_len);
        }
    }
    return NULL;
}

static int
split_conf_set(int argc, char **argv, char *val)
{
    if (argc == 1) {
        if (!strcmp(argv[0], "status")) {
            return CONF_VALUE_SET(val, CONF_INT8, split_status);
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
    char buf[4];

    conf_str_from_value(CONF_INT8, &split_status, buf, sizeof(buf));
    func("split/status", buf);
    return 0;
}

int
split_read_split(splitMode_t *split) {
    *split = (splitMode_t) split_status;
    return 0;
}

int
split_write_split(splitMode_t split)
{
    char str[CONF_STR_FROM_BYTES_LEN(sizeof(splitMode_t))];

    split_status = (int8_t) split;
    if (!conf_str_from_value(CONF_INT8, &split_status, str, sizeof(str))) {
        return -1;
    }
    return conf_save_one("split/status", str);
}

