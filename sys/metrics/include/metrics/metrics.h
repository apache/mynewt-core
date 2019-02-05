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

#ifndef __SYS_METRICS_H__
#define __SYS_METRICS_H__

#include <stdbool.h>
#include <stdint.h>
#include "log/log.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Event metrics is a generic API to collect various data about events happening
 * in a system.
 *
 * Events are defined by application and can represent a simple notification
 * (e.g. "battery state") as well as complex event with data collected over time
 * (e.g. "Bluetooth connection event") - there are no constraints here.
 *
 * Each event has number of metrics associated with it. These metrics are
 * properties of an event and each event can have up to 32 metrics defined.
 * Each metric is either a single value (only one value can be stored per event)
 * or a series value (multiple values can be stored per event). For example:
 *
 * Simple "battery state" event may define metrics:
 * - battery type (single value)
 * - battery level (single value)
 *
 * Bluetooth connection event is series of packets exchanged over the air during
 * connection thus "Bluetooth connection event" may define following metrics:
 * - connection handle (single value)
 * - connection PHY (single value)
 * - packet type (series value, one value for each packet)
 * - inter-frame space (series value)
 * - packet count (single value)
 *
 * There is no limit on number of events created in system (except for available
 * memory).
 *
 * To use events, first a set of metrics needs to be defined using helper macros
 * as below:
 *
 *     METRICS_SECT_START(my_power_metrics)
 *         METRICS_SECT_ENTRY(event_src, EVENT_METRIC_TYPE_SINGLE_U)
 *         METRICS_SECT_ENTRY(power_per_sec, EVENT_METRIC_TYPE_SERIES_U16)
 *         METRICS_SECT_ENTRY(avg_power, EVENT_METRIC_TYPE_SINGLE_U)
 *         METRICS_SECT_ENTRY(min_power, EVENT_METRIC_TYPE_SINGLE_U)
 *         METRICS_SECT_ENTRY(max_power, EVENT_METRIC_TYPE_SINGLE_U)
 *     METRICS_SECT_END
 *
 * This creates metrics section named 'my_power_metrics' with 4 metrics inside,
 * each has name and value type specified. Single value metrics store only last
 * value logged. Series value metrics store all logged values.
 *
 * NOTE:
 * Currently APIs identify metrics by their ordinal number in definition, i.e.:
 *     event_src => 0, power_per_sec => 1, ...
 * To workaround this some symbols may be defined manually, e.g. using an enum:
 *    enum {
 *        POWER_METRIC_EVENT_SRC, // 0
 *        POWER_METRIC_PPS,       // 1
 *        POWER_METRIC_AVG,       // 2
 *        POWER_METRIC_MIN,       // 3
 *        POWER_METRIC_MAX,       // 4
 *    };
 *
 * Next step is to define a structure which describes an event that can store
 * data for all metrics:
 *
 *     METRICS_EVENT_DECLARE(power_event, my_power_metrics);
 *
 * This defines new type 'struct power_source_event' which is a structure large
 * enough to hold data for all metrics defined in set 'my_power_metrics'. Such
 * type can be used to define event variable which is an event instance (can be
 * either static variable or dynamically allocated - it does not matter), e.g.:
 *
 *     struct power_event my_power_event;
 *
 * Event variable needs to be initialized before usage. Each event struct has
 * `hdr` field included which should be passed as an argument identifying event
 * in APIs. Metrics set passed to initialization function shall be the same as
 * used to define event struct.
 *
 *     metrics_event_init(&my_power_event.hdr, my_power_metrics,
 *                       METRICS_SECT_COUNT(my_metrics), "power_event");
 *
 * Event data may be logged manually (e.g. serialized to CBOR using appropriate
 * APIs) or automatically logged to log instance:
 *
 *         struct log log; // see logging subsystem on how to handle this
 *         event_metric_set_log(&log, LOG_MODULE_DEFAULT, LOG_LEVEL_INFO);
 *
 * Sample scenario for collecting data to event might look as below:
 *
 * 1. start event with current timestamp
 *         metrics_event_start(&my_power_event.hdr, os_cputime_get32());
 *
 * 2. log some starting value relavant to this event, e.g. what triggered event
 *         metrics_set_value(&my_power_event.hdr, POWER_METRIC_EVENT_SRC, xxx);
 *
 * 3. log current power value every second
 *         metrics_set_value(&my_power_event.hdr, POWER_METRIC_PPS, cur_power);
 *
 * 4. at end of event log calculated min/max/avg values
 *         metrics_set_value(&my_power_event.hdr, POWER_METRIC_AVG, power_avg);
 *         metrics_set_value(&my_power_event.hdr, POWER_METRIC_MIN, power_min);
 *         metrics_set_value(&my_power_event.hdr, POWER_METRIC_MAX, power_max);
 *
 * 5. finish collecting data for this event
 *         metrics_event_end(&my_power_event.hdr);
 *
 * Now the data can be collected for the same event again by repeating these
 * steps. In case data collection starts just after finishing an event, the
 * call to metrics_event_end() may be omitted as metrics_event_start() will do
 * this implicitly.
 *
 * The amount of data which can be collected for all data series of all metrics
 * in a system is limited by capacity of mempool defined by following syscfg:
 *      METRICS_POOL_COUNT - number of blocks in mempool
 *      METRICS_POOL_SIZE - size of each block in mempool
 *
 * In general, each series require at least single block to hold a single value.
 * As number of values increases in a series it may be necessary to allocate
 * more blocks for the same data series. Once event data is reset, all blocks
 * allocated for an event are freed.
 */

/* Helper to define metric type - use types defined below instead! */
#define METRICS_TYPE(__series, __sign, __size) \
    ((__series) << 7) | ((__sign) << 6) | ((__size))

/* Type definitions for metrics */
#define METRICS_TYPE_SINGLE_U           METRICS_TYPE(0, 0, sizeof(uint32_t))
#define METRICS_TYPE_SINGLE_S           METRICS_TYPE(0, 1, sizeof(uint32_t))
#define METRICS_TYPE_SERIES_U8          METRICS_TYPE(1, 0, sizeof(uint8_t))
#define METRICS_TYPE_SERIES_U16         METRICS_TYPE(1, 0, sizeof(uint16_t))
#define METRICS_TYPE_SERIES_U32         METRICS_TYPE(1, 0, sizeof(uint32_t))
#define METRICS_TYPE_SERIES_S8          METRICS_TYPE(1, 1, sizeof(uint8_t))
#define METRICS_TYPE_SERIES_S16         METRICS_TYPE(1, 1, sizeof(uint16_t))
#define METRICS_TYPE_SERIES_S32         METRICS_TYPE(1, 1, sizeof(uint32_t))
#define METRICS_TYPE_SINGLE             (METRICS_TYPE_SINGLE_U)
#define METRICS_TYPE_SERIES             (METRICS_TYPE_SERIES_U32)

/* Metric definition - use METRICS_SECT_* helpers to create */
struct metrics_metric_def {
    const char *name;
    uint8_t type;
};

/* Event header - use EVENT_DECLARE() helpers to create */
struct metrics_event_hdr {
    const char *name;
    struct log *log;
    int log_module;
    int log_level;
    uint32_t timestamp;
    uint32_t enabled;
    uint32_t set;
    uint8_t count;
    STAILQ_ENTRY(metrics_event_hdr) next;
    const struct metrics_metric_def *defs;
};

/*
 * Helpers to define metrics section
 *
 * Metrics section defines set of metrics which can be used by an event.
 */
#define METRICS_SECT_START(_metrics) \
    static const struct metrics_metric_def _metrics[] = {
#define METRICS_SECT_ENTRY(_name, _type) \
        { .name = #_name, .type = _type },
#define METRICS_SECT_END \
    }

/**
 * Helper to count number of defined metrics in section
 *
 * @param _metrics  Name of metrics section
 */
#define METRICS_SECT_COUNT(_metrics) \
    sizeof(_metrics) / sizeof(_metrics[0])

/**
 * Helper to declare struct type for event definition
 *
 * This macro should be used to declare a named struct type which can hold event
 * data for specified metrics. New struct type can be then used to create event
 * variable. Each struct has `hdr` field which shall be passed to other APIs
 * wherever event needs to be passed.
 *
 * @param _event    Event structure type name
 * @param _metrics  Metrics definitions (metrics section name)
 */
#define METRICS_EVENT_DECLARE(_event, _metrics) \
    struct _event { \
        struct metrics_event_hdr hdr; \
        uintptr_t vals[ METRICS_SECT_COUNT(_metrics) ]; \
    };

/**
 * Initialize event
 *
 * This shall be called to initialize each event variable before it can be used
 * with other macros.
 *
 * @param hdr      Event header
 * @param metrics  Metrics definitions (metrics section name)
 * @param count    Number of metrics definition
 * @param name     Printable event name
 *
 * @return 0 on success, SYS_E[...] error otherwise
 */
int metrics_event_init(struct metrics_event_hdr *hdr,
                       const struct metrics_metric_def *metrics, uint8_t count,
                       const char *name);

/**
 * Register event
 *
 * This can be used to register event in system. It is optional to register an
 * event, however only registered events can be accessed via e.g. shell.
 *
 * @param hdr  Event header
 *
 * @return 0 on success, SYS_E[...] error otherwise
 */
int metrics_event_register(struct metrics_event_hdr *hdr);

/**
 * Set log instance for an event
 *
 * Set log instance for existing event. Data collected in an event are
 * automatically sent to this log instance when even is done. Log entry is
 * created with specified module and level.
 *
 * @param hdr     Event header
 * @param log     Log instance
 * @param module  Log module
 * @param level   Log level
 *
 * @return 0 on success, SYS_E[...] error otherwise
 *
 * @sa log_append
 */
int metrics_event_set_log(struct metrics_event_hdr *hdr, struct log *log,
                          int module, int level);

/**
 * Start new event
 *
 * Start collecting new data for an event. Any data previously collected in this
 * event are first logged (if log instance is set) and then reset.
 *
 * @param hdr        Event header
 * @param timestamp  Event timestamp
 *
 * @return 0 on success, SYS_E[...] error otherwise
 */
int metrics_event_start(struct metrics_event_hdr *hdr, uint32_t timestamp);

/**
 * End an event
 *
 * End an event by logging collected data to log instance (if log instance is
 * set) and then reset all data. This is also done automatically when event is
 * started and there are data collected already, i.e. there was not explicit
 * call to event_metric_end().
 *
 * @param hdr        Event header
 *
 * @return 0 on success, SYS_E[...] error otherwise.
 */
int metrics_event_end(struct metrics_event_hdr *hdr);

/**
 * Set metric data collection state
 *
 * This can be used to enable or disable data collection for given metric. When
 * data collection for metric is disabled, any subsequent data for this metric
 * are ignored. If disabled metric already has some data collected, these data
 * remain unaffected.
 *
 * @param hdr     Event header
 * @param metric  Metric identifier
 * @param state   New state for metric data collection
 *
 * @return 0 on success, SYS_E[...] error otherwise
 */
int metrics_set_state(struct metrics_event_hdr *hdr, uint8_t metric,
                      bool state);

/**
 * Enable metric data collection state
 *
 * This can be used to enable data collection for multiple metrics at once.
 * Affected metrics are specified by bitmask where each bit number corresponds
 * to metric identifier.
 *
 * @param hdr   Event header
 * @param mask  Metrics bitmask
 *
 * @return 0 on success, SYS_E[...] error otherwise
 */
int metrics_set_state_mask(struct metrics_event_hdr *hdr, uint32_t mask);

/**
 * Disable metric data collection state
 *
 * This can be used to disable data collection for multiple metrics at once.
 * Affected metrics are specified by bitmask where each bit number corresponds
 * to metric identifier.
 *
 * @param hdr   Event header
 * @param mask  Metrics bitmask
 *
 * @return 0 on success, SYS_E[...] error otherwise
 */
int metrics_clr_state_mask(struct metrics_event_hdr *hdr, uint32_t mask);
/**
 * Get metric data collection state
 *
 * Returns current state of metrics collection state as set by
 * metrics_set_state_mask() and metrics_clr_state_mask().
 *
 * @param hdr   Event header
 *
 * @return metrics bitmask
 */
uint32_t metrics_get_state_mask(struct metrics_event_hdr *hdr);

/**
 * Set metric value
 *
 * This can be used to set new value for a metric in an event.
 * Depending on metric type, new value either overwrites current value or is
 * appended to existing values in series.
 * Value can be truncated if is outside of range for specified metric type.
 *
 * @param hdr     Event header
 * @param metric  Metric identifier
 * @param val     Metric value
 *
 * @return 0 on success, SYS_E[...] error otherwise
 */
int metrics_set_value(struct metrics_event_hdr *hdr, uint8_t metric,
                      uint32_t val);

/**
 * Set metric value
 *
 * This can be used to set new value for a metric in an event.
 * Metric type shall be 'single', otherwise this function fails.
 *
 * @param hdr     Event header
 * @param metric  Metric identifier
 * @param val     Metric value
 *
 * @return 0 on success, SYS_E[...] error otherwise
 */
int metrics_set_single_value(struct metrics_event_hdr *hdr, uint8_t metric,
                             uint32_t val);

/**
 * Set metric value
 *
 * This can be used to set new value for a metric in an event.
 * Metric type shall be 'series', otherwise this function fails.
 * Value can be truncated if is outside of range for specified metric type.
 *
 * @param hdr     Event header
 * @param metric  Metric identifier
 * @param val     Metric value
 *
 * @return 0 on success, SYS_E[...] error otherwise
 */
int metrics_set_series_value(struct metrics_event_hdr *hdr, uint8_t metric,
                             uint32_t val);

/**
 * Serialize event data to CBOR
 *
 * Serialize event data to os_mbuf using CBOR format. Only currently enabled
 * metrics will be included in output. Metrics which do not have data set will
 * have value set to 'undefined'.
 *
 * Data collected in an event remain unaffected. The only exception is when
 * destination mbuf is allocated using event_metric_get_mbuf() - data are then
 * removed from event during serialization to reduce memory usage and free space
 * for CBOR stream. This way of calling should thus be used only if all data for
 * an event were already collected.
 *
 * @param hdr  Event header
 * @param om   Target mbuf
 *
 * @return 0 on success, SYS_E[...] error otherwise
 */
int metrics_event_to_cbor(struct metrics_event_hdr *hdr, struct os_mbuf *om);

/**
 * Get mbuf from metrics internal pool
 *
 * Internal pool can be used when serializing data to CBOR format to reduce
 * memory usage since pool is then shared between event and CBOR stream. See
 * event_metric_to_cbor() for details.
 *
 * @return mbuf
 */
struct os_mbuf *metrics_get_mbuf(void);

#ifdef __cplusplus
}
#endif

#endif /* __SYS_METRICS_H__ */
