#ifndef H_TESTUTIL_
#define H_TESTUTIL_

#include <inttypes.h>
#include <setjmp.h>
struct ffs_area_desc;

struct tu_config {
    int tc_verbose;
    const char *tc_base_path;
    const struct ffs_area_desc *tc_area_descs;
};

extern struct tu_config tu_config;

int tu_init(void);
void tu_restart(void);

void tu_suite_init(const char *name);

void tu_case_init(const char *name);
void tu_case_complete(void);
void tu_case_fail_assert(int fatal, const char *file, int line,
                         const char *expr, const char *format, ...);
void tu_case_write_pass_auto(void);
void tu_case_pass_manual(const char *file, int line,
                         const char *format, ...);

extern int tu_any_failed;

extern int tu_first_idx;

extern const char *tu_suite_name;
extern int tu_suite_failed;

extern const char *tu_case_name;
extern int tu_case_reported;
extern int tu_case_failed;
extern int tu_case_idx;
extern jmp_buf tu_case_jb;

#define TEST_SUITE(suite_name)                                                \
    static void TEST_SUITE_##suite_name(void);                                \
                                                                              \
    int                                                                       \
    suite_name(void)                                                          \
    {                                                                         \
        tu_suite_init(#suite_name);                                           \
        TEST_SUITE_##suite_name();                                            \
                                                                              \
        return tu_suite_failed;                                               \
    }                                                                         \
                                                                              \
    static void                                                               \
    TEST_SUITE_##suite_name(void)

#define TEST_CASE(case_name)                                                  \
    static void TEST_CASE_##case_name(void);                                  \
                                                                              \
    int                                                                       \
    case_name(void)                                                           \
    {                                                                         \
        if (tu_case_idx >= tu_first_idx) {                                    \
            tu_case_init(#case_name);                                         \
                                                                              \
            if (setjmp(tu_case_jb) == 0) {                                    \
                TEST_CASE_##case_name();                                      \
                tu_case_write_pass_auto();                                    \
            }                                                                 \
        }                                                                     \
                                                                              \
        tu_case_complete();                                                   \
                                                                              \
        return tu_case_failed;                                                \
    }                                                                         \
                                                                              \
    static void                                                               \
    TEST_CASE_##case_name(void)

#define FIRST_AUX(first, ...) first
#define FIRST(...) FIRST_AUX(__VA_ARGS__, _)

#define NUM(...) ARG10(__VA_ARGS__, N, N, N, N, N, N, N, N, 1, _)
#define ARG10(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, ...) a10

#define REST_OR_0(...) REST_OR_0_AUX(NUM(__VA_ARGS__), __VA_ARGS__)
#define REST_OR_0_AUX(qty, ...) REST_OR_0_AUX_INNER(qty, __VA_ARGS__)
#define REST_OR_0_AUX_INNER(qty, ...) REST_OR_0_AUX_##qty(__VA_ARGS__)
#define REST_OR_0_AUX_1(first) 0
#define REST_OR_0_AUX_N(first, ...) __VA_ARGS__

#define XSTR(s) STR(s)
#define STR(s) #s

#define TEST_ASSERT_FULL(fatal, expr, ...) do                                 \
{                                                                             \
    if (!(expr)) {                                                            \
        tu_case_fail_assert((fatal), __FILE__, __LINE__, XSTR(expr),          \
                            __VA_ARGS__);                                     \
    }                                                                         \
} while (0)

#define TEST_ASSERT(...)                                                      \
    TEST_ASSERT_FULL(0, FIRST(__VA_ARGS__), REST_OR_0(__VA_ARGS__))

#define TEST_ASSERT_FATAL(...)                                                \
    TEST_ASSERT_FULL(1, FIRST(__VA_ARGS__), REST_OR_0(__VA_ARGS__))

#define TEST_PASS(...)                                                        \
    tu_case_pass_manual(__FILE__, __LINE__, __VA_ARGS__);

#endif
