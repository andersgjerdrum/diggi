
#include "posix/syscall_def.h"

#include <stdint.h>
#include "posix/io_types.h"
#include <netinet/in.h>
#ifdef __cplusplus
extern "C" {
#endif
uint32_t i_htonl(uint32_t hostlong);
uint16_t i_htons(uint16_t hostshort);
uint32_t i_ntohl(uint32_t netlong);

uint16_t i_ntohs(uint16_t netshort);
int i_inet_aton(const char *cp, struct in_addr *inp);

in_addr_t i_inet_addr(const char *cp);

in_addr_t i_inet_network(const char *cp);

char *i_inet_ntoa(struct in_addr in);

struct in_addr i_inet_makeaddr(int net, int host);

in_addr_t i_inet_lnaof(struct in_addr in);

in_addr_t i_inet_netof(struct in_addr in);

const char *i_inet_ntop(int af, const void *src, char *dst, socklen_t size);

#ifdef __cplusplus
}
#endif // __cplusplus
