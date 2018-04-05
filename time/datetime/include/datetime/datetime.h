/*
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
#ifndef __UTIL_DATETIME_H
#define __UTIL_DATETIME_H

#ifdef __cplusplus
extern "C" {
#endif

struct os_timeval;
struct os_timezone;

#define DATETIME_BUFSIZE    33

struct clocktime {
    int year;   /* year (4 digit year) */
    int mon;    /* month (1 - 12) */
    int day;    /* day (1 - 31) */
    int hour;   /* hour (0 - 23) */
    int min;    /* minute (0 - 59) */
    int sec;    /* second (0 - 59) */
    int dow;    /* day of week (0 - 6; 0 = Sunday) */
    int usec;   /* micro seconds */
};

/*
 * Format the time specified by 'utctime' and 'tz' as per RFC 3339 in
 * the 'output' string. The size of the buffer pointed to by 'output'
 * is specified by 'olen'.
 *
 * Returns 0 on success and non-zero on failure.
 */
int datetime_format(const struct os_timeval *utctime,
    const struct os_timezone *tz, char *output, int olen);

/*
 * Parse 'input' in the Internet date/time format specified by RFC 3339.
 * https://www.ietf.org/rfc/rfc3339.txt
 *
 * We deviate from the RFC in that if the 'time offset' is left unspecified
 * then we default to UTC time.
 *
 * Return 0 if 'input' could be parsed successfully and non-zero otherwise.
 * 'utctime' and 'tz' are updated if 'input' was parsed successfully.
 */
int datetime_parse(const char *input, struct os_timeval *utctime,
    struct os_timezone *tz);


/*
 * Converts from struct clocktime to struct os_timeval
 *
 * Returns 0 on success and OS_EINVAL on invalid input data
 */
int clocktime_to_timeval(const struct clocktime *ct, const struct os_timezone *tz, struct os_timeval *tv);

/*
 * Converts from struct os_timeval to struct clocktime
 *
 * Returns 0 on success and OS_EINVAL on invalid input data
 */
int timeval_to_clocktime(const struct os_timeval *tv, const struct os_timezone *tz,
    struct clocktime *ct);

#ifdef __cplusplus
}
#endif

#endif  /* __UTIL_DATETIME_H */
