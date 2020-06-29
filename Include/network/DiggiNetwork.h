#ifndef TCPSTUB_H
#define TCPSTUB_H


#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include "DiggiAssert.h"
#include "posix/net_stubs.h"
#include <string.h>
#include <errno.h>

/*
FUTURE: replaced by proper implementation of exitless tcp
*/
#ifdef __cplusplus
extern "C" {
#endif

static int  negative_diggi_network_w_block_val[[gnu::unused]] = -1234;
#define DIGGI_MAX_TCP_SEND_BUFFER 512 * 1024

#define DIGGI_NETWORK_WOULD_BLOCK negative_diggi_network_w_block_val
typedef struct tcp_connection_context_t
{
	int fd;
    
}tcp_connection_context_t;

#ifdef __cplusplus
}
#endif /* __cplusplus */

class IConnectionPrimitives {

public:
	IConnectionPrimitives() {}
	virtual ~IConnectionPrimitives() {}

	virtual int send__(tcp_connection_context_t *ctx, const unsigned char *buf, size_t len) = 0;

	virtual void init__(tcp_connection_context_t *ctx) = 0;

	virtual int recv__(tcp_connection_context_t *ctx, unsigned char *buf, size_t len) = 0;

	virtual int connect__(tcp_connection_context_t *ctx, const char *host, const char *port) = 0;

	virtual int accept__(tcp_connection_context_t *bind_ctx,
		tcp_connection_context_t *client_ctx,
		void *client_ip, size_t buf_size, size_t *ip_len) = 0;

	virtual int bind__(tcp_connection_context_t *ctx, const char *bind_ip, const char *port) = 0;

	virtual void free__(tcp_connection_context_t *ctx) = 0;

	virtual int set_nonblock__(tcp_connection_context_t *ctx) = 0;

	virtual int set_block__(tcp_connection_context_t *ctx) = 0;

};


class SimpleTCP : public IConnectionPrimitives {
public:
	int send__(tcp_connection_context_t *ctx, const unsigned char *buf, size_t len);
	void init__(tcp_connection_context_t *ctx);
	int recv__(tcp_connection_context_t *ctx, unsigned char *buf, size_t len);
	int connect__(tcp_connection_context_t *ctx, const char *host, const char *port);
    static int net_would_block(const tcp_connection_context_t *ctx);
	int accept__(tcp_connection_context_t *bind_ctx, tcp_connection_context_t *client_ctx, void *client_ip, size_t buf_size, size_t *ip_len);
	int bind__(tcp_connection_context_t *ctx, const char *bind_ip, const char *port);
	void free__(tcp_connection_context_t *ctx);
	int set_nonblock__(tcp_connection_context_t *ctx);
	int set_block__(tcp_connection_context_t *ctx);
};
#endif