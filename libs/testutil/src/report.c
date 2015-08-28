#include <stdio.h>
#include "testutil/testutil.h"
#include "testutil_priv.h"

static char tu_report_buf[1024];

int
tu_report_mkdir_results(void)
{
    int rc;

    if (tu_config.tc_base_path == NULL) {
        return 0;
    }

    rc = snprintf(tu_report_buf, sizeof tu_report_buf,
                  "%s/results", tu_config.tc_base_path);
    if (rc >= sizeof tu_report_buf) {
        return -1;
    }
    return tu_io_mkdir("results");
}

int
tu_report_mkdir_suite(void)
{
    int rc;

    if (tu_config.tc_base_path == NULL) {
        return 0;
    }

    rc = snprintf(tu_report_buf, sizeof tu_report_buf,
                  "%s/results/%s", tu_config.tc_base_path,
                  tu_suite_name);
    if (rc >= sizeof tu_report_buf) {
        return -1;
    }

    rc = tu_io_mkdir(tu_report_buf);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int
tu_report_mkdir_case(void)
{
    int rc;

    if (tu_config.tc_base_path == NULL) {
        return 0;
    }

    rc = snprintf(tu_report_buf, sizeof tu_report_buf,
                  "%s/results/%s/%s", tu_config.tc_base_path,
                  tu_suite_name, tu_case_name);
    if (rc >= sizeof tu_report_buf) {
        return -1;
    }

    rc = tu_io_mkdir(tu_report_buf);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int
tu_report_write_file(const char *filename, const uint8_t *data,
                           size_t data_len)
{
    int rc;

    if (tu_config.tc_base_path == NULL) {
        return 0;
    }

    rc = snprintf(tu_report_buf, sizeof tu_report_buf,
                  "%s/results/%s/%s/%s", tu_config.tc_base_path,
                  tu_suite_name, tu_case_name, filename);
    if (rc >= sizeof tu_report_buf) {
        return -1;
    }

    rc = tu_io_write(tu_report_buf, data, data_len);
    if (rc != 0) {
        return rc;
    }

    return 0;
}
