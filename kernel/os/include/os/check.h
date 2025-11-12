/*
 * Copyright (c) 2019 Intel Corporation
 * Copyright (c) 2025 Codecoup
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef OS_CHECK_H_
#define OS_CHECK_H_

#include <assert.h>
#include <syscfg/syscfg.h>

#if MYNEWT_VAL(OS_ASSERT_ON_ERRORS)
#define M_CHECKIF(expr)                                                       \
    assert(!(expr));                                                          \
    if (0)
#elif MYNEWT_VAL(OS_NO_RUNTIME_CHECKS)
#define M_CHECKIF(expr) if (0 && (expr))
#else
#define M_CHECKIF(expr) if (expr)
#endif

#endif /* OS_CHECK_H_ */
