#ifndef LWIP_PING_H
#define LWIP_PING_H

#include "lwip/ip_addr.h"

/**
 * PING_USE_SOCKETS: Set to 1 to use sockets, otherwise the raw api is used
 */
#ifndef PING_USE_SOCKETS
#define PING_USE_SOCKETS    LWIP_SOCKET
#endif

typedef void (ping_send_cb)(int seq_num);
typedef void (ping_recv_cb)(int seq_num, int delay);

void ping_set_cbs(ping_send_cb *send, ping_recv_cb *rcv);

void ping_init(const ip_addr_t* ping_addr, int ping_cnt, int ping_size, int ping_delay);

#if !PING_USE_SOCKETS
void ping_send_now(void);
#endif /* !PING_USE_SOCKETS */

#endif /* LWIP_PING_H */