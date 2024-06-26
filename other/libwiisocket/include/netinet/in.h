#pragma once
#include <stdint.h>
#include <sys/socket.h>

#define INADDR_ANY          0x00000000
#define INADDR_LOOPBACK     0x7f000001
#define INADDR_BROADCAST    0xffffffff
#define INADDR_NONE         0xffffffff

#define INET_ADDRSTRLEN     16

#define IPPROTO_IP          0
#define IPPROTO_ICMP        1
#define IPPROTO_TCP         6
#define IPPROTO_UDP         17

#define IP_TOS              7
#define IP_TTL              8


typedef uint16_t in_port_t;
typedef uint32_t in_addr_t;

struct in_addr {
	in_addr_t       s_addr;
};

struct sockaddr_in {
    unsigned char sin_len;
    sa_family_t sin_family;
    in_port_t sin_port;
    struct in_addr sin_addr;
};
