#pragma once
#include <stdint.h>
#include <sys/time.h>

#define SOL_SOCKET      0xffff

#define PF_UNSPEC       0
#define PF_INET         2

#define AF_UNSPEC       PF_UNSPEC
#define AF_INET         PF_INET

#define SOCK_STREAM     1
#define SOCK_DGRAM      2

#define MSG_OOB         0x0001
#define MSG_PEEK        0x0002
#define MSG_DONTWAIT    0x0004

#define SHUT_RD         0
#define SHUT_WR         1
#define SHUT_RDWR       2

#define SO_REUSEADDR    0x0004
#define SO_LINGER       0x0080
#define SO_OOBINLINE    0x0100
#define SO_SNDBUF       0x1001
#define SO_RCVBUF       0x1002
#define SO_SNDLOWAT     0x1003
#define SO_RCVLOWAT     0x1004
#define SO_TYPE         0x1008
#define SO_ERROR        0x1009

#define SO_ACCEPTCONN   0xFFF0 // stub
#define SO_BROADCAST    0xFFF1 // stub
#define SO_DEBUG        0xFFF2 // stub
#define SO_DONTROUTE    0xFFF3 // stub
#define SO_KEEPALIVE    0xFFF4 // stub
#define SO_RCVTIMEO     0xFFF5 // stub
#define SO_SNDTIMEO     0xFFF6 // stub

typedef uint32_t socklen_t;
typedef uint8_t sa_family_t;

struct sockaddr
{
    unsigned char sa_len;
    sa_family_t sa_family;
    char sa_data[6];
};

struct sockaddr_storage
{
    unsigned char ss_len;
    sa_family_t ss_family;
    char ss_padding[26];
};

struct linger
{
   int l_onoff;
   int l_linger;
};

#ifdef __cplusplus
extern "C" {
#endif

int
accept(int sockfd,
       struct sockaddr *addr,
       socklen_t *addrlen);

int
bind(int sockfd,
     const struct sockaddr *addr,
     socklen_t addrlen);

int
connect(int sockfd,
        const struct sockaddr *addr,
        socklen_t addrlen);

int
getpeername(int sockfd,
            struct sockaddr *addr,
            socklen_t *addrlen);

int
getsockname(int sockfd,
            struct sockaddr *addr,
            socklen_t *addrlen);

int
getsockopt(int sockfd,
           int level,
           int optname,
           void *optval,
           socklen_t *optlen);

int
listen(int sockfd,
       int backlog);

ssize_t
recv(int sockfd,
     void *buf,
     size_t len,
     int flags);

ssize_t
recvfrom(int sockfd,
         void *buf,
         size_t len,
         int flags,
         struct sockaddr *src_addr,
         socklen_t *addrlen);

ssize_t
send(int sockfd,
     const void *buf,
     size_t len,
     int flags);

ssize_t
sendto(int sockfd,
       const void *buf,
       size_t len,
       int flags,
       const struct sockaddr *dest_addr,
       socklen_t addrlen);

int
setsockopt(int sockfd,
           int level,
           int optname,
           const void *optval,
           socklen_t optlen);

int
shutdown(int sockfd,
         int how);

int
socket(int domain,
       int type,
       int protocol);

int
sockatmark(int sockfd);

#ifdef __cplusplus
}
#endif
