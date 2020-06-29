/**
 * @file netStubs.cpp
 * @author Lars Brenna (lars.brenna@uit.no)
 * @brief implementation of synchronous translation of asynchronous network manager requests
 * @version 0.1
 * @date 2020-02-03
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#include "posix/net_stubs.h"
#include "messaging/Pack.h"

#include "DiggiGlobal.h"
#include "misc.h"

static int stop = 0;

void netstubs_teardown() {
	DIGGI_ASSERT(!stop);
	stop = 1;
	__sync_synchronize();
}

void netstubs_free_response(msg_t **incoming_msg) {
	DIGGI_ASSERT(*incoming_msg);
	free(*incoming_msg);
	*incoming_msg = nullptr;
}

void netstubs_setresponse(void *ptr, int status) { // why can't parameter 1 be a msg_async_response_t* ?
	auto rsp = (msg_async_response_t*)ptr;
	DIGGI_ASSERT(ptr);
	DIGGI_ASSERT(status); // status isn't really used, just magic number 1 for now.
	IDiggiAPI* global_context = GET_DIGGI_GLOBAL_CONTEXT();

	global_context->GetLogObject()->Log(LDEBUG, "Setting response from call\n");

	msg_t **mr = (msg_t **)rsp->context;
	*mr = COPY(msg_t, rsp->msg, rsp->msg->size);
	global_context->GetMessageManager()->endAsync(rsp->msg);

	return;
}

msg_t* netstubs_wait_for_response(msg_t **ptr) {

	while (*ptr == nullptr) {
		IDiggiAPI* global_context = GET_DIGGI_GLOBAL_CONTEXT();
		auto tp = global_context->GetThreadPool();
		DIGGI_ASSERT(tp != nullptr);
		tp->Yield();
	}
	return *ptr;
}


int netstubs_extract_retval(msg_t **mptr) {   // at this point mptr is still nullptr

	auto response = netstubs_wait_for_response(mptr);
	auto ptr = response->data;
	DIGGI_ASSERT(response->size == sizeof(msg_t) + sizeof(int));
	int retval = Pack::unpack<int>(&ptr);

	netstubs_free_response(mptr);
	if (retval < 0) {
		set_errno(ENOENT);
		errno = ENOENT;
	}
	return retval;
}

// not implemented:
void        i_freeaddrinfo(struct addrinfo *res){ DIGGI_ASSERT(false);}
int         i_getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res){ DIGGI_ASSERT(false); return 0;}
int			i_getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen) { DIGGI_ASSERT(false); return 0;}
ssize_t		i_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen) { DIGGI_ASSERT(false); return 0;}
ssize_t		i_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen) { DIGGI_ASSERT(false); return 0;}

int i_getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen) { 
    // setting up pointer to return value
	msg_t *retmsg = nullptr;
	msg_t **put = &retmsg;

	IDiggiAPI* global_context = GET_DIGGI_GLOBAL_CONTEXT();
	global_context->GetLogObject()->Log(LogLevel::LRELEASE, "Intercepted getsockopt syscall %d %d %d\n", sockfd, level, optname);

	global_context->GetNetworkManager()->AsyncGetsockopt(sockfd, level, optname, optlen, 
												netstubs_setresponse, 
												put);
    netstubs_wait_for_response(put);                                                
    
    // fetch stuff from put and place in pointers given from caller
	auto *ptr = retmsg->data;	
    int retval = Pack::unpack<int>(&ptr);
    *optlen = Pack::unpack<unsigned int>(&ptr);
    if (optlen != NULL){
        Pack::unpackBuffer(&ptr, (uint8_t*) optval, (unsigned int) *optlen);
    }


	return retval;

}



int	i_socket(int domain, int type, int protocol) { 	
	DIGGI_ASSERT(type > 0 && type < 6); // always min 1 and max 5

	// setting up pointer to return value
	msg_t *retmsg = nullptr;
	msg_t **put = &retmsg;

	IDiggiAPI* global_context = GET_DIGGI_GLOBAL_CONTEXT();
	global_context->GetLogObject()->Log(LogLevel::LRELEASE, "Intercepted socket syscall %d %d %d\n", domain, type, protocol);

	global_context->GetNetworkManager()->AsyncSocket(domain, type, protocol, 
												netstubs_setresponse, 
												put);
	return netstubs_extract_retval(put);
 }

//TODO: implement err codes and place in ERRNO
int	i_setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) {
	IDiggiAPI* global_context = GET_DIGGI_GLOBAL_CONTEXT();
	global_context->GetLogObject()->Log(LogLevel::LRELEASE, "Intercepted setsockopt syscall %d, %d, %d, %p, %d\n", sockfd, level, optname, optval, optlen);

	// setting up pointer to return value
	msg_t *retmsg = nullptr;
	msg_t **put = &retmsg;

	global_context->GetNetworkManager()->AsyncSetsockopt(sockfd, level, optname, optval, optlen,
												netstubs_setresponse, 
												put);

	return netstubs_extract_retval(put);

}

ssize_t	i_send(int sockfd, const void *buf, size_t len, int flags) { 
	IDiggiAPI* global_context = GET_DIGGI_GLOBAL_CONTEXT();
	global_context->GetLogObject()->Log(LogLevel::LDEBUG, "Intercepted SEND syscall on fd %d\n", sockfd);

	// setting up pointer to return value
	msg_t *retmsg = nullptr;
	msg_t **put = &retmsg;

	global_context->GetNetworkManager()->AsyncSend(sockfd, buf, len, flags, netstubs_setresponse, put);
	netstubs_wait_for_response(put);
	
	// fetch retval from put 
	auto *ptr = retmsg->data;	

	return Pack::unpack<ssize_t>(&ptr);
}



int	i_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
	IDiggiAPI* global_context = GET_DIGGI_GLOBAL_CONTEXT();
	global_context->GetLogObject()->Log(LogLevel::LDEBUG, "Intercepted bind syscall on fd %d\n", sockfd);

	// setting up pointer to return value
	msg_t *retmsg = nullptr;
	msg_t **put = &retmsg;

	global_context->GetNetworkManager()->AsyncBind(sockfd, addr, addrlen, netstubs_setresponse, put);

	return netstubs_extract_retval(put);
 }
   
int	i_getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen) { 
	IDiggiAPI* global_context = GET_DIGGI_GLOBAL_CONTEXT();
	global_context->GetLogObject()->Log(LogLevel::LDEBUG, "Intercepted getsockname syscall on fd %d with addrlen %d\n", sockfd, *addrlen);

	// setting up pointer to return value
	msg_t *retmsg = nullptr;
	msg_t **put = &retmsg;

	global_context->GetNetworkManager()->AsyncGetsockname(sockfd, addr, addrlen, netstubs_setresponse, put);
	netstubs_wait_for_response(put);

	// fetch stuff from put and place in pointers given from caller
	auto *ptr = retmsg->data;	
	int retval = Pack::unpack<int>(&ptr);
    *addrlen = Pack::unpack<socklen_t>(&ptr);
    Pack::unpackBuffer(&ptr, (uint8_t*)addr, *addrlen);
	
	return retval;
}

int	i_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) { 
	IDiggiAPI* global_context = GET_DIGGI_GLOBAL_CONTEXT();
	global_context->GetLogObject()->Log(LogLevel::LDEBUG, "Intercepted connect syscall on fd %d\n", sockfd);

	// setting up pointer to return value
	msg_t *retmsg = nullptr;
	msg_t **put = &retmsg;

	global_context->GetNetworkManager()->AsyncConnect(sockfd, addr, addrlen, netstubs_setresponse, put);
	
	return netstubs_extract_retval(put); 
	
}

ssize_t	i_recv(int sockfd, void *buf, size_t len, int flags) { 
	IDiggiAPI* global_context = GET_DIGGI_GLOBAL_CONTEXT();
	global_context->GetLogObject()->Log(LogLevel::LDEBUG, "Intercepted recv syscall on fd %d\n", sockfd);
    
	// setting up pointer to return value
	msg_t *retmsg = nullptr;
	msg_t **put = &retmsg;

	global_context->GetNetworkManager()->AsyncRecv(sockfd, buf, len, flags, netstubs_setresponse, put);
	netstubs_wait_for_response(put);

	// unpack msg and place in buffer:
	auto *ptr = retmsg->data;	
	ssize_t retval = Pack::unpack<ssize_t>(&ptr);
    
	DIGGI_ASSERT (retval <= (ssize_t)len);

	if (retval < 0){ // retval is either -1 for error, 0 for end-of-stream, or number of bytes written
		//TODO: set errno 
        errno = Pack::unpack<int>(&ptr);
        //printf("i_recv: retval from AsyncRecv is %d, errno is %d\n", retval, errno);
		global_context->GetLogObject()->Log(LogLevel::LRELEASE,"Recv returned errno %d, %s\n", errno, strerror(errno));
	} else {
        
		memcpy(buf, ptr, retval);
	}
	return retval;	 
}

int	i_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) { 
	IDiggiAPI* global_context = GET_DIGGI_GLOBAL_CONTEXT();
	global_context->GetLogObject()->Log(LogLevel::LDEBUG, "Intercepted accept syscall on fd %d\n", sockfd);

	// setting up pointer to return value
	msg_t *retmsg = nullptr;
	msg_t **put = &retmsg;

	global_context->GetNetworkManager()->AsyncAccept(sockfd, addr, addrlen, netstubs_setresponse, put);
	
	return netstubs_extract_retval(put);
}

int	i_listen(int sockfd, int backlog) { 
	IDiggiAPI* global_context = GET_DIGGI_GLOBAL_CONTEXT();
	global_context->GetLogObject()->Log(LogLevel::LDEBUG, "Intercepted listen syscall on fd %d\n", sockfd);

	// setting up pointer to return value
	msg_t *retmsg = nullptr;
	msg_t **put = &retmsg;

	global_context->GetNetworkManager()->AsyncListen(sockfd, backlog, netstubs_setresponse, put);
	
	return netstubs_extract_retval(put);
}

int	i_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout) {
	IDiggiAPI* global_context = GET_DIGGI_GLOBAL_CONTEXT();
	global_context->GetLogObject()->Log(LogLevel::LDEBUG, "Intercepted select syscall on nfds %d\n", nfds);

	// setting up pointer to return value
	msg_t *retmsg = nullptr;
	msg_t **put = &retmsg;

	global_context->GetNetworkManager()->AsyncSelect(nfds, readfds, writefds, exceptfds, timeout, netstubs_setresponse, put);
	netstubs_wait_for_response(put);
	
	// fetch stuff from put and place in pointers given from caller	
	auto *ptr = retmsg->data;	
	int retval = Pack::unpack<int>(&ptr);
	*readfds   = Pack::unpack<fd_set>(&ptr);
	*writefds  = Pack::unpack<fd_set>(&ptr);
	*exceptfds = Pack::unpack<fd_set>(&ptr);

	return retval;
}

///TODO:Added by Anders Gjerdrum, not implemented yet.
int             i_shutdown(int socket, int how){
    DIGGI_ASSERT(false);
    return 0;
}

int network_close(int sockfd){
	IDiggiAPI* global_context = GET_DIGGI_GLOBAL_CONTEXT();
	global_context->GetLogObject()->Log(LogLevel::LDEBUG, "Intercepted close syscall on fd %d\n", sockfd);

	msg_t *retmsg = nullptr;
	msg_t **put = &retmsg;

	global_context->GetNetworkManager()->AsyncClose(sockfd, netstubs_setresponse, put);

	return netstubs_extract_retval(put);

}
///TODO: This approach will not work for enclaves, as syscall_def.h is responsible for intercepting enclave systemcalls, not intercept.cpp
int  network_fcntl(int sockfd, int cmd, int flag)  // network implementation of fcntl.
{
	IDiggiAPI* global_context = GET_DIGGI_GLOBAL_CONTEXT();
	global_context->GetLogObject()->Log(LogLevel::LDEBUG, "Intercepted fcntl syscall on fd %d\n", sockfd);

	msg_t *retmsg = nullptr;
	msg_t **put = &retmsg;
	
	global_context->GetNetworkManager()->AsyncFcntl(sockfd, cmd, flag, netstubs_setresponse, put);

	return netstubs_extract_retval(put);
}