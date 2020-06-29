#ifndef I_NETWORKMANAGER_H
#define I_NETWORKMANAGER_H

class INetworkManager{
    public:

    INetworkManager() {};
    virtual ~INetworkManager() {};

    virtual void AsyncSocket(int domain, int type, int protocol, async_cb_t cb, void * context) = 0;
    virtual void AsyncBind(int sockfd, const struct sockaddr *addr, socklen_t addrlen, async_cb_t cb, void * context)  = 0;
    virtual void AsyncGetsockname(int sockfd, const struct sockaddr *addr, socklen_t *addrlen, async_cb_t cb, void * context)  = 0;
    virtual void AsyncGetsockopt(int sockfd, int level, int optname, socklen_t *optlen, async_cb_t cb, void *context) = 0;
    virtual void AsyncConnect(int sockfd, const struct sockaddr *addr, socklen_t addrlen, async_cb_t cb, void *context) = 0;
    virtual void AsyncFcntl(int sockfd, int cmd, int flag, async_cb_t cb, void *context) = 0;
    virtual void AsyncSetsockopt(int sockfd, int level, int optname, const void* optval, socklen_t optlen, async_cb_t cb, void *context) = 0;
    virtual void AsyncListen(int sockfd, int backlog, async_cb_t cb, void *context) = 0;
    virtual void AsyncSelect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout, async_cb_t cb, void *context) =0;
    virtual void AsyncRecv(int sockfd, void* buf, size_t len, int flags, async_cb_t cb, void *context) = 0;
    virtual void AsyncSend(int sockfd, const void* buf, size_t len, int flags, async_cb_t cb, void *context) = 0;
    virtual void AsyncAccept(int sockfd, const struct sockaddr *addr, socklen_t *addrlen, async_cb_t cb, void *context) = 0; 
    virtual void AsyncClose(int sockfd, async_cb_t cb, void *context) = 0;

    virtual void AsyncRand(async_cb_t cb, void *context) = 0;
       
};

#endif 