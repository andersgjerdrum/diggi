#ifndef NET_STUBS_H
#define NET_STUBS_H
#include "posix/syscall_def.h"

#include "posix/io_types.h"


#ifdef __cplusplus
extern "C" {
#endif

#include "datatypes.h"
#include "posix/tlibc/errno.h"
#include "posix/base_stubs.h"
#include <sys/socket.h>

void netstubs_teardown();
void netstubs_setresponse(void *ptr, int status);
void netstub_free_response(msg_t **incoming_msg);

int set_errno(int val);

	int				i_socket(int domain, int type, int protocol);
    void            i_freeaddrinfo(struct addrinfo *res);
    int             i_getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res);
	int				i_setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
	int				i_getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
	ssize_t			i_send(int sockfd, const void *buf, size_t len, int flags);
	ssize_t			i_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);
	ssize_t			i_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);
	int				i_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
	int				i_getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
	ssize_t			i_recv(int sockfd, void *buf, size_t len, int flags);
	int				i_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
	int				i_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
	int				i_listen(int sockfd, int backlog);
	int				i_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
	int				i_getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen);
    int             i_shutdown(int socket, int how);


	int				network_close(int sockfd);
	int 			network_fcntl(int sockfd, int cmd, int flag);

    // moved here from cryptoStubs.h:
    int				i_rand(void);

#ifdef __cplusplus
}
#endif // __cplusplus


#endif
