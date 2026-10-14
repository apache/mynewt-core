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

/*
 * coreMQTT configuration for Mynewt. This file is included by coreMQTT
 * through core_mqtt_config_defaults.h; settings come from syscfg.
 */

#ifndef H_CORE_MQTT_CONFIG_
#define H_CORE_MQTT_CONFIG_

#include "os/mynewt.h"
#include "modlog/modlog.h"
#include "coremqtt/coremqtt.h"

#define MQTT_SUB_UNSUB_MAX_VECTORS MYNEWT_VAL(COREMQTT_SUB_UNSUB_MAX_VECTORS)
#define MQTT_MAX_CONNACK_RECEIVE_RETRY_COUNT                                  \
    MYNEWT_VAL(COREMQTT_MAX_CONNACK_RECEIVE_RETRY_COUNT)
#define MQTT_PINGRESP_TIMEOUT_MS MYNEWT_VAL(COREMQTT_PINGRESP_TIMEOUT_MS)
#define PACKET_TX_TIMEOUT_MS     MYNEWT_VAL(COREMQTT_PACKET_TX_TIMEOUT_MS)
#define PACKET_RX_TIMEOUT_MS     MYNEWT_VAL(COREMQTT_PACKET_RX_TIMEOUT_MS)
#define MQTT_RECV_POLLING_TIMEOUT_MS                                          \
    MYNEWT_VAL(COREMQTT_RECV_POLLING_TIMEOUT_MS)
#define MQTT_SEND_TIMEOUT_MS MYNEWT_VAL(COREMQTT_SEND_TIMEOUT_MS)

/*
 * coreMQTT log macros take a single parenthesized argument list, e.g.
 * LogError(("fmt %d", x)). Levels below the configured minimum compile to
 * nothing (default definitions from core_mqtt_config_defaults.h).
 */
#define COREMQTT_LOG_ENABLED(lvl_)                                            \
    (MYNEWT_VAL(COREMQTT_LOG_LVL) <= (lvl_) && MYNEWT_VAL(LOG_LEVEL) <= (lvl_))

#if COREMQTT_LOG_ENABLED(LOG_LEVEL_ERROR)
#define COREMQTT_LOG_ERROR_(...) coremqtt_log(LOG_LEVEL_ERROR, __VA_ARGS__)
#define LogError(message)        COREMQTT_LOG_ERROR_ message
#endif

#if COREMQTT_LOG_ENABLED(LOG_LEVEL_WARN)
#define COREMQTT_LOG_WARN_(...) coremqtt_log(LOG_LEVEL_WARN, __VA_ARGS__)
#define LogWarn(message)        COREMQTT_LOG_WARN_ message
#endif

#if COREMQTT_LOG_ENABLED(LOG_LEVEL_INFO)
#define COREMQTT_LOG_INFO_(...) coremqtt_log(LOG_LEVEL_INFO, __VA_ARGS__)
#define LogInfo(message)        COREMQTT_LOG_INFO_ message
#endif

#if COREMQTT_LOG_ENABLED(LOG_LEVEL_DEBUG)
#define COREMQTT_LOG_DEBUG_(...) coremqtt_log(LOG_LEVEL_DEBUG, __VA_ARGS__)
#define LogDebug(message)        COREMQTT_LOG_DEBUG_ message
#endif

#endif
