#include "log_test_util/log_test_util.h"

struct sector_range fcb_ranges[] = {
    [0] = {
        .sr_flash_area = {
            .fa_device_id = 0,
            .fa_off = 0,
            .fa_size = 0x8000, /* 32K */
        },
        .sr_range_start = 0,
        .sr_first_sector = 0,
        .sr_sector_size = 0x4000, /* 16 K */
        .sr_sector_count = 2,
    },
};

static int ltu_str_idx = 0;
static int ltu_str_max_idx = 0;

char *ltu_str_logs[] = {
    "testdata",
    "1testdata2",
    "",
    "alkjfadkjsfajsd;kfjadls;hg;lasdhgl;aksdhfl;asdkh;afbabababaaacsds",
    NULL
};

static uint8_t ltu_cbmem_buf[2048];

int
ltu_num_strs(void)
{
    int i;

    for (i = 0; ltu_str_logs[i] != NULL; i++) { }

    return i;
}

struct os_mbuf *
ltu_flat_to_fragged_mbuf(const void *flat, int len, int frag_sz)
{
    const uint8_t *u8p;
    struct os_mbuf *first;
    struct os_mbuf *cur;
    int chunk_sz;
    int rc;

    first = NULL;

    u8p = flat;
    do {
        cur = os_msys_get(0, 0);
        TEST_ASSERT_FATAL(cur != NULL);

        if (len >= frag_sz) {
            chunk_sz = frag_sz;
        } else {
            chunk_sz = len;
        }

        rc = os_mbuf_copyinto(cur, 0, u8p, chunk_sz);
        TEST_ASSERT_FATAL(rc == 0);

        u8p += chunk_sz;
        len -= chunk_sz;

        if (first == NULL) {
            first = cur;
        } else {
            os_mbuf_concat(first, cur);
        }
    } while (len > 0);

    return first;
}

void
ltu_setup_fcb(struct fcb_log *fcb_log, struct log *log)
{
    int rc;
    int i;

    sysinit();

    *fcb_log = (struct fcb_log) { 0 };

    fcb_log->fl_fcb.f_ranges = fcb_ranges;
    fcb_log->fl_fcb.f_sector_cnt = fcb_ranges[0].sr_sector_count;
    fcb_log->fl_fcb.f_range_cnt = 1;
    fcb_log->fl_fcb.f_magic = 0x7EADBADF;
    fcb_log->fl_fcb.f_version = 0;

    for (i = 0; i < fcb_log->fl_fcb.f_sector_cnt; i++) {
        rc = fcb_sector_erase(&fcb_log->fl_fcb, i);
        TEST_ASSERT(rc == 0);
    }
    rc = fcb_init(&fcb_log->fl_fcb);
    TEST_ASSERT(rc == 0);

    log_register("log", log, &log_fcb_handler, fcb_log, LOG_SYSLEVEL);
}

void
ltu_setup_cbmem(struct cbmem *cbmem, struct log *log)
{
    sysinit();

    cbmem_init(cbmem, ltu_cbmem_buf, sizeof ltu_cbmem_buf);
    log_register("log", log, &log_cbmem_handler, cbmem, LOG_SYSLEVEL);
}

static int
ltu_walk_verify(struct log *log, struct log_offset *log_offset,
                void *dptr, uint16_t len)
{
    int rc;
    struct log_entry_hdr ueh;
    struct os_mbuf *om;
    char data[128];
    int dlen;

    TEST_ASSERT(ltu_str_idx < ltu_str_max_idx);

    /*** Verify contents using single read. */

    rc = log_read(log, dptr, &ueh, 0, sizeof(ueh));
    TEST_ASSERT(rc == sizeof(ueh));

    dlen = len - sizeof(ueh);
    TEST_ASSERT(dlen < sizeof(data));

    rc = log_read(log, dptr, data, sizeof(ueh), dlen);
    TEST_ASSERT(rc == dlen);

    data[rc] = '\0';

    TEST_ASSERT(strlen(ltu_str_logs[ltu_str_idx]) == dlen);
    TEST_ASSERT(!memcmp(ltu_str_logs[ltu_str_idx], data, dlen));

    /*** Verify contents using separate header and body reads. */

    rc = log_read_hdr(log, dptr, &ueh);
    TEST_ASSERT(rc == 0);

    rc = log_read_body(log, dptr, data, 0, dlen);
    TEST_ASSERT(rc == dlen);

    TEST_ASSERT(strlen(ltu_str_logs[ltu_str_idx]) == dlen);
    TEST_ASSERT(!memcmp(ltu_str_logs[ltu_str_idx], data, dlen));

    /*** Verify contents using mbuf read. */

    om = os_msys_get_pkthdr(0, 0);
    TEST_ASSERT_FATAL(om != NULL);

    rc = log_read_mbuf(log, dptr, om, sizeof(ueh), dlen);
    TEST_ASSERT(rc == dlen);

    TEST_ASSERT(strlen(ltu_str_logs[ltu_str_idx]) == dlen);
    TEST_ASSERT(os_mbuf_cmpf(om, 0, ltu_str_logs[ltu_str_idx], dlen) == 0);

    /*** Verify contents using mbuf read body. */

    os_mbuf_adj(om, OS_MBUF_PKTLEN(om));

    rc = log_read_mbuf_body(log, dptr, om, 0, dlen);
    TEST_ASSERT(rc == dlen);

    TEST_ASSERT(strlen(ltu_str_logs[ltu_str_idx]) == dlen);
    TEST_ASSERT(os_mbuf_cmpf(om, 0, ltu_str_logs[ltu_str_idx], dlen) == 0);

    os_mbuf_free_chain(om);

    ltu_str_idx++;

    return 0;
}

static int
ltu_walk_body_verify(struct log *log, struct log_offset *log_offset,
                     const struct log_entry_hdr *euh, void *dptr, uint16_t len)
{
    struct os_mbuf *om;
    char data[128];
    int rc;

    TEST_ASSERT(ltu_str_idx < ltu_str_max_idx);

    /*** Verify contents using single read. */

    TEST_ASSERT(len < sizeof(data));

    rc = log_read_body(log, dptr, data, 0, len);
    TEST_ASSERT(rc == len);

    data[rc] = '\0';

    TEST_ASSERT(strlen(ltu_str_logs[ltu_str_idx]) == len);
    TEST_ASSERT(memcmp(ltu_str_logs[ltu_str_idx], data, len) == 0);

    /*** Verify contents using mbuf read body. */

    om = os_msys_get_pkthdr(0, 0);
    TEST_ASSERT_FATAL(om != NULL);

    rc = log_read_mbuf_body(log, dptr, om, 0, len);
    TEST_ASSERT(rc == len);

    TEST_ASSERT(strlen(ltu_str_logs[ltu_str_idx]) == len);
    TEST_ASSERT(os_mbuf_cmpf(om, 0, ltu_str_logs[ltu_str_idx], len) == 0);

    os_mbuf_free_chain(om);

    ltu_str_idx++;

    return 0;
}

static int
ltu_walk_empty(struct log *log, struct log_offset *log_offset,
               void *dptr, uint16_t len)
{
    TEST_ASSERT(0);
    return 0;
}

void
ltu_verify_contents(struct log *log)
{
    struct log_offset log_offset = { 0 };
    int rc;

    ltu_str_max_idx = ltu_num_strs();

    /* Regular walk. */
    ltu_str_idx = 0;
    rc = log_walk(log, ltu_walk_verify, &log_offset);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(ltu_str_idx == ltu_str_max_idx);

    /* Body walk. */
    ltu_str_idx = 0;
    rc = log_walk_body(log, ltu_walk_body_verify, &log_offset);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(ltu_str_idx == ltu_str_max_idx);

    rc = log_flush(log);
    TEST_ASSERT(rc == 0);

    rc = log_walk(log, ltu_walk_empty, &log_offset);
    TEST_ASSERT(rc == 0);
}
