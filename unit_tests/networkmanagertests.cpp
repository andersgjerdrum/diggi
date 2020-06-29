#include <gtest/gtest.h>
#include "mongoose.h"
#include "datatypes.h"
#include "Logging.h"
#include "network/NetworkManager.h"
#include "messaging/AsyncMessageManager.h"
#include "posix/intercept.h"
#include "posix/net_stubs.h"
#include "DiggiGlobal.h"
#include "threading/ThreadPool.h"

static const char *s_http_port = "8000";
static struct mg_serve_http_opts s_http_server_opts;

static void ev_handler(struct mg_connection *nc, int ev, void *p)
{
    if (ev == MG_EV_HTTP_REQUEST)
    {
        mg_serve_http(nc, (struct http_message *)p, s_http_server_opts);
    }
}

class MockMongooseLog : public ILog
{
    std::string GetfuncName()
    {
        return "func";
    }
    void SetFuncId(aid_t aid, std::string snname = "func")
    {
    }
    void SetLogLevel(LogLevel lvl)
    {
    }
    void Log(LogLevel lvl, const char *fmt, ...)
    {
    }
    void Log(const char *fmt, ...)
    {
    }
};
static MockMongooseLog loginstance;

class MockMessageManager : public IMessageManager
{
    int test_iter = 0;
    int sock_fd = 100;

    void Send(msg_t *msg, async_cb_t cb, void *cb_context)
    {
        if (msg->type == NET_SOCKET_MSG_TYPE)
        {
            // socket call. expecting the following four parameters:
            int domain = -1;
            int type = -1;
            int protocol = -1;

            auto ptr = msg->data;
            memcpy(&domain, ptr, sizeof(int));
            ptr += sizeof(int);
            memcpy(&type, ptr, sizeof(int));
            ptr += sizeof(int);
            memcpy(&protocol, ptr, sizeof(int));

            // printf("MMM socket received domain %d, type %d, protocol %d\n", domain, type, protocol);

            // reply:
            auto resp = ALLOC(msg_async_response_t);
            resp->context = cb_context;
            resp->msg = ALLOC_P(msg_t, sizeof(int));
            resp->msg->size = sizeof(msg_t) + sizeof(int);

            memcpy(resp->msg->data, &sock_fd, sizeof(int));

            cb(resp, 1);
            free(resp);

            test_iter++;
            sock_fd++;
        }
        else if (msg->type == NET_BIND_MSG_TYPE)
        {
            int fd = -1;
            unsigned int socklen = 0;

            auto ptr = msg->data;
            memcpy(&fd, ptr, sizeof(int));
            ptr += sizeof(int);

            memcpy(&socklen, ptr, sizeof(unsigned int));
            ptr += sizeof(unsigned int);

            struct sockaddr addr = {0};
            memcpy(&addr, ptr, socklen);

            // printf("MMM bind received sockfd %d, sockaddr %d, socklen %d\n", fd, addr.sa_family, socklen);

            // reply:
            auto resp = ALLOC(msg_async_response_t);
            resp->context = cb_context;
            resp->msg = ALLOC_P(msg_t, sizeof(int));
            resp->msg->size = sizeof(msg_t) + sizeof(int);
            int retval = 0; // is the proper reply for success.
            memcpy(resp->msg->data, &retval, sizeof(int));

            cb(resp, 1);
            free(resp);

            test_iter++;
        }
        else if (msg->type == NET_GETSOCKNAME_MSG_TYPE)
        {
            int fd = -1;
            unsigned int sa_family = 0;
            unsigned int addrlen = 0;

            auto ptr = msg->data;
            memcpy(&fd, ptr, sizeof(int));
            ptr += sizeof(int);

            memcpy(&sa_family, ptr, sizeof(unsigned short));
            ptr += sizeof(unsigned short);

            memcpy(&addrlen, ptr, sizeof(unsigned int));

            ptr = nullptr;

            // printf("MMM getsockname received sockfd %d, sa_family %d, addrlen %d\n", fd, sa_family, addrlen);

            // reply with sockname and actual addrlen
            const char *sockname = "TEST1";
            unsigned int actual_length = sizeof(sockname);
            auto resp = ALLOC(msg_async_response_t);
            resp->context = cb_context;
            resp->msg = ALLOC_P(msg_t, sizeof(int) + sizeof(unsigned int) + actual_length);

            resp->msg->size = sizeof(msg_t) + sizeof(int) + sizeof(unsigned int) + actual_length;
            int retval = 0; // is the proper reply for success.
            ptr = resp->msg->data;
            memcpy(ptr, &retval, sizeof(int));
            ptr += sizeof(int);
            memcpy(ptr, &actual_length, sizeof(unsigned int));
            ptr += sizeof(unsigned int);
            memcpy(ptr, sockname, actual_length);

            cb(resp, 1);
            free(resp);
            test_iter++;
        }
        else if (msg->type == NET_CONNECT_MSG_TYPE)
        {
            struct sockaddr sa = {0};

            uint8_t *ptr = msg->data;
            int fd = Pack::unpack<int>(&ptr);
            unsigned int addrlen = Pack::unpack<unsigned int>(&ptr);
            Pack::unpackBuffer(&ptr, (uint8_t*)&sa, addrlen);

            printf("MMM connect received sockfd %d, sa_family %d, addrlen %d\n", fd, sa.sa_family, addrlen);
            ptr = nullptr;

            // reply:
            auto resp = ALLOC(msg_async_response_t);
            resp->context = cb_context;
            resp->msg = ALLOC_P(msg_t, sizeof(int));
            resp->msg->size = sizeof(msg_t) + sizeof(int);
            int retval = 0; // is the proper reply for success.
            memcpy(resp->msg->data, &retval, sizeof(int));

            cb(resp, 1);
            free(resp);

            test_iter++;
        }
        else if (msg->type == NET_FCNTL_MSG_TYPE)
        {
            int fd = -1;
            int cmd = 0;
            int flag = 0;

            auto ptr = msg->data;
            memcpy(&fd, ptr, sizeof(int));
            ptr += sizeof(int);

            memcpy(&cmd, ptr, sizeof(int));
            ptr += sizeof(int);

            memcpy(&flag, ptr, sizeof(int));

            // printf("MMM fcntl received sockfd %d, cmd %d, flag %d\n", fd, cmd, flag);
            ptr = nullptr;

            // reply:
            auto resp = ALLOC(msg_async_response_t);
            resp->context = cb_context;
            resp->msg = ALLOC_P(msg_t, sizeof(int));
            resp->msg->size = sizeof(msg_t) + sizeof(int);
            int retval = 0; // is the proper reply for success for most cmds.
            if (cmd == F_GETFL)
            {
                retval = O_NONBLOCK | O_RDWR;
            }
            memcpy(resp->msg->data, &retval, sizeof(int));

            cb(resp, 1);
            free(resp);

            test_iter++;
        }
        else if (msg->type == NET_SETSOCKOPT_MSG_TYPE)
        {
            int fd = -1;
            int level = -1;
            int optname = -1;
            int optval = -1;

            auto ptr = msg->data;
            memcpy(&fd, ptr, sizeof(int));
            ptr += sizeof(int);

            memcpy(&level, ptr, sizeof(int));
            ptr += sizeof(int);

            memcpy(&optname, ptr, sizeof(int));
            ptr += sizeof(int);

            memcpy(&optval, ptr, sizeof(int));

            // printf("MMM setsockopt received fd %d, level %d, optname %d, optval, %d\n", fd, level, optname, optval);
            ptr = nullptr;

            // reply:
            auto resp = ALLOC(msg_async_response_t);
            resp->context = cb_context;
            resp->msg = ALLOC_P(msg_t, sizeof(int));
            resp->msg->size = sizeof(msg_t) + sizeof(int);
            int retval = 0; // is the proper reply for success.
            memcpy(resp->msg->data, &retval, sizeof(int));

            cb(resp, 1);
            free(resp);

            test_iter++;
        }
        else if (msg->type == NET_LISTEN_MSG_TYPE)
        {
            int fd = -1;
            int backlog = -1;

            auto ptr = msg->data;
            memcpy(&fd, ptr, sizeof(int));
            ptr += sizeof(int);

            memcpy(&backlog, ptr, sizeof(int));

            // printf("MMM listen received sockfd %d, backlog %d\n", fd, backlog);
            ptr = nullptr;

            // reply:
            auto resp = ALLOC(msg_async_response_t);
            resp->context = cb_context;
            resp->msg = ALLOC_P(msg_t, sizeof(int));
            resp->msg->size = sizeof(msg_t) + sizeof(int);
            int retval = 0; // is the proper reply for success.
            memcpy(resp->msg->data, &retval, sizeof(int));

            cb(resp, 1);
            free(resp);

            test_iter++;
        }

        else if (msg->type == NET_SELECT_MSG_TYPE)
        {
            // printf("MMM SELECT ");
            int nfds = 0;
            fd_set fdread = {0};
            fd_set fdwrite = {0};
            fd_set fdexept = {0};

            auto ptr = msg->data;
            memcpy(&nfds, ptr, sizeof(int));
            ptr += sizeof(int);
            // printf("received nfds %d \n", nfds);

            memcpy(&fdread, ptr, sizeof(fd_set));
            ptr += sizeof(fd_set);
            memcpy(&fdwrite, ptr, sizeof(fd_set));
            ptr += sizeof(fd_set);
            memcpy(&fdexept, ptr, sizeof(fd_set));

            ptr = nullptr;
            // reply:
            auto resp = ALLOC(msg_async_response_t);
            resp->context = cb_context;
            resp->msg = ALLOC_P(msg_t, sizeof(int) + sizeof(fd_set) + sizeof(fd_set) + sizeof(fd_set));
            resp->msg->size = sizeof(msg_t) + sizeof(int) + sizeof(fd_set) + sizeof(fd_set) + sizeof(fd_set);
            int retval = 100 + 1; // retval should be the highest fd that is ready + 1.
            ptr = resp->msg->data;
            memcpy(ptr, &retval, sizeof(int));
            ptr += sizeof(int);
            FD_SET(100, &fdread);
            memcpy(ptr, &fdread, sizeof(fd_set));
            ptr += sizeof(fd_set);
            memcpy(ptr, &fdwrite, sizeof(fd_set));
            ptr += sizeof(fd_set);
            memcpy(ptr, &fdexept, sizeof(fd_set));

            cb(resp, 1);
            free(resp);

            test_iter++;
        }
        else if (msg->type == NET_RECV_MSG_TYPE)
        {
            // printf("MMM RECV ");
            int sockfd, flags = 0;
            unsigned int maxbytes = 0;
            auto ptr = msg->data;
            memcpy(&sockfd, ptr, sizeof(int));
            ptr += sizeof(int);
            memcpy(&maxbytes, ptr, sizeof(int));
            ptr += sizeof(int);
            memcpy(&flags, ptr, sizeof(int));

            // printf("received sockfd %d, maxbytes %ud, flags %d \n", sockfd, maxbytes, flags);
            // reply:
            auto resp = ALLOC(msg_async_response_t);
            resp->context = cb_context;
            resp->msg = ALLOC_P(msg_t, sizeof(unsigned int) + maxbytes);
            resp->msg->size = sizeof(msg_t) + sizeof(unsigned int) + maxbytes;
            ptr = resp->msg->data;
            memcpy(ptr, &maxbytes, sizeof(unsigned int));
            ptr += sizeof(unsigned int);
            memset(ptr, 0, maxbytes);

            cb(resp, 1);
            free(resp);

            test_iter++;
        }
        else if (msg->type == NET_SEND_MSG_TYPE)
        {
            // printf("MMM SEND ");
            int sockfd, flags = 0;
            unsigned int buflength = 0;
            auto ptr = msg->data;
            memcpy(&sockfd, ptr, sizeof(int));
            ptr += sizeof(int);
            memcpy(&buflength, ptr, sizeof(unsigned int));
            ptr += sizeof(unsigned int);
            const void *outbuf[buflength];
            memcpy(&outbuf, ptr, buflength);
            ptr += buflength;
            memcpy(&flags, ptr, sizeof(int));

            // printf("received sockfd %d, buflength %u, flags %d \n", sockfd, buflength, flags);
            std::string outString((char *)outbuf, buflength);
            // std::cout << "Content from SEND was:" <<  outString <<  std::endl;

            // reply:
            auto resp = ALLOC(msg_async_response_t);
            resp->context = cb_context;
            resp->msg = ALLOC_P(msg_t, sizeof(unsigned int));
            resp->msg->size = sizeof(msg_t) + sizeof(unsigned int);
            ptr = resp->msg->data;
            memcpy(ptr, &buflength, sizeof(unsigned int)); // return number of bytes actually sent.

            cb(resp, 1);
            free(resp);

            test_iter++;
        }
        else if (msg->type == NET_ACCEPT_MSG_TYPE)
        {
            // printf("MMM ACCEPT ");
            int sockfd = 0;
            unsigned int addrlength = 0;
            auto ptr = msg->data;
            memcpy(&sockfd, ptr, sizeof(int));
            ptr += sizeof(int);
            memcpy(&addrlength, ptr, sizeof(unsigned int));
            ptr += sizeof(unsigned int);
            struct sockaddr sa = {0};
            memcpy(&sa, ptr, addrlength);

            // printf("received sockfd %d, sa.family %d, addrlen %u\n", sockfd, sa.sa_family, addrlength);

            // reply:
            auto resp = ALLOC(msg_async_response_t);
            resp->context = cb_context;
            resp->msg = ALLOC_P(msg_t, sizeof(int));
            resp->msg->size = sizeof(msg_t) + sizeof(int);
            ptr = resp->msg->data;
            memcpy(ptr, &sockfd, sizeof(int)); // return number of bytes actually sent.

            cb(resp, 1);
            free(resp);

            test_iter++;
        }
        else if (msg->type == NET_CLOSE_MSG_TYPE)
        {
            // printf("MMM CLOSE ");
            int sockfd = 0;
            auto ptr = msg->data;
            memcpy(&sockfd, ptr, sizeof(int));

            // printf("received sockfd %d\n", sockfd);

            // reply:
            auto resp = ALLOC(msg_async_response_t);
            resp->context = cb_context;
            resp->msg = ALLOC_P(msg_t, sizeof(int));
            resp->msg->size = sizeof(msg_t) + sizeof(int);
            ptr = resp->msg->data;
            int retval = 0;
            memcpy(ptr, &retval, sizeof(int));

            cb(resp, 1);
            free(resp);

            test_iter++;
        }

        else
        {
            // printf("MMM: undefined msg type");
            DIGGI_ASSERT(false);
        }
    }

    msg_t *allocateMessage(msg_t *msg, size_t payload_size)
    {
        auto msgret = ALLOC_P(msg_t, payload_size);
        msgret->size = sizeof(msg_t) + payload_size;
        return msgret;
    }

    void endAsync(msg_t *msg)
    {
        return;
    }
    msg_t *allocateMessage(aid_t destination, size_t payload_size, msg_convention_t async, msg_delivery_t delivery)
    {
        auto msgret = ALLOC_P(msg_t, payload_size);
        msgret->size = sizeof(msg_t) + payload_size;
        return msgret;
    }

    msg_t *allocateMessage(std::string destination, size_t payload_size, msg_convention_t async, msg_delivery_t delivery)
    {
        EXPECT_TRUE(memcmp(destination.c_str(), "network_server_func", 20) == 0);

        return allocateMessage(aid_t(), payload_size, async, delivery);
    }
    void registerTypeCallback(async_cb_t cb, msg_type_t type, void *ctx)
    {
        DIGGI_ASSERT(false);
    }
    std::map<std::string, aid_t> getfuncNames()
    {
        return std::map<std::string, aid_t>();
    }
};

TEST(networkmanagertests, DISABLED_simple_server_test)
{
    //TODO: Add test clauses which captures errors better.
    set_syscall_interposition(1);
    auto threadpool = new ThreadPool(1);
    auto mlog = new StdLogger(threadpool);
    auto mkmm = new MockMessageManager();

    auto ctx = new DiggiAPI(nullptr, nullptr, mkmm, nullptr, nullptr, mlog, aid_t(), nullptr);
    auto ns = new NoSeal();

    auto netmanager = new NetworkManager(ctx, ns);
    ctx->SetNetworkManager(netmanager);
    
    SET_DIGGI_GLOBAL_CONTEXT(ctx); // 0 means not encrypted

    struct mg_mgr mgr;
    struct mg_connection *nc;

    printf("hey\n");
    mg_mgr_init(&mgr, NULL);
    printf("Starting web server on port %s\n", s_http_port);

    nc = mg_bind(&mgr, s_http_port, ev_handler);
    
    if (nc == NULL)
    {
        // printf("Failed to create listener\n");
        EXPECT_TRUE(0);
        return;
    }
   
    //// Set up HTTP server parameters
    mg_set_protocol_http_websocket(nc);
    s_http_server_opts.document_root = "."; // Serve current directory
    s_http_server_opts.enable_directory_listing = "yes";

    mg_mgr_poll(&mgr, 100);
    mg_mgr_free(&mgr);
    set_syscall_interposition(0);
}