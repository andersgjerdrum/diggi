#include <gtest/gtest.h>
#include "mongoose.h"
#include "Logging.h"
#include "messaging/AsyncMessageManager.h"
#include "threading/ThreadPool.h"
#include "network/NetworkServer.h"
#include "messaging/Pack.h"


#define MAXMSG  512

//note: copied from https://www.gnu.org/software/libc/manual/html_node/Server-Example.html
int
read_from_client (int filedes)
{
  char buffer[MAXMSG];
  int nbytes;

  nbytes = read (filedes, buffer, MAXMSG);
  if (nbytes < 0)
    {
      /* Read error. */
      perror ("read");
      exit (EXIT_FAILURE);
    }
  else if (nbytes == 0)
    /* End-of-file. */
    return -1;
  else
    {
      /* Data read. */
      fprintf (stderr, "Server: got message: `%s'\n", buffer);
      return 0;
    }
}


class MockLog : public ILog
{
	std::string GetfuncName() {
		return "testfunc";
	}
	void SetFuncId(aid_t aid, std::string name = "func") {

	}
	void SetLogLevel(LogLevel lvl) {

	}
	void Log(LogLevel lvl, const char *fmt, ...) {

	}
	void Log(const char *fmt, ...) {

	}
};


class SMockMessageManager : public IMessageManager
{
	msg_t * outbound;
public:
	SMockMessageManager() :outbound(nullptr) {

	}

	msg_t* GetOutboundMessage() {
		return outbound;
	}
	void Send(msg_t * msg, async_cb_t cb, void * cb_context)
	{
		DIGGI_ASSERT(cb == nullptr);
		DIGGI_ASSERT(cb_context == nullptr);
		outbound = msg;
	}
	msg_t * allocateMessage(msg_t * msg, size_t payload_size)
	{
		auto msgret = ALLOC_P(msg_t, payload_size);
		memset(msgret, 0x0, sizeof(msg_t) + payload_size);
		msgret->size = sizeof(msg_t) + payload_size;
		return msgret;
	}
	void endAsync(msg_t *msg) {
		return;
	}
    msg_t * allocateMessage(std::string destination, size_t payload_size, msg_convention_t async, msg_delivery_t delivery)
    {
        return allocateMessage(aid_t(), payload_size, async, delivery);
    }

	msg_t * allocateMessage(aid_t destination, size_t payload_size, msg_convention_t async, msg_delivery_t delivery)
	{
		auto msgret = ALLOC_P(msg_t, payload_size);
		memset(msgret, 0x0, sizeof(msg_t) + payload_size);

		msgret->size = sizeof(msg_t) + payload_size;
		return msgret;
	}
	void registerTypeCallback(async_cb_t cb, msg_type_t type, void * ctx)
	{

	}
	std::map<std::string, aid_t> getfuncNames() {
		return std::map<std::string, aid_t>();
	}
};

TEST(networkservertests, DISABLED_mgInit)
{
    ThreadPool *threadpool = new ThreadPool(1);
    auto mlog = new StdLogger(threadpool);
	auto mm = new SMockMessageManager();
	
    aid_t serv;
    DiggiAPI * acontext_srv = new DiggiAPI(nullptr, nullptr, nullptr, nullptr, nullptr, mlog, serv, nullptr);
	auto ns = new NetworkServer(acontext_srv); 

	auto resp = new msg_async_response_t();
	resp->context = ns;

    // test socket:
    size_t request_size = sizeof(int) + sizeof(int) + sizeof(int);
	auto msg = mm->allocateMessage(aid_t(),request_size, CALLBACK, CLEARTEXT);
	msg->type = NET_SOCKET_MSG_TYPE;

	uint8_t *currentPtr = msg->data;
	Pack::pack<int>(&currentPtr, 2); // domain
	Pack::pack<int>(&currentPtr, 1); // type
	Pack::pack<int>(&currentPtr, 0); // protocol

	resp->msg = msg;
    ns->ServerSocket(resp, 1);

	auto resp_msg = mm->GetOutboundMessage();
	EXPECT_TRUE(resp_msg->size == sizeof(msg_t) + sizeof(int));
	uint8_t *resp_data = resp_msg->data;
	int sock_fd = Pack::unpack<int>(&resp_data);
	EXPECT_TRUE(sock_fd > 0);
	free(resp_msg);
	delete resp;
	free(msg);


	// test bind:

	struct sockaddr_in sa = {0};
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	sa.sin_port = htons(8080);
	unsigned int addrlen = sizeof(sa);

	request_size = sizeof(int) + sizeof(unsigned int) + addrlen;

	auto msg2 = mm->allocateMessage(aid_t(),request_size, CALLBACK, CLEARTEXT);
	msg2->type = NET_BIND_MSG_TYPE;
	currentPtr = msg2->data;
	Pack::pack<int>(&currentPtr, sock_fd);  // retval from socket above
	Pack::pack<unsigned int>(&currentPtr, addrlen); // addrlen 
	Pack::packBuffer(&currentPtr, (uint8_t*)&sa, addrlen);   

	auto resp2 = new msg_async_response_t();
	resp2->context = ns;
	resp2->msg = msg2;
	ns->ServerBind(resp2, 1);

	auto resp2_msg2 = mm->GetOutboundMessage();
	EXPECT_TRUE(resp2_msg2->size == sizeof(msg_t) + sizeof(int));
	uint8_t *resp2msg2_data = resp2_msg2->data;
	int retval = Pack::unpack<int>(&resp2msg2_data);
	EXPECT_TRUE(retval == 0);
	free(resp2_msg2);
	free(msg2);
	delete resp2;
	
	// test getsockname:
	struct sockaddr *sock_addr;
	addrlen = 100; // arbitrary size
	sock_addr = (sockaddr*) malloc(addrlen); 
	memset(sock_addr, 0, addrlen);

	request_size = sizeof(int) + addrlen + sizeof(socklen_t);
	msg = mm->allocateMessage(aid_t(), request_size, CALLBACK, CLEARTEXT);
	msg->type = NET_GETSOCKNAME_MSG_TYPE;
	currentPtr = msg->data;
	Pack::pack<int>(&currentPtr, sock_fd);
	Pack::pack<unsigned int>(&currentPtr, addrlen);
	Pack::packBuffer(&currentPtr, (uint8_t*)sock_addr, addrlen);

	resp = new msg_async_response_t();
	resp->context = ns;
	resp->msg = msg;
    ns->ServerGetsockname(resp, 1);

	resp_msg = mm->GetOutboundMessage();
	EXPECT_TRUE(resp_msg->size <= sizeof(msg_t) + sizeof(int) + sizeof(unsigned int) + addrlen);
	resp_data = resp_msg->data;
	retval = Pack::unpack<int>(&resp_data);

	EXPECT_TRUE(sock_fd > 0);
	free(resp_msg);
	delete resp;
	free(msg);

	// test fcntl:
	request_size = sizeof(int) + sizeof(int) + sizeof(int);
	msg = mm->allocateMessage(aid_t(), request_size, CALLBACK, CLEARTEXT);
	msg->type = NET_FCNTL_MSG_TYPE;
	int cmd = 2;
	int flag = 1;
	currentPtr = msg->data;
	Pack::pack<int>(&currentPtr, sock_fd);
	Pack::pack<int>(&currentPtr, cmd);
	Pack::pack<int>(&currentPtr, flag);

	resp = new msg_async_response_t();
	resp->context = ns;
	resp->msg = msg;
	ns->ServerFcntl(resp, 1);

	resp_msg = mm->GetOutboundMessage();
	EXPECT_TRUE(resp_msg->size == sizeof(msg_t) + sizeof(int));
	resp_data = resp_msg->data;
	retval = Pack::unpack<int>(&resp_data);

	EXPECT_TRUE(retval == 0);
	free(resp_msg);
	delete resp;
	free(msg);

	// test listen:

	request_size = sizeof(int) + sizeof(int);
	msg = mm->allocateMessage(aid_t(), request_size, CALLBACK, CLEARTEXT);
	msg->type = NET_LISTEN_MSG_TYPE;
	
	currentPtr = msg->data;
	int backlog = 64;
	Pack::pack<int>(&currentPtr, sock_fd);
	Pack::pack<int>(&currentPtr, backlog);

	resp = new msg_async_response_t();
	resp->context = ns;
	resp->msg = msg;
	ns->ServerListen(resp, 1);

	resp_msg = mm->GetOutboundMessage();
	EXPECT_TRUE(resp_msg->size == sizeof(msg_t) + sizeof(int));
	resp_data = resp_msg->data;
	retval = Pack::unpack<int>(&resp_data);

	EXPECT_TRUE(retval == 0);
	free(resp_msg);
	delete resp;
	free(msg);

	// test select:      
	request_size = sizeof(int) + sizeof(fd_set) + sizeof(fd_set) + sizeof(fd_set) + sizeof(long) + sizeof(long);

	msg = mm->allocateMessage(aid_t(), request_size, CALLBACK, CLEARTEXT);
	msg->type = NET_SELECT_MSG_TYPE;
	
	fd_set read_fd_set = {0};
	fd_set write_fd_set = {0};
	fd_set except_fd_set = {0};
	// Initialize the set of active sockets. 
	FD_ZERO (&read_fd_set);
	FD_ZERO(&write_fd_set);
	FD_ZERO(&except_fd_set);
  	FD_SET (sock_fd, &read_fd_set);
	struct timeval timeout = {0};
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;

	printf("FD_SETSIZE = %d, sizeof(fdset) = %lu, sizeof(read_fd_set) = %lu\n", FD_SETSIZE, sizeof(fd_set), sizeof(read_fd_set));

	currentPtr = msg->data;
	Pack::pack<int>(&currentPtr, FD_SETSIZE);
	Pack::pack<fd_set>(&currentPtr, read_fd_set); //, nfds);
	Pack::pack<fd_set>(&currentPtr, write_fd_set);
	Pack::pack<fd_set>(&currentPtr, except_fd_set);
	Pack::pack<long>(&currentPtr, timeout.tv_sec);
	Pack::pack<long>(&currentPtr, timeout.tv_usec);

	resp = new msg_async_response_t();
	resp->context = ns;
	resp->msg = msg;
	ns->ServerSelect(resp, 1);

	resp_msg = mm->GetOutboundMessage();
	EXPECT_TRUE(resp_msg->size == sizeof(msg_t) + sizeof(int) + sizeof(fd_set) + sizeof(fd_set) + sizeof(fd_set));
	resp_data = resp_msg->data;
	retval = Pack::unpack<int>(&resp_data);
	DIGGI_ASSERT(retval == 0);

	read_fd_set = Pack::unpack<fd_set>(&resp_data);
	write_fd_set = Pack::unpack<fd_set>(&resp_data);
    except_fd_set = Pack::unpack<fd_set>(&resp_data);
    
	// note: copied code from https://www.gnu.org/software/libc/manual/html_node/Server-Example.html
	/* Service all the sockets with input pending. */
	int i;
	socklen_t size = 0;
	sockaddr new_addr = {0};
	for (i = 0; i < FD_SETSIZE; ++i) {
		if (FD_ISSET (i, &read_fd_set)) {
			if (i == sock_fd) {
				/* Connection request on original socket. */
				//TODO: must edit this to a ServerAccept call
                request_size = sizeof(int) + sizeof(socklen_t) + sizeof(sockaddr);
            	msg = mm->allocateMessage(aid_t(), request_size, CALLBACK, CLEARTEXT);
            	msg->type = NET_ACCEPT_MSG_TYPE;
                Pack::pack<int>(&currentPtr, sock_fd);
                Pack::pack<socklen_t>(&currentPtr, size);
                Pack::packBuffer(&currentPtr, (uint8_t*)&new_addr, sizeof(sockaddr));
                resp = new msg_async_response_t();
    	        resp->context = ns;
	            resp->msg = msg;
				ns->ServerAccept(resp, 1); // send message
                resp_msg = mm->GetOutboundMessage(); // recv message
                EXPECT_TRUE(resp_msg->size == sizeof(int));
                resp_data = resp_msg->data;
                int ready_socks = Pack::unpack<int>(&resp_data);
				EXPECT_TRUE(ready_socks > 0);
	/*                 printf (stdout,
							"Server: connect from host %s, port %hd.\n",
							inet_ntoa (((sockaddr_in)new_addr).sin_addr),
							ntohs (new_addr.sin_port)); */
				FD_SET (ready_socks, &read_fd_set);
                printf("Got through the select/accept\n");
			}
			else {
				/* Data arriving on an already-connected socket. */
				// TODO: here is the shiznits
                printf("in the close clause...\n");
				if (read_from_client (i) < 0)  {
					close (i);
					FD_CLR (i, &read_fd_set);
				}
			}
		}
	}
	free(resp_msg);
	delete resp;
	free(msg);

	// test recv:      
	request_size = sizeof(int) + sizeof(unsigned int) + sizeof(int);

	msg = mm->allocateMessage(aid_t(), request_size, CALLBACK, CLEARTEXT);
	msg->type = NET_RECV_MSG_TYPE;
	currentPtr = msg->data;
	Pack::pack<int>(&currentPtr, sock_fd);
	Pack::pack<unsigned int>(&currentPtr, 128);
	Pack::pack<int>(&currentPtr, 0); // flags

	resp = new msg_async_response_t();
	resp->context = ns;
	resp->msg = msg;
	ns->ServerRecv(resp, 1);

	resp_msg = mm->GetOutboundMessage();
	resp_data = resp_msg->data;
	retval = Pack::unpack<int>(&resp_data);
	EXPECT_TRUE(retval != -1);
    
	char databuffer[retval + 1];
	memset((void*)databuffer, 0, retval+1);
	Pack::unpackBuffer(&resp_data, (uint8_t*)databuffer, retval);

	printf("MST received %d bytes: %s\n", retval, databuffer);

	free(resp_msg);
	delete resp;
	free(msg);

	// test send:
	request_size = sizeof(int) + sizeof(unsigned int) + 1024 + sizeof(int); //random request size 1024.
	msg = mm->allocateMessage(aid_t(), request_size, CALLBACK, CLEARTEXT);
	msg->type = NET_SEND_MSG_TYPE;
	currentPtr = msg->data;
	Pack::pack<int>(&currentPtr, sock_fd);
	Pack::pack<unsigned int>(&currentPtr, 1024);
	char dataBuffer[1024];
	memset(dataBuffer, 0, 1024);
	Pack::packBuffer(&currentPtr, (uint8_t*)dataBuffer, 1024);
	Pack::pack<int>(&currentPtr, 0); // flags

	resp = new msg_async_response_t();
	resp->context = ns;
	resp->msg = msg;
	ns->ServerSend(resp, 1);

	resp_msg = mm->GetOutboundMessage();
	resp_data = resp_msg->data;
	retval = Pack::unpack<int>(&resp_data);
	DIGGI_ASSERT(retval == 0);
	
	printf("MST sent %d bytes out of requested %d\n", retval, 1024);


	// test accept:
	

	free(resp_msg);
	delete resp;
	free(msg);

	
	free(sock_addr);
	delete mm;
	delete mlog;
	delete ns;
	delete resp;
	free(msg); 


/* 

	size_t request_size = sizeof(int) + sizeof(mode_t) + path_length + 1;
	auto msg = mm->allocateMessage(request_size, CALLBACK);
	msg->type = FILEIO_OPEN;


	auto ptr = msg->data;

	memcpy(ptr, &mode, sizeof(mode_t));

	ptr += sizeof(mode_t);

	memcpy(ptr, &oflags, sizeof(int));

	ptr += sizeof(int);
	memcpy(ptr, path_n, path_length + 1);
	resp->msg = msg;
	StorageServer::fileIoOpen(resp, 1);

	auto resp_msg = mm->GetOutboundMessage();
	EXPECT_TRUE(resp_msg->size == sizeof(msg_t) + sizeof(int));

	memcpy(&fileDescriptor, resp_msg->data, sizeof(int));

	EXPECT_TRUE(fileDescriptor > 0);
	EXPECT_TRUE(ss->lseekstatemap[fileDescriptor] == 0);
	free(resp_msg);

	delete mm;
	delete log;
	delete ss;
	delete resp;
	free(msg); */
}

