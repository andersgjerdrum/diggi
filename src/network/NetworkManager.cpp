/**
 * @file NetworkManager.cpp
 * @author Lars Brenna (lars.brenna@uit.no)
 * @brief Implementation of asynchronous berkley network socket support in the trusted runtime. translated into synchronous POSIX calls via netStubs.cpp
 * @version 0.1
 * @date 2020-02-03
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#include "network/NetworkManager.h"

NetworkManager::NetworkManager(IDiggiAPI *context, ISealingAlgorithm *seal)
    : aDiggiAPI(context),
      aSealer(seal)
{
}

NetworkManager::~NetworkManager()
{
    //nothing
}

void NetworkManager::AsyncSocket(int domain, int type, int protocol,
                                 async_cb_t callback, void *context)
{

    aDiggiAPI->GetLogObject()->Log(LDEBUG, "AsyncSocket domain=%d\n", domain);

    size_t request_size = sizeof(domain) + sizeof(type) + sizeof(protocol);

	auto mngr = aDiggiAPI->GetMessageManager();
	auto msg = mngr->allocateMessage("network_server_func", request_size, CALLBACK, CLEARTEXT);
	msg->type = NET_SOCKET_MSG_TYPE;
	
	auto *currentPtr = msg->data;
	Pack::pack<int>(&currentPtr, domain);
	Pack::pack<int>(&currentPtr, type);
	Pack::pack<int>(&currentPtr, protocol);

    mngr->Send(msg, callback, context);
}

void NetworkManager::AsyncBind(int sockfd, const struct sockaddr *addr,
                               socklen_t addrlen, async_cb_t callback, void *context)
{
    aDiggiAPI->GetLogObject()->Log(LDEBUG, "AsyncBind sockfd=%d\n", sockfd);

    size_t request_size = sizeof(int) + sizeof(unsigned int) + sizeof(sockaddr);

    auto mngr = aDiggiAPI->GetMessageManager();
    auto msg = mngr->allocateMessage("network_server_func", request_size, CALLBACK, CLEARTEXT);
    msg->type = NET_BIND_MSG_TYPE;

	uint8_t *currentPtr = msg->data;
	Pack::pack<int>(&currentPtr, sockfd);
	Pack::pack<unsigned int>(&currentPtr, addrlen);
	Pack::packBuffer(&currentPtr, (uint8_t*)addr, addrlen);

    mngr->Send(msg, callback, context);
}

void NetworkManager::AsyncGetsockname(int sockfd, const struct sockaddr *addr, socklen_t *addrlen, async_cb_t callback, void *context)
{
    aDiggiAPI->GetLogObject()->Log(LDEBUG, "AsyncGetsockname sockfd=%d\n", sockfd);

    size_t request_size = sizeof(int) + sizeof(unsigned int);
	
    auto mngr = aDiggiAPI->GetMessageManager();
    auto msg = mngr->allocateMessage("network_server_func", request_size, CALLBACK, CLEARTEXT);
    msg->type = NET_GETSOCKNAME_MSG_TYPE;

	auto *currentPtr = msg->data;
	Pack::pack<int>(&currentPtr, sockfd);
	Pack::pack<unsigned int>(&currentPtr, (unsigned int) *addrlen);
    mngr->Send(msg, callback, context);
}

void NetworkManager::AsyncGetsockopt(int sockfd, int level, int optname, socklen_t *optlen, async_cb_t cb, void *context)
{
    aDiggiAPI->GetLogObject()->Log(LDEBUG, "AsyncGetsockopt sockfd=%d\n", sockfd);

    size_t request_size = sizeof(int) + sizeof(int) + sizeof(int) + *optlen;

	auto mngr = aDiggiAPI->GetMessageManager();
	auto msg = mngr->allocateMessage("network_server_func", request_size, CALLBACK, CLEARTEXT);
	msg->type = NET_GETSOCKOPT_MSG_TYPE;
	
	auto *currentPtr = msg->data;
	Pack::pack<int>(&currentPtr, sockfd);
	Pack::pack<int>(&currentPtr, level);
	Pack::pack<int>(&currentPtr, optname);
	Pack::pack<unsigned int>(&currentPtr, *optlen);

    mngr->Send(msg, cb, context);
}

void NetworkManager::AsyncSetsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen, async_cb_t cb, void *context)
{

    aDiggiAPI->GetLogObject()->Log(LDEBUG, "AsyncSetsockopt sockfd=%d\n", sockfd);

    size_t request_size = sizeof(int) + sizeof(int) + sizeof(int) + sizeof(socklen_t) + optlen;

	auto mngr = aDiggiAPI->GetMessageManager();
	auto msg = mngr->allocateMessage("network_server_func",request_size, CALLBACK, CLEARTEXT);
	msg->type = NET_SETSOCKOPT_MSG_TYPE;
	
	auto *currentPtr = msg->data;
	Pack::pack<int>(&currentPtr, sockfd);
	Pack::pack<int>(&currentPtr, level);
	Pack::pack<int>(&currentPtr, optname);
	Pack::pack<unsigned int>(&currentPtr, optlen);
	Pack::packBuffer(&currentPtr, (uint8_t*) optval, (unsigned int)optlen);

    mngr->Send(msg, cb, context);
}

void NetworkManager::AsyncConnect(int sockfd, const struct sockaddr *addr, socklen_t addrlen, async_cb_t cb, void *context)
{
    aDiggiAPI->GetLogObject()->Log(LDEBUG, "AsyncConnect sockfd=%d\n", sockfd);

    size_t request_size = sizeof(int) + sizeof(unsigned int) + addrlen;

	auto mngr = aDiggiAPI->GetMessageManager();
    auto msg = mngr->allocateMessage("network_server_func", request_size, CALLBACK, CLEARTEXT);
    msg->type = NET_CONNECT_MSG_TYPE;

    mngr->Send(msg, cb, context);
}

void NetworkManager::AsyncFcntl(int sockfd, int cmd, int flag, async_cb_t cb, void *context)
{
    aDiggiAPI->GetLogObject()->Log(LDEBUG, "AsyncFcntl sockfd=%d\n", sockfd);
	
    size_t request_size = sizeof(int) + sizeof(int) + sizeof(int);

    auto mngr = aDiggiAPI->GetMessageManager();
    auto msg = mngr->allocateMessage("network_server_func", request_size, CALLBACK, CLEARTEXT);
    msg->type = NET_FCNTL_MSG_TYPE;

    uint8_t *currentPtr = msg->data;
    
	Pack::pack<int>(&currentPtr, sockfd);
	Pack::pack<int>(&currentPtr, cmd);
	Pack::pack<int>(&currentPtr, flag);

    mngr->Send(msg, cb, context);
}

void NetworkManager::AsyncListen(int sockfd, int backlog, async_cb_t cb, void *context)
{
    aDiggiAPI->GetLogObject()->Log(LDEBUG, "AsyncListen sockfd=%d\n", sockfd);

    size_t request_size = sizeof(int) + sizeof(int);

    auto mngr = aDiggiAPI->GetMessageManager();
    auto msg = mngr->allocateMessage("network_server_func", request_size, CALLBACK, CLEARTEXT);
    msg->type = NET_LISTEN_MSG_TYPE;

    uint8_t *currentPtr = msg->data;
    Pack::pack<int>(&currentPtr, sockfd);
    Pack::pack<int>(&currentPtr, backlog);

    mngr->Send(msg, cb, context);
}

void NetworkManager::AsyncSelect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout, async_cb_t cb, void *context)
{
    aDiggiAPI->GetLogObject()->Log(LDEBUG, "AsyncSelect nfds=%d\n", nfds);

    size_t request_size = sizeof(int) + sizeof(fd_set) + sizeof(fd_set) + sizeof(fd_set) + sizeof(long) + sizeof(long);

	auto mngr = aDiggiAPI->GetMessageManager();
	auto msg = mngr->allocateMessage("network_server_func",request_size, CALLBACK, CLEARTEXT);
	msg->type = NET_SELECT_MSG_TYPE;
	
	uint8_t *currentPtr = msg->data;
	Pack::pack<int>(&currentPtr, nfds);
	Pack::pack<fd_set>(&currentPtr, *readfds); 
	Pack::pack<fd_set>(&currentPtr, *writefds);
	Pack::pack<fd_set>(&currentPtr, *exceptfds);
	Pack::pack<long>(&currentPtr, timeout->tv_sec);
	Pack::pack<long>(&currentPtr, timeout->tv_usec);

    mngr->Send(msg, cb, context);
}

void NetworkManager::AsyncRecv(int sockfd, void *buf, size_t length, int flags, async_cb_t cb, void *context)
{
    aDiggiAPI->GetLogObject()->Log(LDEBUG, "AsyncRecv sockfd=%d\n", sockfd);

    size_t request_size = sizeof(int) + sizeof(size_t) + sizeof(int);

	auto mngr = aDiggiAPI->GetMessageManager();
	auto msg = mngr->allocateMessage("network_server_func", request_size, CALLBACK, CLEARTEXT);
	msg->type = NET_RECV_MSG_TYPE;
	
	uint8_t *currentPtr = msg->data;
	Pack::pack<int>(&currentPtr, sockfd);
	Pack::pack<size_t>(&currentPtr, length);
	Pack::pack<int>(&currentPtr, flags);

    mngr->Send(msg, cb, context);
}

void NetworkManager::AsyncSend(int sockfd, const void *buf, size_t length, int flags, async_cb_t cb, void *context)
{
    aDiggiAPI->GetLogObject()->Log(LDEBUG, "AsyncSend sockfd=%d\n", sockfd);

    size_t request_size = sizeof(int) + sizeof(size_t) + length + sizeof(int);

	auto mngr = aDiggiAPI->GetMessageManager();
	auto msg = mngr->allocateMessage("network_server_func",request_size, CALLBACK, CLEARTEXT);
	msg->type = NET_SEND_MSG_TYPE;
	
	uint8_t *currentPtr = msg->data;
	Pack::pack<int>(&currentPtr, sockfd);
	Pack::pack<size_t>(&currentPtr, length);
	Pack::packBuffer(&currentPtr, (uint8_t*)buf, length);
	Pack::pack<int>(&currentPtr, flags);

    mngr->Send(msg, cb, context);
}

void NetworkManager::AsyncAccept(int sockfd, const struct sockaddr *addr, socklen_t *addrlen, async_cb_t cb, void *context)
{
    aDiggiAPI->GetLogObject()->Log(LDEBUG, "AsyncAccept sockfd=%d\n", sockfd);

    size_t request_size = sizeof(int) + sizeof(socklen_t) + *addrlen;

	auto mngr = aDiggiAPI->GetMessageManager();
	auto msg = mngr->allocateMessage("network_server_func", request_size, CALLBACK, CLEARTEXT);
	msg->type = NET_ACCEPT_MSG_TYPE;
	
	uint8_t *currentPtr = msg->data;
	Pack::pack<int>(&currentPtr, sockfd);
	Pack::packBuffer(&currentPtr, (uint8_t*)addrlen, sizeof(socklen_t));
	Pack::packBuffer(&currentPtr, (uint8_t*)addr, *addrlen);

    mngr->Send(msg, cb, context);
}

void NetworkManager::AsyncClose(int sockfd, async_cb_t cb, void *context)
{
    aDiggiAPI->GetLogObject()->Log(LDEBUG, "AsyncClose sockfd=%d\n", sockfd);

    size_t request_size = sizeof(int);

    auto mngr = aDiggiAPI->GetMessageManager();
    auto msg = mngr->allocateMessage("network_server_func", request_size, CALLBACK, CLEARTEXT);
    msg->type = NET_CLOSE_MSG_TYPE;

	uint8_t *currentPtr = msg->data;
	Pack::pack<int>(&currentPtr, sockfd);

	mngr->Send(msg, cb, context);
}

void NetworkManager::AsyncRand(async_cb_t cb, void *context){
    auto mngr = aDiggiAPI->GetMessageManager();
	auto msg = mngr->allocateMessage("network_server_func", 0, CALLBACK, CLEARTEXT);
	msg->type = NET_RAND_MSG_TYPE;
	
	mngr->Send(msg, cb, context);
}
        
