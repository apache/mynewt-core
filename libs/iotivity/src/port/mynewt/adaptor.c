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
#include <assert.h>
#include <os/os.h>
#include <os/endian.h>
#include <string.h>
#include <log/log.h>
#include "../oc_network_events_mutex.h"
#include "../oc_connectivity.h"
#include "oc_buffer.h"
#include "../oc_log.h"
#include "adaptor.h"

struct os_eventq oc_event_q;

/* not sure if these semaphores are necessary yet.  If we are running
 * all of this from one task, we may not need these */
static struct os_mutex oc_net_mutex;

void
oc_network_event_handler_mutex_init(void)
{
    os_error_t rc;
    rc = os_mutex_init(&oc_net_mutex);
    assert(rc == 0);
}

void
oc_network_event_handler_mutex_lock(void)
{
    os_mutex_pend(&oc_net_mutex, OS_TIMEOUT_NEVER);
}

void
oc_network_event_handler_mutex_unlock(void)
{
    os_mutex_release(&oc_net_mutex);
}

/* need a task to process OCF messages */
#define OC_NET_TASK_STACK_SIZE          OS_STACK_ALIGN(300)
#define OC_NET_TASK_PRIORITY            (4)
struct os_task oc_task;
os_stack_t *oc_stack;

void
oc_send_buffer(oc_message_t *message) {

    switch (message->endpoint.flags)
    {
#ifdef OC_TRANSPORT_IP
        case IP:
            oc_send_buffer_ip(message);
            break;
#endif
#ifdef OC_TRANSPORT_GATT
        case GATT:
            oc_send_buffer_gatt(message);
            break;
#endif
#ifdef OC_TRANSPORT_SERIAL
        case SERIAL:
            oc_send_buffer_serial(message);
            break;
#endif
        default:
            ERROR("Unknown transport option %u\n", message->endpoint.flags);
            oc_message_unref(message);
    }
}

void oc_send_multicast_message(oc_message_t *message)
{

    /* send on all the transports.  Don't forget to reference the message
     * so it doesn't get deleted  */

#ifdef OC_TRANSPORT_IP
    oc_send_buffer_ip_mcast(message);
#endif

#ifdef OC_TRANSPORT_GATT
    /* no multicast for GATT, just send unicast */
    oc_message_add_ref(message);
    oc_send_buffer_gatt(message);
#endif

#ifdef OC_TRANSPORT_SERIAL
    /* no multi-cast for serial.  just send unicast */
    oc_message_add_ref(message);
    oc_send_buffer_serial(message);
#endif
}

/* send all the entries to the OCF stack through the same task */
void
oc_task_handler(void *arg) {

#ifdef OC_TRANSPORT_GATT
    oc_connectivity_start_gatt();
#endif

    while (1) {
        struct os_callout_func *cf;
        oc_message_t *pmsg;
        (void) pmsg;    /* to avoid unused */
        struct os_event *evt = os_eventq_get(&oc_event_q);

        switch(evt->ev_type) {
#ifdef OC_TRANSPORT_IP
            case OC_ADATOR_EVENT_IP:
                while ((pmsg = oc_attempt_rx_ip()) != NULL) {
                    oc_network_event(pmsg);
                }
                break;
#endif
#ifdef OC_TRANSPORT_SERIAL
            case OC_ADATOR_EVENT_SERIAL:
                while ((pmsg = oc_attempt_rx_serial()) != NULL) {
                    oc_network_event(pmsg);
                }
                break;
#endif
#ifdef OC_TRANSPORT_GATT
            case OC_ADATOR_EVENT_GATT:
                while ((pmsg = oc_attempt_rx_gatt()) != NULL) {
                    oc_network_event(pmsg);
                }
                break;
        case OS_EVENT_T_TIMER:
            cf = (struct os_callout_func *)evt;
            assert(cf->cf_func);
            cf->cf_func(CF_ARG(cf));
            break;
#endif
            default:
                ERROR("oc_task_handler: Unidentified event %d\n", evt->ev_type);
        }
    }
}

static int
oc_init_task(void) {
    int rc;

    os_eventq_init(&oc_event_q);

    oc_stack = (os_stack_t*) malloc(sizeof(os_stack_t)*OC_NET_TASK_STACK_SIZE);
    if (NULL == oc_stack) {
        ERROR("Could not malloc oc stack\n");
        return -1;
    }

    rc = os_task_init(&oc_task, "oc", oc_task_handler, NULL,
            OC_NET_TASK_PRIORITY, OS_WAIT_FOREVER,
            oc_stack, OC_NET_TASK_STACK_SIZE);

    if (rc != 0) {
        ERROR("Could not start oc task\n");
        free(oc_stack);
    }

    return rc;
}

void
oc_connectivity_shutdown(void)
{
#ifdef OC_TRANSPORT_IP
    oc_connectivity_shutdown_ip();
#endif
#ifdef OC_TRANSPORT_SERIAL
    oc_connectivity_shutdown_serial();
#endif
#ifdef OC_TRANSPORT_GATT
    oc_connectivity_shutdown_gatt();
#endif
}

int
oc_connectivity_init(void)
{
    int rc;

#ifdef OC_TRANSPORT_IP
    rc = oc_connectivity_init_ip();
    if (rc != 0) {
        goto oc_connectivity_init_err;
    }
#endif
#ifdef OC_TRANSPORT_SERIAL
    rc = oc_connectivity_init_serial();
    if (rc != 0) {
        goto oc_connectivity_init_err;
    }
#endif
#ifdef OC_TRANSPORT_GATT
    rc = oc_connectivity_init_gatt();
    if (rc != 0) {
        goto oc_connectivity_init_err;
    }
#endif

    rc = oc_init_task();
    if (rc != 0) {
        goto oc_connectivity_init_err;
    }

    return 0;

oc_connectivity_init_err:
    oc_connectivity_shutdown();
    return rc;
}