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

#include <stdio.h>
#include <string.h>

#include "syscfg/syscfg.h"
#include "os/os.h"
#include "testutil/testutil.h"

#include "fcb/fcb.h"
#include "fcb/../../src/fcb_priv.h"

static struct fcb test_fcb;

static struct flash_area test_fcb_area[] = {
    [0] = {
        .fa_device_id = 0,
        .fa_off = 0,
        .fa_size = 0x4000, /* 16K */
    },
    [1] = {
        .fa_device_id = 0,
        .fa_off = 0x4000,
        .fa_size = 0x4000
    },
    [2] = {
        .fa_device_id = 0,
        .fa_off = 0x8000,
        .fa_size = 0x4000
    },
    [3] = {
        .fa_device_id = 0,
        .fa_off = 0xc000,
        .fa_size = 0x4000
    }
};

static void
fcb_test_wipe(void)
{
    int i;
    int rc;
    struct flash_area *fap;

    for (i = 0; i < sizeof(test_fcb_area) / sizeof(test_fcb_area[0]); i++) {
        fap = &test_fcb_area[i];
        rc = flash_area_erase(fap, 0, fap->fa_size);
        TEST_ASSERT(rc == 0);
    }
}

TEST_CASE(fcb_test_len)
{
    uint8_t buf[3];
    uint16_t len;
    uint16_t len2;
    int rc;

    for (len = 0; len < FCB_MAX_LEN; len++) {
        rc = fcb_put_len(buf, len);
        TEST_ASSERT(rc == 1 || rc == 2);

        rc = fcb_get_len(buf, &len2);
        TEST_ASSERT(rc == 1 || rc == 2);

        TEST_ASSERT(len == len2);
    }
}

TEST_CASE(fcb_test_init)
{
    int rc;
    struct fcb *fcb;

    fcb = &test_fcb;
    memset(fcb, 0, sizeof(*fcb));

    rc = fcb_init(fcb);
    TEST_ASSERT(rc == FCB_ERR_ARGS);

    fcb->f_sectors = test_fcb_area;

    rc = fcb_init(fcb);
    TEST_ASSERT(rc == FCB_ERR_ARGS);

    fcb->f_sector_cnt = 2;
    rc = fcb_init(fcb);
    TEST_ASSERT(rc == 0);
}

static int
fcb_test_empty_walk_cb(struct fcb_entry *loc, void *arg)
{
    TEST_ASSERT(0);
    return 0;
}

TEST_CASE(fcb_test_empty_walk)
{
    int rc;
    struct fcb *fcb;

    fcb_test_wipe();
    fcb = &test_fcb;
    memset(fcb, 0, sizeof(*fcb));

    fcb->f_sector_cnt = 2;
    fcb->f_sectors = test_fcb_area;

    rc = fcb_init(fcb);
    TEST_ASSERT(rc == 0);

    rc = fcb_walk(fcb, 0, fcb_test_empty_walk_cb, NULL);
    TEST_ASSERT(rc == 0);
}

static uint8_t
fcb_test_append_data(int msg_len, int off)
{
    return (msg_len ^ off);
}

static int
fcb_test_data_walk_cb(struct fcb_entry *loc, void *arg)
{
    uint16_t len;
    uint8_t test_data[128];
    int rc;
    int i;
    int *var_cnt = (int *)arg;

    len = loc->fe_data_len;

    TEST_ASSERT(len == *var_cnt);

    rc = flash_area_read(loc->fe_area, loc->fe_data_off, test_data, len);
    TEST_ASSERT(rc == 0);

    for (i = 0; i < len; i++) {
        TEST_ASSERT(test_data[i] == fcb_test_append_data(len, i));
    }
    (*var_cnt)++;
    return 0;
}

TEST_CASE(fcb_test_append)
{
    int rc;
    struct fcb *fcb;
    struct fcb_entry loc;
    uint8_t test_data[128];
    int i;
    int j;
    int var_cnt;

    fcb_test_wipe();
    fcb = &test_fcb;
    memset(fcb, 0, sizeof(*fcb));
    fcb->f_sector_cnt = 2;
    fcb->f_sectors = test_fcb_area;

    rc = fcb_init(fcb);
    TEST_ASSERT(rc == 0);

    for (i = 0; i < sizeof(test_data); i++) {
        for (j = 0; j < i; j++) {
            test_data[j] = fcb_test_append_data(i, j);
        }
        rc = fcb_append(fcb, i, &loc);
        TEST_ASSERT_FATAL(rc == 0);
        rc = flash_area_write(loc.fe_area, loc.fe_data_off, test_data, i);
        TEST_ASSERT(rc == 0);
        rc = fcb_append_finish(fcb, &loc);
        TEST_ASSERT(rc == 0);
    }

    var_cnt = 0;
    rc = fcb_walk(fcb, 0, fcb_test_data_walk_cb, &var_cnt);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(var_cnt == sizeof(test_data));
}

TEST_CASE(fcb_test_append_too_big)
{
    struct fcb *fcb;
    int rc;
    int len;
    struct fcb_entry elem_loc;

    fcb_test_wipe();
    fcb = &test_fcb;
    memset(fcb, 0, sizeof(*fcb));
    fcb->f_sector_cnt = 2;
    fcb->f_sectors = test_fcb_area;

    rc = fcb_init(fcb);
    TEST_ASSERT(rc == 0);

    /*
     * Max element which fits inside sector is
     * sector size - (disk header + crc + 1-2 bytes of length).
     */
    len = fcb->f_active.fe_area->fa_size;

    rc = fcb_append(fcb, len, &elem_loc);
    TEST_ASSERT(rc != 0);

    len--;
    rc = fcb_append(fcb, len, &elem_loc);
    TEST_ASSERT(rc != 0);

    len -= sizeof(struct fcb_disk_area);
    rc = fcb_append(fcb, len, &elem_loc);
    TEST_ASSERT(rc != 0);

    len = fcb->f_active.fe_area->fa_size -
      (sizeof(struct fcb_disk_area) + 1 + 2);
    rc = fcb_append(fcb, len, &elem_loc);
    TEST_ASSERT(rc == 0);

    rc = fcb_append_finish(fcb, &elem_loc);
    TEST_ASSERT(rc == 0);

    rc = fcb_elem_info(fcb, &elem_loc);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(elem_loc.fe_data_len == len);
}

struct append_arg {
    int *elem_cnts;
};

static int
fcb_test_cnt_elems_cb(struct fcb_entry *loc, void *arg)
{
    struct append_arg *aa = (struct append_arg *)arg;
    int idx;

    idx = loc->fe_area - &test_fcb_area[0];
    aa->elem_cnts[idx]++;
    return 0;
}

TEST_CASE(fcb_test_append_fill)
{
    struct fcb *fcb;
    int rc;
    int i;
    struct fcb_entry loc;
    uint8_t test_data[128];
    int elem_cnts[2] = {0, 0};
    int aa_together_cnts[2];
    struct append_arg aa_together = {
        .elem_cnts = aa_together_cnts
    };
    int aa_separate_cnts[2];
    struct append_arg aa_separate = {
        .elem_cnts = aa_separate_cnts
    };

    fcb_test_wipe();
    fcb = &test_fcb;
    memset(fcb, 0, sizeof(*fcb));
    fcb->f_sector_cnt = 2;
    fcb->f_sectors = test_fcb_area;

    rc = fcb_init(fcb);
    TEST_ASSERT(rc == 0);

    for (i = 0; i < sizeof(test_data); i++) {
        test_data[i] = fcb_test_append_data(sizeof(test_data), i);
    }

    while (1) {
        rc = fcb_append(fcb, sizeof(test_data), &loc);
        if (rc == FCB_ERR_NOSPACE) {
            break;
        }
        if (loc.fe_area == &test_fcb_area[0]) {
            elem_cnts[0]++;
        } else if (loc.fe_area == &test_fcb_area[1]) {
            elem_cnts[1]++;
        } else {
            TEST_ASSERT(0);
        }

        rc = flash_area_write(loc.fe_area, loc.fe_data_off, test_data,
          sizeof(test_data));
        TEST_ASSERT(rc == 0);

        rc = fcb_append_finish(fcb, &loc);
        TEST_ASSERT(rc == 0);
    }
    TEST_ASSERT(elem_cnts[0] > 0);
    TEST_ASSERT(elem_cnts[0] == elem_cnts[1]);

    memset(&aa_together_cnts, 0, sizeof(aa_together_cnts));
    rc = fcb_walk(fcb, NULL, fcb_test_cnt_elems_cb, &aa_together);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(aa_together.elem_cnts[0] == elem_cnts[0]);
    TEST_ASSERT(aa_together.elem_cnts[1] == elem_cnts[1]);

    memset(&aa_separate_cnts, 0, sizeof(aa_separate_cnts));
    rc = fcb_walk(fcb, &test_fcb_area[0], fcb_test_cnt_elems_cb,
      &aa_separate);
    TEST_ASSERT(rc == 0);
    rc = fcb_walk(fcb, &test_fcb_area[1], fcb_test_cnt_elems_cb,
      &aa_separate);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(aa_separate.elem_cnts[0] == elem_cnts[0]);
    TEST_ASSERT(aa_separate.elem_cnts[1] == elem_cnts[1]);

}

TEST_CASE(fcb_test_reset)
{
    struct fcb *fcb;
    int rc;
    int i;
    struct fcb_entry loc;
    uint8_t test_data[128];
    int var_cnt;

    fcb_test_wipe();
    fcb = &test_fcb;
    memset(fcb, 0, sizeof(*fcb));
    fcb->f_sector_cnt = 2;
    fcb->f_sectors = test_fcb_area;

    rc = fcb_init(fcb);
    TEST_ASSERT(rc == 0);

    var_cnt = 0;
    rc = fcb_walk(fcb, 0, fcb_test_data_walk_cb, &var_cnt);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(var_cnt == 0);

    rc = fcb_append(fcb, 32, &loc);
    TEST_ASSERT(rc == 0);

    /*
     * No ready ones yet. CRC should not match.
     */
    var_cnt = 0;
    rc = fcb_walk(fcb, 0, fcb_test_data_walk_cb, &var_cnt);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(var_cnt == 0);

    for (i = 0; i < sizeof(test_data); i++) {
        test_data[i] = fcb_test_append_data(32, i);
    }
    rc = flash_area_write(loc.fe_area, loc.fe_data_off, test_data, 32);
    TEST_ASSERT(rc == 0);

    rc = fcb_append_finish(fcb, &loc);
    TEST_ASSERT(rc == 0);

    /*
     * one entry
     */
    var_cnt = 32;
    rc = fcb_walk(fcb, 0, fcb_test_data_walk_cb, &var_cnt);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(var_cnt == 33);

    /*
     * Pretend reset
     */
    memset(fcb, 0, sizeof(*fcb));
    fcb->f_sector_cnt = 2;
    fcb->f_sectors = test_fcb_area;

    rc = fcb_init(fcb);
    TEST_ASSERT(rc == 0);

    var_cnt = 32;
    rc = fcb_walk(fcb, 0, fcb_test_data_walk_cb, &var_cnt);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(var_cnt == 33);

    rc = fcb_append(fcb, 33, &loc);
    TEST_ASSERT(rc == 0);

    for (i = 0; i < sizeof(test_data); i++) {
        test_data[i] = fcb_test_append_data(33, i);
    }
    rc = flash_area_write(loc.fe_area, loc.fe_data_off, test_data, 33);
    TEST_ASSERT(rc == 0);

    rc = fcb_append_finish(fcb, &loc);
    TEST_ASSERT(rc == 0);

    var_cnt = 32;
    rc = fcb_walk(fcb, 0, fcb_test_data_walk_cb, &var_cnt);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(var_cnt == 34);

    /*
     * Add partial one, make sure that we survive reset then.
     */
    rc = fcb_append(fcb, 34, &loc);
    TEST_ASSERT(rc == 0);

    memset(fcb, 0, sizeof(*fcb));
    fcb->f_sector_cnt = 2;
    fcb->f_sectors = test_fcb_area;

    rc = fcb_init(fcb);
    TEST_ASSERT(rc == 0);

    /*
     * Walk should skip that.
     */
    var_cnt = 32;
    rc = fcb_walk(fcb, 0, fcb_test_data_walk_cb, &var_cnt);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(var_cnt == 34);

    /* Add a 3rd one, should go behind corrupt entry */
    rc = fcb_append(fcb, 34, &loc);
    TEST_ASSERT(rc == 0);

    for (i = 0; i < sizeof(test_data); i++) {
        test_data[i] = fcb_test_append_data(34, i);
    }
    rc = flash_area_write(loc.fe_area, loc.fe_data_off, test_data, 34);
    TEST_ASSERT(rc == 0);

    rc = fcb_append_finish(fcb, &loc);
    TEST_ASSERT(rc == 0);

    /*
     * Walk should skip corrupt entry, but report the next one.
     */
    var_cnt = 32;
    rc = fcb_walk(fcb, 0, fcb_test_data_walk_cb, &var_cnt);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(var_cnt == 35);
}

TEST_CASE(fcb_test_rotate)
{
    struct fcb *fcb;
    int rc;
    int old_id;
    struct fcb_entry loc;
    uint8_t test_data[128];
    int elem_cnts[2] = {0, 0};
    int cnts[2];
    struct append_arg aa_arg = {
        .elem_cnts = cnts
    };

    fcb_test_wipe();
    fcb = &test_fcb;
    memset(fcb, 0, sizeof(*fcb));
    fcb->f_sector_cnt = 2;
    fcb->f_sectors = test_fcb_area;

    rc = fcb_init(fcb);
    TEST_ASSERT(rc == 0);

    old_id = fcb->f_active_id;
    rc = fcb_rotate(fcb);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(fcb->f_active_id == old_id + 1);

    /*
     * Now fill up the
     */
    while (1) {
        rc = fcb_append(fcb, sizeof(test_data), &loc);
        if (rc == FCB_ERR_NOSPACE) {
            break;
        }
        if (loc.fe_area == &test_fcb_area[0]) {
            elem_cnts[0]++;
        } else if (loc.fe_area == &test_fcb_area[1]) {
            elem_cnts[1]++;
        } else {
            TEST_ASSERT(0);
        }

        rc = flash_area_write(loc.fe_area, loc.fe_data_off, test_data,
          sizeof(test_data));
        TEST_ASSERT(rc == 0);

        rc = fcb_append_finish(fcb, &loc);
        TEST_ASSERT(rc == 0);
    }
    TEST_ASSERT(elem_cnts[0] > 0 && elem_cnts[0] == elem_cnts[1]);

    old_id = fcb->f_active_id;
    rc = fcb_rotate(fcb);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(fcb->f_active_id == old_id); /* no new area created */

    memset(cnts, 0, sizeof(cnts));
    rc = fcb_walk(fcb, NULL, fcb_test_cnt_elems_cb, &aa_arg);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(aa_arg.elem_cnts[0] == elem_cnts[0] ||
      aa_arg.elem_cnts[1] == elem_cnts[1]);
    TEST_ASSERT(aa_arg.elem_cnts[0] == 0 || aa_arg.elem_cnts[1] == 0);

    /*
     * One sector is full. The other one should have one entry in it.
     */
    rc = fcb_append(fcb, sizeof(test_data), &loc);
    TEST_ASSERT(rc == 0);

    rc = flash_area_write(loc.fe_area, loc.fe_data_off, test_data,
      sizeof(test_data));
    TEST_ASSERT(rc == 0);

    rc = fcb_append_finish(fcb, &loc);
    TEST_ASSERT(rc == 0);

    old_id = fcb->f_active_id;
    rc = fcb_rotate(fcb);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(fcb->f_active_id == old_id);

    memset(cnts, 0, sizeof(cnts));
    rc = fcb_walk(fcb, NULL, fcb_test_cnt_elems_cb, &aa_arg);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(aa_arg.elem_cnts[0] == 1 || aa_arg.elem_cnts[1] == 1);
    TEST_ASSERT(aa_arg.elem_cnts[0] == 0 || aa_arg.elem_cnts[1] == 0);
}

TEST_CASE(fcb_test_multiple_scratch)
{
    struct fcb *fcb;
    int rc;
    struct fcb_entry loc;
    uint8_t test_data[128];
    int elem_cnts[4];
    int idx;
    int cnts[4];
    struct append_arg aa_arg = {
        .elem_cnts = cnts
    };

    fcb_test_wipe();
    fcb = &test_fcb;
    memset(fcb, 0, sizeof(*fcb));
    fcb->f_sector_cnt = 4;
    fcb->f_scratch_cnt = 1;
    fcb->f_sectors = test_fcb_area;

    rc = fcb_init(fcb);
    TEST_ASSERT(rc == 0);

    /*
     * Now fill up everything. We should be able to get 3 of the sectors
     * full.
     */
    memset(elem_cnts, 0, sizeof(elem_cnts));
    while (1) {
        rc = fcb_append(fcb, sizeof(test_data), &loc);
        if (rc == FCB_ERR_NOSPACE) {
            break;
        }
        idx = loc.fe_area - &test_fcb_area[0];
        elem_cnts[idx]++;

        rc = flash_area_write(loc.fe_area, loc.fe_data_off, test_data,
          sizeof(test_data));
        TEST_ASSERT(rc == 0);

        rc = fcb_append_finish(fcb, &loc);
        TEST_ASSERT(rc == 0);
    }

    TEST_ASSERT(elem_cnts[0] > 0);
    TEST_ASSERT(elem_cnts[0] == elem_cnts[1] && elem_cnts[0] == elem_cnts[2]);
    TEST_ASSERT(elem_cnts[3] == 0);

    /*
     * Ask to use scratch block, then fill it up.
     */
    rc = fcb_append_to_scratch(fcb);
    TEST_ASSERT(rc == 0);

    while (1) {
        rc = fcb_append(fcb, sizeof(test_data), &loc);
        if (rc == FCB_ERR_NOSPACE) {
            break;
        }
        idx = loc.fe_area - &test_fcb_area[0];
        elem_cnts[idx]++;

        rc = flash_area_write(loc.fe_area, loc.fe_data_off, test_data,
          sizeof(test_data));
        TEST_ASSERT(rc == 0);

        rc = fcb_append_finish(fcb, &loc);
        TEST_ASSERT(rc == 0);
    }
    TEST_ASSERT(elem_cnts[3] == elem_cnts[0]);

    /*
     * Rotate
     */
    rc = fcb_rotate(fcb);
    TEST_ASSERT(rc == 0);

    memset(&cnts, 0, sizeof(cnts));
    rc = fcb_walk(fcb, NULL, fcb_test_cnt_elems_cb, &aa_arg);
    TEST_ASSERT(rc == 0);

    TEST_ASSERT(cnts[0] == 0);
    TEST_ASSERT(cnts[1] > 0);
    TEST_ASSERT(cnts[1] == cnts[2] && cnts[1] == cnts[3]);

    rc = fcb_append_to_scratch(fcb);
    TEST_ASSERT(rc == 0);
    rc = fcb_append_to_scratch(fcb);
    TEST_ASSERT(rc != 0);
}

TEST_SUITE(fcb_test_all)
{
    fcb_test_len();

    fcb_test_init();

    fcb_test_empty_walk();

    fcb_test_append();

    fcb_test_append_too_big();

    fcb_test_append_fill();

    fcb_test_reset();

    fcb_test_rotate();

    fcb_test_multiple_scratch();
}

#if MYNEWT_VAL(SELFTEST)

int
main(int argc, char **argv)
{
    tu_config.tc_print_results = 1;
    tu_init();

    fcb_test_all();

    return tu_any_failed;
}
#endif

