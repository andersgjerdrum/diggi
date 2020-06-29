#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include "Seal.h"
#include "runtime/DiggiAPI.h"
#include "network/INetworkManager.h"
#include "sys/socket.h"
#include "messaging/Pack.h"

class NetworkManager : public INetworkManager {
    private:
        IDiggiAPI *aDiggiAPI;
        ISealingAlgorithm *aSealer;

    public:
        NetworkManager(IDiggiAPI *context, ISealingAlgorithm *seal);
        ~NetworkManager();

        void AsyncSocket(int domain, int type, int protocol, async_cb_t cb, void *context);
        void AsyncBind(int sockfd, const struct sockaddr *addr, socklen_t addrlen, async_cb_t cb, void *context);
        void AsyncGetsockname(int sockfd, const struct sockaddr *addr, socklen_t *addrlen, async_cb_t cb, void *context);
        void AsyncGetsockopt(int sockfd, int level, int optname, socklen_t *optlen, async_cb_t cb, void *context);
        void AsyncSetsockopt(int sockfd, int level, int optname, const void* optval, socklen_t optlen, async_cb_t cb, void *context);
        void AsyncConnect(int sockfd, const struct sockaddr *addr, socklen_t addrlen, async_cb_t cb, void *context);
        void AsyncFcntl(int sockfd, int cmd, int flag, async_cb_t cb, void *context);
        void AsyncListen(int sockfd, int backlog, async_cb_t cb, void *context);
        void AsyncSelect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout, async_cb_t cb, void *context);
        void AsyncRecv(int sockfd, void* buf, size_t len, int flags, async_cb_t cb, void *context);
        void AsyncSend(int sockfd, const void* buf, size_t len, int flags, async_cb_t cb, void *context);
        void AsyncAccept(int sockfd, const struct sockaddr *addr, socklen_t *addrlen, async_cb_t cb, void *context); 
        void AsyncClose(int sockfd, async_cb_t cb, void *context); 
        void AsyncRand(async_cb_t cb, void *context);
        

};

#endif