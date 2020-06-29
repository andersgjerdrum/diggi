/*only used in libfunc*/
/**
 * @file NetworkServer.cpp
 * @author Lars Brenna (lars.brenna@uit.no)
 * @brief 
 * @version 0.1
 * @date 2020-02-03
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#if  !defined(DIGGI_ENCLAVE) && !defined(UNTRUSTED_APP)

#include "network/NetworkServer.h"


NetworkServer::NetworkServer(IDiggiAPI *api) : diggiapi(api) {
     
}

NetworkServer::~NetworkServer(){

	}

void NetworkServer::InitializeServer(){
    diggiapi->GetMessageManager()->registerTypeCallback(NetworkServer::ServerSocket,     NET_SOCKET_MSG_TYPE, this);
    diggiapi->GetMessageManager()->registerTypeCallback(NetworkServer::ServerBind,       NET_BIND_MSG_TYPE, this);
    diggiapi->GetMessageManager()->registerTypeCallback(NetworkServer::ServerGetsockname, NET_GETSOCKNAME_MSG_TYPE, this);
	diggiapi->GetMessageManager()->registerTypeCallback(NetworkServer::ServerConnect,    NET_CONNECT_MSG_TYPE, this);
	diggiapi->GetMessageManager()->registerTypeCallback(NetworkServer::ServerFcntl,      NET_FCNTL_MSG_TYPE, this);
	diggiapi->GetMessageManager()->registerTypeCallback(NetworkServer::ServerSetsockopt, NET_SETSOCKOPT_MSG_TYPE, this);
    diggiapi->GetMessageManager()->registerTypeCallback(NetworkServer::ServerGetsockopt, NET_GETSOCKOPT_MSG_TYPE, this);
	diggiapi->GetMessageManager()->registerTypeCallback(NetworkServer::ServerListen,     NET_LISTEN_MSG_TYPE, this);
	diggiapi->GetMessageManager()->registerTypeCallback(NetworkServer::ServerSelect,     NET_SELECT_MSG_TYPE, this);
	diggiapi->GetMessageManager()->registerTypeCallback(NetworkServer::ServerRecv,     	NET_RECV_MSG_TYPE, this);
	diggiapi->GetMessageManager()->registerTypeCallback(NetworkServer::ServerSend,    	NET_SEND_MSG_TYPE, this);
	diggiapi->GetMessageManager()->registerTypeCallback(NetworkServer::ServerAccept,     NET_ACCEPT_MSG_TYPE, this);
	diggiapi->GetMessageManager()->registerTypeCallback(NetworkServer::ServerClose,     	NET_CLOSE_MSG_TYPE, this);
    diggiapi->GetMessageManager()->registerTypeCallback(NetworkServer::ServerRand,     	NET_RAND_MSG_TYPE, this);
    

	
}

void NetworkServer::ServerSocket(void *msg, int status){
    // now unpack message:
	auto ctx = (msg_async_response_t*)msg;
	DIGGI_ASSERT(ctx);
	auto _this = (NetworkServer*)ctx->context;
	auto *ptr = ctx->msg->data;
    int domain = Pack::unpack<int>(&ptr);
	int type = Pack::unpack<int>(&ptr); 
	int protocol = Pack::unpack<int>(&ptr);
    DIGGI_TRACE(_this->diggiapi->GetLogObject(), LogLevel::LDEBUG, "ServerSocket domain %d, type %d, protocol %d\n", domain, type, protocol);
	
    // then direct syscall:   
    int sockfd = __real_socket(domain, type, protocol);
    if (sockfd == -1) {
        DIGGI_TRACE(_this->diggiapi->GetLogObject(), LogLevel::LRELEASE, "NetworkServer failed to create socked endpoint, with error %d: %s\n", errno, strerror(errno));
	}

    // set up return value (sockfd)
	auto msg_n = _this->diggiapi->GetMessageManager()->allocateMessage(ctx->msg, sizeof(int));
	msg_n->src = ctx->msg->dest;
	msg_n->dest = ctx->msg->src;
	auto *dataPtr = msg_n->data;
	Pack::pack<int>(&dataPtr, sockfd);
	_this->diggiapi->GetMessageManager()->Send(msg_n, nullptr, nullptr);

}

void NetworkServer::ServerBind(void *message, int status){
    // now unpack message:
	auto ctx = (msg_async_response_t*)message;
	DIGGI_ASSERT(ctx);
	auto _this = (NetworkServer*)ctx->context;
	auto *ptr = ctx->msg->data;
   
    int sockfd = Pack::unpack<int>(&ptr);
    socklen_t addrlen = (socklen_t) Pack::unpack<unsigned int>(&ptr);
	struct sockaddr sa = { 0 };
    DIGGI_ASSERT(sizeof(sockaddr) >= addrlen);
	Pack::unpackBuffer(&ptr, (uint8_t*)&sa, addrlen);

	// then direct syscall:
	int retval = __real_bind(sockfd, &sa, addrlen);
	if (retval == -1) {
        DIGGI_TRACE(_this->diggiapi->GetLogObject(), LogLevel::LRELEASE, "NetworkServer failed in bind, with error %d: %s\n", errno, strerror(errno));
    }

	auto msg_n = _this->diggiapi->GetMessageManager()->allocateMessage(ctx->msg, sizeof(int));
	msg_n->src = ctx->msg->dest;
	msg_n->dest = ctx->msg->src;
	
	auto *dataPtr = msg_n->data;
	Pack::pack<int>(&dataPtr, retval);
	_this->diggiapi->GetMessageManager()->Send(msg_n, nullptr, nullptr);
}

void NetworkServer::ServerGetsockname(void *message, int status){
    // now unpack message:
	auto ctx = (msg_async_response_t*)message;
	DIGGI_ASSERT(ctx);
	auto _this = (NetworkServer*)ctx->context;
	auto *ptr = ctx->msg->data;

	int sockfd = Pack::unpack<int>(&ptr);
    socklen_t addrlen = (socklen_t) Pack::unpack<unsigned int>(&ptr);
	struct sockaddr sa = {0};
    DIGGI_ASSERT(sizeof(sockaddr) >= addrlen);
	//then direct syscall:
	int retval = __real_getsockname(sockfd, &sa, &addrlen);
	if (retval == -1) {
        DIGGI_TRACE(_this->diggiapi->GetLogObject(), LogLevel::LRELEASE, "NetworkServer failed in getsockname, with error %d: %s\n", errno, strerror(errno));
    }

	auto msg_n = _this->diggiapi->GetMessageManager()->allocateMessage(ctx->msg, sizeof(int) + sizeof(unsigned int) + addrlen);
	msg_n->src = ctx->msg->dest;
	msg_n->dest = ctx->msg->src;
	
	auto *dataPtr = msg_n->data;
	Pack::pack<int>(&dataPtr, retval);
	Pack::pack<unsigned int>(&dataPtr, (unsigned int)addrlen);
	Pack::packBuffer(&dataPtr, (uint8_t*)&sa, addrlen);

	_this->diggiapi->GetMessageManager()->Send(msg_n, nullptr, nullptr);
}

void NetworkServer::ServerFcntl(void *message, int status){
    // now unpack message:
	auto ctx = (msg_async_response_t*)message;
	DIGGI_ASSERT(ctx);
	auto _this = (NetworkServer*)ctx->context;
	auto *ptr = ctx->msg->data;

	int sockfd = Pack::unpack<int>(&ptr);
	int cmd = Pack::unpack<int>(&ptr);
	int flag = Pack::unpack<int>(&ptr);

	int retval = __real_fcntl(sockfd, cmd, flag);

	if (retval == -1) {
		DIGGI_TRACE(_this->diggiapi->GetLogObject(), LogLevel::LRELEASE, "NetworkServer failed in fcntl, with error %d: %s\n", errno, strerror(errno));    
	}
    
	auto msg_n = _this->diggiapi->GetMessageManager()->allocateMessage(ctx->msg, sizeof(int));
	msg_n->src = ctx->msg->dest;
	msg_n->dest = ctx->msg->src;
	
	auto *dataPtr = msg_n->data;
	Pack::pack<int>(&dataPtr, retval);

	_this->diggiapi->GetMessageManager()->Send(msg_n, nullptr, nullptr);
}

void NetworkServer::ServerConnect(void *message, int status){
    // now unpack message:
	auto ctx = (msg_async_response_t*)message;
	DIGGI_ASSERT(ctx);
	auto _this = (NetworkServer*)ctx->context;
	auto *ptr = ctx->msg->data;

	int sockfd = Pack::unpack<int>(&ptr);
    socklen_t addrlen = (socklen_t) Pack::unpack<unsigned int>(&ptr);
	struct sockaddr sa = {0};
    DIGGI_ASSERT(sizeof(sockaddr)>=addrlen);
	Pack::unpackBuffer(&ptr, (uint8_t*)&sa, addrlen);
	
	//then direct syscall:
	int retval = __real_connect(sockfd, &sa, addrlen);
	if (retval == -1) {
       DIGGI_TRACE(_this->diggiapi->GetLogObject(), LogLevel::LRELEASE, "NetworkServer failed in connect, with error %d: %s\n", errno, strerror(errno));
    }

	auto msg_n = _this->diggiapi->GetMessageManager()->allocateMessage(ctx->msg, sizeof(int));
	msg_n->src = ctx->msg->dest;
	msg_n->dest = ctx->msg->src;
	
	auto *dataPtr = msg_n->data;
	Pack::pack<int>(&dataPtr, retval);

	_this->diggiapi->GetMessageManager()->Send(msg_n, nullptr, nullptr);

}


void NetworkServer::ServerSetsockopt(void *message, int status){
	// now unpack message:
	auto ctx = (msg_async_response_t*)message;
	DIGGI_ASSERT(ctx);
	auto _this = (NetworkServer*)ctx->context;
	auto *ptr = ctx->msg->data;

	int sockfd = Pack::unpack<int>(&ptr);
	int level = Pack::unpack<int>(&ptr);
	int optname = Pack::unpack<int>(&ptr);
	unsigned int optlen = Pack::unpack<unsigned int>(&ptr);
	const void* optval = (const void*)malloc(optlen);
	Pack::unpackBuffer((unsigned char**)&ptr, (uint8_t*)optval, optlen);

	//then direct syscall:
	int retval = __real_setsockopt(sockfd, level, optname, optval, optlen);

	if (retval == -1) {
      DIGGI_TRACE(_this->diggiapi->GetLogObject(), LogLevel::LRELEASE, "NetworkServer failed in setsockopt, with error %d: %s\n", errno, strerror(errno));
    }

	auto msg_n = _this->diggiapi->GetMessageManager()->allocateMessage(ctx->msg, sizeof(int));
	msg_n->src = ctx->msg->dest;
	msg_n->dest = ctx->msg->src;
	
	auto *dataPtr = msg_n->data;
	Pack::pack<int>(&dataPtr, retval);
	free((void*)optval);

	_this->diggiapi->GetMessageManager()->Send(msg_n, nullptr, nullptr);
}

void NetworkServer::ServerGetsockopt(void *message, int status){
    // now unpack message:
	auto ctx = (msg_async_response_t*)message;
	DIGGI_ASSERT(ctx);
	auto _this = (NetworkServer*)ctx->context;
	auto *ptr = ctx->msg->data;

	int sockfd = Pack::unpack<int>(&ptr);
	int level = Pack::unpack<int>(&ptr);
	int optname = Pack::unpack<int>(&ptr);
	socklen_t optlen = Pack::unpack<unsigned int>(&ptr);
	void* optval = (void*)malloc(optlen);
    memset(optval, 0, optlen);

	//then direct syscall:
	int retval = __real_getsockopt(sockfd, level, optname, optval, &optlen);
  
	if (retval == -1) {
      DIGGI_TRACE(_this->diggiapi->GetLogObject(), LogLevel::LRELEASE, "NetworkServer failed in getsockopt, with error %d: %s\n", errno, strerror(errno));
    }

    //TODO: handle special case of optlen = NULL;
	auto msg_n = _this->diggiapi->GetMessageManager()->allocateMessage(ctx->msg, sizeof(int) + sizeof(unsigned int) + optlen);
	msg_n->src = ctx->msg->dest;
	msg_n->dest = ctx->msg->src;
	
	auto *dataPtr = msg_n->data;
	Pack::pack<int>(&dataPtr, retval);
    Pack::pack<unsigned int>(&dataPtr, optlen);
    Pack::packBuffer(&dataPtr, (uint8_t*) optval, (unsigned int)optlen);
	free((void*)optval);

	_this->diggiapi->GetMessageManager()->Send(msg_n, nullptr, nullptr);
}

void NetworkServer::ServerListen(void *message, int status){
    // now unpack message:
	auto ctx = (msg_async_response_t*)message;
	DIGGI_ASSERT(ctx);
	auto _this = (NetworkServer*)ctx->context;
	auto *ptr = ctx->msg->data;

	int sockfd = Pack::unpack<int>(&ptr);
	int backlog = Pack::unpack<int>(&ptr);

	int retval = __real_listen(sockfd, backlog);

	if (retval == -1) {
		DIGGI_TRACE(_this->diggiapi->GetLogObject(), LogLevel::LRELEASE, "NetworkServer failed in listen, with error %d: %s\n", errno, strerror(errno));
    }

	auto msg_n = _this->diggiapi->GetMessageManager()->allocateMessage(ctx->msg, sizeof(int));
	msg_n->src = ctx->msg->dest;
	msg_n->dest = ctx->msg->src;
	
	auto *dataPtr = msg_n->data;
	Pack::pack<int>(&dataPtr, retval);

	_this->diggiapi->GetMessageManager()->Send(msg_n, nullptr, nullptr);
}

void NetworkServer::ServerSelect(void *message, int status){
    // now unpack message:
	auto ctx = (msg_async_response_t*)message;
	DIGGI_ASSERT(ctx);
	auto _this = (NetworkServer*)ctx->context;
	auto *ptr = ctx->msg->data;

	int nfds = Pack::unpack<int>(&ptr);
	fd_set read_fd_set = {0};
	fd_set write_fd_set = {0};
	fd_set except_fd_set = {0};
	read_fd_set = Pack::unpack<fd_set>(&ptr);
	write_fd_set = Pack::unpack<fd_set>(&ptr);
	except_fd_set = Pack::unpack<fd_set>(&ptr);
	struct timeval timeout = {0};
	timeout.tv_sec = Pack::unpack<long>(&ptr);
	timeout.tv_usec = Pack::unpack<long>(&ptr);
	
	int retval = __real_select(nfds, &read_fd_set, &write_fd_set, &except_fd_set, &timeout);
	if (retval == -1) {
       DIGGI_TRACE(_this->diggiapi->GetLogObject(), LogLevel::LRELEASE, "NetworkServer failed in select, with error %d: %s\n", errno, strerror(errno));
    }

	// now send those fdsets back:
	auto msg_n = _this->diggiapi->GetMessageManager()->allocateMessage(ctx->msg, sizeof(int) + sizeof(fd_set) + sizeof(fd_set) + sizeof(fd_set));
	msg_n->src = ctx->msg->dest;
	msg_n->dest = ctx->msg->src;
	
	auto *dataPtr = msg_n->data;
	Pack::pack<int>(&dataPtr, retval);
	Pack::pack<fd_set>(&dataPtr, read_fd_set);
	Pack::pack<fd_set>(&dataPtr, write_fd_set);
	Pack::pack<fd_set>(&dataPtr, except_fd_set);

	_this->diggiapi->GetMessageManager()->Send(msg_n, nullptr, nullptr);	
}

void NetworkServer::ServerRecv(void *msg, int status){
    // now unpack message:
	auto ctx = (msg_async_response_t*)msg;
	DIGGI_ASSERT(ctx);
	auto _this = (NetworkServer*)ctx->context;
	auto *ptr = ctx->msg->data;
	int sockfd = Pack::unpack<int>(&ptr);
	size_t buflength = Pack::unpack<size_t>(&ptr);
	int flags = Pack::unpack<int>(&ptr);

	char buffer[buflength];

	ssize_t retval = __real_recv(sockfd, buffer, buflength, flags);
	if (retval == -1) {
       DIGGI_TRACE(_this->diggiapi->GetLogObject(), LogLevel::LRELEASE, "NetworkServer failed in recv, with error %d: %s\n", errno, strerror(errno));
    }
  	size_t payloadSize = sizeof(ssize_t);
	if (retval > 0){
		payloadSize+= retval;
	} else {
        payloadSize += sizeof(int); // sending errno
    }
	auto msg_n = _this->diggiapi->GetMessageManager()->allocateMessage(ctx->msg, payloadSize);
	msg_n->src = ctx->msg->dest;
	msg_n->dest = ctx->msg->src;
	
	auto *dataPtr = msg_n->data;
	Pack::pack<ssize_t>(&dataPtr, retval);
	if (retval > 0){
		Pack::packBuffer(&dataPtr, (uint8_t*)buffer, retval);
	} else {
        Pack::pack<int>(&dataPtr, errno);
    }
	_this->diggiapi->GetMessageManager()->Send(msg_n, nullptr, nullptr);	

}

void NetworkServer::ServerSend(void *msg, int status){
    // now unpack message:
	auto ctx = (msg_async_response_t*)msg;
	DIGGI_ASSERT(ctx);
	auto _this = (NetworkServer*)ctx->context;
	auto *ptr = ctx->msg->data;
	int sockfd = Pack::unpack<int>(&ptr);
	size_t buflength = Pack::unpack<size_t>(&ptr);
	char buffer[buflength];
	Pack::unpackBuffer(&ptr, (uint8_t*)buffer, buflength);
	int flags = Pack::unpack<int>(&ptr);

	ssize_t retval = __real_send(sockfd, buffer, buflength, flags);
	if (retval == -1) {
       DIGGI_TRACE(_this->diggiapi->GetLogObject(), LogLevel::LRELEASE, "NetworkServer failed in send, with error %d: %s\n", errno, strerror(errno));
    }

	auto msg_n = _this->diggiapi->GetMessageManager()->allocateMessage(ctx->msg, sizeof(ssize_t));
	msg_n->src = ctx->msg->dest;
	msg_n->dest = ctx->msg->src;
	
	auto *dataPtr = msg_n->data;
	Pack::pack<ssize_t>(&dataPtr, retval);

	_this->diggiapi->GetMessageManager()->Send(msg_n, nullptr, nullptr);	

}
void NetworkServer::ServerAccept(void *msg, int status){
    // now unpack message:
	auto ctx = (msg_async_response_t*)msg;
	DIGGI_ASSERT(ctx);
	auto _this = (NetworkServer*)ctx->context;
	auto *ptr = ctx->msg->data;
	int sockfd = Pack::unpack<int>(&ptr);
	socklen_t addrlen = 0;
	Pack::unpackBuffer(&ptr, (uint8_t*)&addrlen, sizeof(socklen_t));
	struct sockaddr sa = {0};    
    DIGGI_ASSERT(sizeof(sockaddr) >= addrlen);
	Pack::unpackBuffer(&ptr, (uint8_t*)&sa, addrlen);

	int retval = __real_accept(sockfd, &sa, &addrlen);
	if (retval == -1) {
       DIGGI_TRACE(_this->diggiapi->GetLogObject(), LogLevel::LRELEASE, "NetworkServer failed in accept, with error %d: %s\n", errno, strerror(errno));
    }
	
	auto msg_n = _this->diggiapi->GetMessageManager()->allocateMessage(ctx->msg, sizeof(int));
	msg_n->src = ctx->msg->dest;
	msg_n->dest = ctx->msg->src;
	
	auto *dataPtr = msg_n->data;
	Pack::pack<int>(&dataPtr, retval);

	_this->diggiapi->GetMessageManager()->Send(msg_n, nullptr, nullptr);	

}
void NetworkServer::ServerClose(void *msg, int status){
    // now unpack message:
	auto ctx = (msg_async_response_t*)msg;
	DIGGI_ASSERT(ctx);
	auto _this = (NetworkServer*)ctx->context;
	auto *ptr = ctx->msg->data;
	int sockfd = Pack::unpack<int>(&ptr);

	int retval = __real_close(sockfd);
	if (retval > 0) {
        DIGGI_TRACE(_this->diggiapi->GetLogObject(), LogLevel::LRELEASE, "NetworkServer failed in close, with error %d: %s\n", errno, strerror(errno));
    }

	auto msg_n = _this->diggiapi->GetMessageManager()->allocateMessage(ctx->msg, sizeof(int));
	msg_n->src = ctx->msg->dest;
	msg_n->dest = ctx->msg->src;
	
	auto *dataPtr = msg_n->data;
	Pack::pack<int>(&dataPtr, retval);

	_this->diggiapi->GetMessageManager()->Send(msg_n, nullptr, nullptr);	
}

void NetworkServer::ServerRand(void *msg, int status){
	auto ctx = (msg_async_response_t*)msg;
	DIGGI_ASSERT(ctx);
	auto _this = (NetworkServer*)ctx->context;
	
	int retval = __real_rand();
	if (retval < 0) {
        DIGGI_TRACE(_this->diggiapi->GetLogObject(), LogLevel::LRELEASE, "NetworkServer failed in rand, with error %d: %s\n", errno, strerror(errno));
    }

	auto msg_n = _this->diggiapi->GetMessageManager()->allocateMessage(ctx->msg, sizeof(int));
	msg_n->src = ctx->msg->dest;
	msg_n->dest = ctx->msg->src;
	
	auto *dataPtr = msg_n->data;
	Pack::pack<int>(&dataPtr, retval);

	_this->diggiapi->GetMessageManager()->Send(msg_n, nullptr, nullptr);
}

#endif