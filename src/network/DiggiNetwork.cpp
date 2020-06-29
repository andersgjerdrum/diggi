/**
 * @file diggi_network.cpp
 * @author your name (you@domain.com)
 * @brief implementation of socket interface, simplifying configuration, network interface discovery and error handling.
 * This class is exclusively used by connection.cpp
 * @see Connection::Connection
 * @version 0.1
 * @date 2020-02-02
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#include "network/DiggiNetwork.h"

/**
 * @brief send operation, may execute both blocking and non blocking.
 * 
 * @param ctx connection context, may hold TLS info if setup
 * @param buf write buffer
 * @param len write length in bytes
 * @return int returns DIGGI_NETWORK_WOULD_BLOCK if send buffer is full. retry operation.
 */
int SimpleTCP::send__(tcp_connection_context_t *ctx, const unsigned char *buf, size_t len)
{
    int ret;
    int fd = ((tcp_connection_context_t *)ctx)->fd;

    if (fd < 0)
        return (-1);

    ret = (int)write(fd, buf, len);

    if (ret < 0)
    {
        if (net_would_block(ctx) != 0)
            return (DIGGI_NETWORK_WOULD_BLOCK);
        if (errno == EPIPE || errno == ECONNRESET)
            return (-1);

        if (errno == EINTR)
            return (DIGGI_NETWORK_WOULD_BLOCK);
        return (-1);
    }

    return (ret);
}
/**
 * @brief int interface, noop for plain TCP, TLS requires init operation.
 * 
 * @param ctx 
 */
void SimpleTCP::init__(tcp_connection_context_t *ctx)
{
}

/**
 * @brief recieve call reads "len" bytes from socket, support both blocking and non-blocking.
 * 
 * @param ctx  Connection context
 * @param buf read buffer
 * @param len length of read in bytes.
 * @return int return DIGGI_NETWORK_WOULD_BLOCK if input buffer is unavailible. retry operation.
 */
int SimpleTCP::recv__(tcp_connection_context_t *ctx, unsigned char *buf, size_t len)
{
    int ret;
    int fd = ((tcp_connection_context_t *)ctx)->fd;

    if (fd < 0)
        return (-1);

    ret = (int)read(fd, buf, len);

    if (ret < 0)
    {
        if (net_would_block((tcp_connection_context_t *)ctx) != 0)
            return (DIGGI_NETWORK_WOULD_BLOCK);
        if (errno == EPIPE || errno == ECONNRESET)
            return (-1);

        if (errno == EINTR)
            return (DIGGI_NETWORK_WOULD_BLOCK);
        return (-1);
    }
    return (ret);
}
/**
 * @brief connect to a server with host:port
 * host may be DNS name or ip address
 * 
 * @param ctx connection context object
 * @param host host name, DNS or IP
 * @param port port number
 * @return int success flagg
 */
int SimpleTCP::connect__(tcp_connection_context_t *ctx, const char *host, const char *port)
{
    int ret;
    struct addrinfo hints, *addr_list, *cur;

    /* Do name resolution with both IPv6 and IPv4 */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(host, port, &hints, &addr_list) != 0)
        return (-1);

    /* Try the sockaddrs until a connection succeeds */
    ret = -1;
    for (cur = addr_list; cur != NULL; cur = cur->ai_next)
    {
        ctx->fd = (int)socket(cur->ai_family, cur->ai_socktype,
                              cur->ai_protocol);
        if (ctx->fd < 0)
        {
            ret = -1;
            continue;
        }

        if (connect(ctx->fd, cur->ai_addr, (int)cur->ai_addrlen) == 0)
        {
            ret = 0;
            break;
        }

        close(ctx->fd);
        ret = -1;
    }

    freeaddrinfo(addr_list);
    int flag = 1;
    int result = setsockopt(ctx->fd,       /* socket affected */
                            IPPROTO_TCP,   /* set option at TCP level */
                            TCP_NODELAY,   /* name of option */
                            (char *)&flag, /* the cast is historical cruft */
                            sizeof(int));  /* length of option value */
    DIGGI_ASSERT(result >= 0);
    size_t sendbuff = DIGGI_MAX_TCP_SEND_BUFFER;
    result = setsockopt(ctx->fd, SOL_SOCKET, SO_SNDBUF, &sendbuff, sizeof(sendbuff));
    DIGGI_ASSERT(result >= 0);
    return (ret);
}
/**
 * @brief internally used method for determining if a socket will block
 * 
 * @param ctx 
 * @return int 
 */
int SimpleTCP::net_would_block(const tcp_connection_context_t *ctx)
{
    /*
	* Never return 'WOULD BLOCK' on a non-blocking socket
	*/
    if ((fcntl(ctx->fd, F_GETFL) & O_NONBLOCK) != O_NONBLOCK)
        return (0);

    switch (errno)
    {
#if defined EAGAIN
    case EAGAIN:
#endif
#if defined EWOULDBLOCK && EWOULDBLOCK != EAGAIN
    case EWOULDBLOCK:
#endif
        return (1);
    }
    return (0);
}
/**
 * @brief accept incomming connection request from listen socket. 
 * Returns a new client connection context object associated with a new socket, and ip of client.
 * 
 * @param bind_ctx 
 * @param client_ctx 
 * @param client_ip 
 * @param buf_size 
 * @param ip_len 
 * @return int return DIGGI_NETWORK_WOULD_BLOCK if resulting in blocking operation, must be retried.
 */
int SimpleTCP::accept__(tcp_connection_context_t *bind_ctx,
                        tcp_connection_context_t *client_ctx,
                        void *client_ip, size_t buf_size, size_t *ip_len)
{
    int ret;
    int type;

    struct sockaddr_storage client_addr;

    socklen_t n = (socklen_t)sizeof(client_addr);
    socklen_t type_len = (socklen_t)sizeof(type);

    /* Is this a TCP or UDP socket? */
    if (getsockopt(bind_ctx->fd, SOL_SOCKET, SO_TYPE,
                   (void *)&type, &type_len) != 0 ||
        (type != SOCK_STREAM && type != SOCK_DGRAM))
    {
        return (-1);
    }

    if (type == SOCK_STREAM)
    {
        /* TCP: actual accept() */
        ret = client_ctx->fd = (int)accept(bind_ctx->fd,
                                           (struct sockaddr *)&client_addr, &n);
    }
    else
    {
        return (-1);
    }

    if (ret < 0)
    {
        if (net_would_block(bind_ctx) != 0)
            return (DIGGI_NETWORK_WOULD_BLOCK);

        return (-1);
    }

    /* UDP: hijack the listening socket to communicate with the client,
	* then bind a new socket to accept new connections */
    if (type != SOCK_STREAM)
    {
        struct sockaddr_storage local_addr;
        int one = 1;

        if (connect(bind_ctx->fd, (struct sockaddr *)&client_addr, n) != 0)
            return (-1);

        client_ctx->fd = bind_ctx->fd;
        bind_ctx->fd = -1; /* In case we exit early */

        n = sizeof(struct sockaddr_storage);
        if (getsockname(client_ctx->fd,
                        (struct sockaddr *)&local_addr, &n) != 0 ||
            (bind_ctx->fd = (int)socket(local_addr.ss_family,
                                        SOCK_DGRAM, IPPROTO_UDP)) < 0 ||
            setsockopt(bind_ctx->fd, SOL_SOCKET, SO_REUSEADDR,
                       (const char *)&one, sizeof(one)) != 0)
        {
            return (-1);
        }

        if (bind(bind_ctx->fd, (struct sockaddr *)&local_addr, n) != 0)
        {
            return (-1);
        }
    }

    if (client_ip != NULL)
    {
        if (client_addr.ss_family == AF_INET)
        {
            struct sockaddr_in *addr4 = (struct sockaddr_in *)&client_addr;
            *ip_len = sizeof(addr4->sin_addr.s_addr);

            if (buf_size < *ip_len)
                return (-1);

            memcpy(client_ip, &addr4->sin_addr.s_addr, *ip_len);
        }
        else
        {
            return (-1);
        }
    }
    int flag = 1;
    int result = setsockopt(client_ctx->fd, /* socket affected */
                            IPPROTO_TCP,    /* set option at TCP level */
                            TCP_NODELAY,    /* name of option */
                            (char *)&flag,  /* the cast is historical cruft */
                            sizeof(int));   /* length of option value */
    DIGGI_ASSERT(result >= 0);
    size_t sendbuff = DIGGI_MAX_TCP_SEND_BUFFER;
    result = setsockopt(client_ctx->fd, SOL_SOCKET, SO_SNDBUF, &sendbuff, sizeof(sendbuff));
    DIGGI_ASSERT(result >= 0);
    return (0);
}
/*done*/

/**
 * @brief bind listen socket to a given network interface.
 * if no ip is provided, bind searches network interfaces for eligible candidate.
 * @param ctx 
 * @param bind_ip 
 * @param port 
 * @return int 
 */
int SimpleTCP::bind__(tcp_connection_context_t *ctx, const char *bind_ip, const char *port)
{
    int n, ret;
    struct addrinfo hints, *addr_list, *cur;

    /* Bind to IPv6 and/or IPv4, but only in the desired protocol */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    if (bind_ip == NULL)
        hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(bind_ip, port, &hints, &addr_list) != 0)
        return (-1);

    /* Try the sockaddrs until a binding succeeds */
    ret = -1;
    for (cur = addr_list; cur != NULL; cur = cur->ai_next)
    {
        ctx->fd = (int)socket(cur->ai_family, cur->ai_socktype,
                              cur->ai_protocol);
        if (ctx->fd < 0)
        {
            ret = -1;
            continue;
        }

        n = 1;
        if (setsockopt(ctx->fd, SOL_SOCKET, SO_REUSEADDR,
                       (const char *)&n, sizeof(n)) != 0)
        {
            close(ctx->fd);
            ret = -1;
            continue;
        }

        if (bind(ctx->fd, cur->ai_addr, (int)cur->ai_addrlen) != 0)
        {
            close(ctx->fd);
            ret = -1;
            continue;
        }

        /* Listen only makes sense for TCP */
        if (listen(ctx->fd, 10) != 0)
        {
            close(ctx->fd);
            ret = -1;
            continue;
        }

        /* I we ever get there, it's a success */
        ret = 0;
        break;
    }

    freeaddrinfo(addr_list);

    return (ret);
}
/**
 * @brief free connection context, close file descriptor.
 * 
 * @param ctx 
 */
void SimpleTCP::free__(tcp_connection_context_t *ctx)
{
    close(ctx->fd);
}
/**
 * @brief set connection context to non blocking mode.
 * 
 * @param ctx 
 * @return int 
 */
int SimpleTCP::set_nonblock__(tcp_connection_context_t *ctx)
{
    return (fcntl(ctx->fd, F_SETFL, fcntl(ctx->fd, F_GETFL) | O_NONBLOCK));
}
/**
 * @brief set connection context to blocking mode.
 * 
 * @param ctx 
 * @return int 
 */
int SimpleTCP::set_block__(tcp_connection_context_t *ctx)
{
    return (fcntl(ctx->fd, F_SETFL, fcntl(ctx->fd, F_GETFL) & ~O_NONBLOCK));
}