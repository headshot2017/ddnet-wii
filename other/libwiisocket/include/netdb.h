#pragma once
#include <sys/socket.h>

#define HOST_NOT_FOUND  1
#define TRY_AGAIN       2
#define NO_RECOVERY     3
#define NO_DATA         4

#define NI_MAXHOST      256
#define NI_MAXSERV      32

#define NI_NOFQDN       0x01
#define NI_NUMERICHOST  0x02
#define NI_NAMEREQD     0x04
#define NI_NUMERICSERV  0x08
#define NI_DGRAM        0x10
#define NI_NUMERICSCOPE 0x20

#define AI_PASSIVE      0x01
#define AI_CANONNAME    0x02
#define AI_NUMERICHOST  0x04
#define AI_NUMERICSERV  0x08
#define AI_V4MAPPED     0x10
#define AI_ALL          0x20
#define AI_ADDRCONFIG   0x40 // stub/nonexistent
#define AI_DEFAULT      (AI_V4MAPPED | AI_ADDRCONFIG)

#define EAI_FAIL        (-0x12E)
#define EAI_FAMILY      (-0x12F)
#define EAI_MEMORY      (-0x130)
#define EAI_NONAME      (-0x131)
#define EAI_SERVICE     (-0x132)
#define EAI_SOCKTYPE    (-0x133)
#define EAI_SYSTEM      (-0x134)
#define EAI_AGAIN       (-1) // stub/nonexistent
#define EAI_BADFLAGS    (-2) // stub/nonexistent
#define EAI_OVERFLOW    (-3) // stub/nonexistent


struct addrinfo
{
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    socklen_t ai_addrlen;
    char *ai_canonname;
    void *ai_addr;
    struct addrinfo *ai_next;
};

struct hostent
{
    char *h_name;     
    char **h_aliases; 
    int h_addrtype;  
    int h_length;    
    char **h_addr_list;
};

#define h_addr h_addr_list[0]

#ifdef __cplusplus
extern "C" {
#endif

extern int
h_errno;

int
getaddrinfo(const char *node,
            const char *service,
            const struct addrinfo *hints,
            struct addrinfo **res);

void
freeaddrinfo(struct addrinfo *res);

struct hostent *
gethostbyaddr(const void *addr,
              socklen_t len,
              int type);

struct hostent *
gethostbyname(const char *name);

struct hostent *
gethostent(void);

struct servent *
getservbyname(const char *name,
              const char *proto);

struct servent *
getservbyport(int port,
              const char *proto);

struct servent *
getservent(void);

int
getnameinfo(const struct sockaddr *addr,
            socklen_t addrlen,
            char *host,
            socklen_t hostlen,
            char *serv,
            socklen_t servlen,
            int flags);

const char *
gai_strerror(int errnum);

#ifdef __cplusplus
}
#endif
