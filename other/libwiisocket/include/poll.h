#pragma once

#define POLLRDNORM  0x0001
#define POLLRDBAND  0x0002
#define POLLPRI     0x0004
#define POLLWRNORM  0x0008
#define POLLWRBAND  0x0010
#define POLLERR     0x0020
#define POLLHUP     0x0040
#define POLLNVAL    0x0080
#define POLLIN      (POLLRDNORM | POLLRDBAND)
#define POLLOUT     POLLWRNORM

typedef unsigned long nfds_t;

struct pollfd
{
   int fd;
   int events;
   int revents;
};

#ifdef __cplusplus
extern "C" {
#endif

int
poll(struct pollfd *fds,
     nfds_t nfds,
     int timeout);

#ifdef __cplusplus
}
#endif

