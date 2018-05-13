#ifndef _GNSS_LOG_H_
#define _GNSS_LOG_H_

#include <os/mynewt.h>
#include <log/log.h>

extern struct log _gnss_log;

#define GNSS_LOG_INFO(fmt...) \
    LOG_INFO(&_gnss_log, LOG_MODULE_DEFAULT, fmt)

#endif

