/*
 * Copyright (c) 2020-2026 Siddharth Chandrasekaran <sidcha.dev@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _OSDP_CONFIG_H_
#define _OSDP_CONFIG_H_

#include <syscfg/syscfg.h>

/**
 * @brief The following macros are defined defined from the variable in cmake
 * files. All @XXX@ are replaced by the value of XXX as resolved by cmake.
 */
#define PROJECT_VERSION "3.2.1"
#define PROJECT_NAME    "libosdp"
#define GIT_BRANCH      ""
#define GIT_REV         ""
#define GIT_TAG         ""
#define GIT_DIFF        ""
#define REPO_ROOT       ""

/**
 * @brief OSDP configuration parameters
 *
 * These values can be overridden by build system (Kconfig for Zephyr,
 * CMake options for other platforms). The values below are defaults
 * used when not overridden.
 */

/* Protocol Timings */
#ifndef OSDP_PD_SC_RETRY_MS
#define OSDP_PD_SC_RETRY_MS (MYNEWT_VAL(OSDP_SC_RETRY_WAIT_SEC) * 1000u)
#endif

#ifndef OSDP_PD_POLL_TIMEOUT_MS
#define OSDP_PD_POLL_TIMEOUT_MS (1000 / MYNEWT_VAL(OSDP_PD_POLL_RATE))
#endif

#ifndef OSDP_PD_SC_TIMEOUT_MS
#define OSDP_PD_SC_TIMEOUT_MS (MYNEWT_VAL(OSDP_PD_SC_TIMEOUT_MS) * 1000u)
#endif

#ifndef OSDP_PD_ONLINE_TOUT_MS
#define OSDP_PD_ONLINE_TOUT_MS (MYNEWT_VAL(OSDP_PD_IDLE_TIMEOUT_MS))
#endif

#ifndef OSDP_RESP_TOUT_MS
#define OSDP_RESP_TOUT_MS (MYNEWT_VAL(OSDP_RESP_TOUT_MS))
#endif

#ifndef OSDP_CMD_RETRY_WAIT_MS
#define OSDP_CMD_RETRY_WAIT_MS (MYNEWT_VAL(OSDP_CMD_RETRY_WAIT_SEC) * 1000)
#endif

#ifndef OSDP_ONLINE_RETRY_WAIT_MAX_MS
#define OSDP_ONLINE_RETRY_WAIT_MAX_MS                                         \
    (MYNEWT_VAL(OSDP_ONLINE_RETRY_WAIT_MAX_SEC) * 1000u)
#endif

/* Protocol Limits */
#ifndef OSDP_CMD_MAX_RETRIES
#define OSDP_CMD_MAX_RETRIES (8)
#endif

#ifndef OSDP_FILE_ERROR_RETRY_MAX
#define OSDP_FILE_ERROR_RETRY_MAX (10)
#endif

#ifndef OSDP_PD_MAX
#define OSDP_PD_MAX (126)
#endif

#ifndef OSDP_PD_NAME_MAXLEN
#define OSDP_PD_NAME_MAXLEN (16)
#endif

/* Memory Configuration */
#ifndef OSDP_PACKET_BUF_SIZE
#define OSDP_PACKET_BUF_SIZE (256)
#endif

#ifndef OSDP_MINIMUM_PACKET_SIZE
#define OSDP_MINIMUM_PACKET_SIZE (128)
#endif

#ifndef OSDP_RX_RB_SIZE
#define OSDP_RX_RB_SIZE (512)
#endif

#ifndef OSDP_CP_CMD_POOL_SIZE
#define OSDP_CP_CMD_POOL_SIZE (4)
#endif

/* Internal Constants */
#ifndef OSDP_CMD_ID_OFFSET
#define OSDP_CMD_ID_OFFSET (MYNEWT_VAL(OSDP_CMD_ID_OFFSET))
#endif

#ifndef OSDP_PCAP_LINK_TYPE
#define OSDP_PCAP_LINK_TYPE (162)
#endif

#endif /* _OSDP_CONFIG_H_ */
