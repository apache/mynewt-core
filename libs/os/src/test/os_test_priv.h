#ifndef H_OS_TEST_PRIV_
#define H_OS_TEST_PRIV_

void os_test_arch_start(void);
void os_test_arch_stop(void);

int os_mempool_test_suite(void);
int os_mutex_test_suite(void);
int os_sem_test_suite(void);

#endif
