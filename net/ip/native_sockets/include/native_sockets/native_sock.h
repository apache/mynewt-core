#ifndef H_NATIVE_SOCKETS_
#define H_NATIVE_SOCKETS_

#include <sys/socket.h>

#define MN_AF_LOCAL         255
#define MN_PF_LOCAL         MN_AF_LOCAL

struct mn_sockaddr_un {
    uint8_t msun_len;
    uint8_t msun_family;
    char msun_path[104];
};

#endif
