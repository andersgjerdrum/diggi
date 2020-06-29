#include <gtest/gtest.h>
#include "network/Connection.h"
#include "threading/IThreadPool.h"
using namespace std;

class MockThreadPool_con : public IThreadPool
{
    int rounds;

public:
    MockThreadPool_con(int rnds) : rounds(rnds) {}

    void Schedule(async_cb_t cb, void *args, const char *label)
    {
        if (rounds-- > 0)
        {
            cb(args, 1);
        }
    }
    void ScheduleOn(size_t id, async_cb_t cb, void *args, const char *label)
    {
    }
    size_t physicalThreadCount()
    {
        return 1;
    }
    void Stop() {}
    ~MockThreadPool_con() {}

    void SetThreadId(void) {}

    void SchedulerLoop() {}
    void Yield() {}

    void InitializeThread() {}

    // Inherited via IThreadPool
    int currentThreadId()
    {
        return size_t();
    }
    size_t currentVThreadId()
    {
        return 0;
    }
    bool Alive()
    {
        return true;
    }
};

class MockLog : public ILog
{
public:
    MockLog()
    {
    }
    std::string GetfuncName()
    {
        return "testfunc";
    }
    void SetFuncId(aid_t aid, std::string name = "func")
    {
    }
    void SetLogLevel(LogLevel lvl) {}
    void Log(LogLevel lvl, const char *fmt, ...) {}
    void Log(const char *fmt, ...) {}
};

typedef int (*send_cb_t)(tcp_connection_context_t *ctx, const unsigned char *buf, size_t len);
typedef void (*init_cb_t)(tcp_connection_context_t *ctx);
typedef int (*recv_cb_t)(tcp_connection_context_t *ctx, unsigned char *buf, size_t len);
typedef int (*connect_cb_t)(tcp_connection_context_t *ctx, const char *host, const char *port);
typedef int (*accept_cb_t)(tcp_connection_context_t *bind_ctx,
                           tcp_connection_context_t *client_ctx,
                           void *client_ip, size_t buf_size, size_t *ip_len);
typedef int (*bind_cb_t)(tcp_connection_context_t *ctx, const char *bind_ip, const char *port);
typedef void (*free_cb_t)(tcp_connection_context_t *ctx);
typedef int (*set_nonblock_cb_t)(tcp_connection_context_t *ctx);
typedef int (*set_block_cb_t)(tcp_connection_context_t *ctx);

class MockConnection : public IConnectionPrimitives
{

public:
    send_cb_t send_r = nullptr;
    init_cb_t init_r = nullptr;
    recv_cb_t recv_r = nullptr;
    connect_cb_t con_r = nullptr;
    accept_cb_t acpt_r = nullptr;
    bind_cb_t bind_r = nullptr;
    free_cb_t free_r = nullptr;
    set_nonblock_cb_t nblck_r = nullptr;
    set_block_cb_t blck_r = nullptr;
    int send__(tcp_connection_context_t *ctx, const unsigned char *buf, size_t len)
    {
        if (send_r)
        {
            return send_r(ctx, buf, len);
        }
        return 0;
    }

    void init__(tcp_connection_context_t *ctx)
    {
        ctx->fd = 1;
        if (init_r)
        {
            init_r(ctx);
        }
    }

    int recv__(tcp_connection_context_t *ctx, unsigned char *buf, size_t len)
    {
        if (recv_r)
        {
            return recv_r(ctx, buf, len);
        }
        return 0;
    }

    int connect__(tcp_connection_context_t *ctx, const char *host, const char *port)
    {
        if (con_r)
        {
            return con_r(ctx, host, port);
        }
        return 0;
    }

    int accept__(tcp_connection_context_t *bind_ctx,
                 tcp_connection_context_t *client_ctx,
                 void *client_ip, size_t buf_size, size_t *ip_len)
    {

        if (acpt_r)
        {
            return acpt_r(bind_ctx, client_ctx, client_ip, buf_size, ip_len);
        }
        return 0;
    }

    int bind__(tcp_connection_context_t *ctx, const char *bind_ip, const char *port)
    {
        if (bind_r)
        {
            return bind_r(ctx, bind_ip, port);
        }
        return 0;
    }

    void free__(tcp_connection_context_t *ctx)
    {
        if (free_r)
        {
            free_r(ctx);
        }
        return;
    }

    int set_nonblock__(tcp_connection_context_t *ctx)
    {
        if (nblck_r)
        {
            return nblck_r(ctx);
        }
        return 0;
    }

    int set_block__(tcp_connection_context_t *ctx)
    {
        if (blck_r)
        {
            return blck_r(ctx);
        }
        return 0;
    }
};

ALIGNED_CHAR(8)
connectiontests_simple_read_test[] = "heyman";
static bool didexecute = false;
TEST(connectiontests, simple_read_test)
{
    struct Callback
    {
        static void server_callback(void *ptr, int status)
        {
            /*callback stuff*/
            auto cli = (Connection *)ptr;
            cli->read(strlen(connectiontests_simple_read_test), server_callback_read_cb, nullptr);
        };
        static void server_callback_read_cb(void *ptr, int status)
        {
            EXPECT_TRUE(ptr != nullptr);
            auto ctx = (AsyncContext<Connection *, zcstring, void *, async_cb_t, void *> *)ptr;

            EXPECT_TRUE(ctx->item2.size() == strlen(connectiontests_simple_read_test));
            EXPECT_TRUE(strcmp(ctx->item2.tostring().c_str(), connectiontests_simple_read_test) == 0);
            ctx->item1->close();
            delete ctx->item1;
            didexecute = true;
        }
        static int accept(tcp_connection_context_t *bind_ctx,
                          tcp_connection_context_t *client_ctx,
                          void *client_ip, size_t buf_size, size_t *ip_len)
        {
            client_ctx->fd = 1;
            return 0;
        }
        static int recv(tcp_connection_context_t *ctx, unsigned char *buf, size_t len)
        {
            /*callback stuff*/

            memcpy(buf, connectiontests_simple_read_test, strlen(connectiontests_simple_read_test));
            return strlen(connectiontests_simple_read_test);
        };
    };

    auto tp = new MockThreadPool_con(3);
    auto cn = new MockConnection();
    auto lg = new MockLog();
    cn->recv_r = Callback::recv;
    cn->acpt_r = Callback::accept;
    auto con = new Connection(tp, Callback::server_callback, cn, lg);
    con->InitializeServer(NULL, "4442");
    con->close();
    delete con;
    EXPECT_TRUE(didexecute);
    delete tp;
    delete cn;
    delete lg;
}

ALIGNED_CHAR(8)
connectiontests_simple_read_line_chain[] = "heyman\nsausemaster\ntoystory\r\nletterswithoutcarrige";
ALIGNED_CHAR(8)
connectiontests_simple_read_line_chain_Exp1[] = "heyman\n";
ALIGNED_CHAR(8)
connectiontests_simple_read_line_chain_Exp2[] = "sausemaster\n";
ALIGNED_CHAR(8)
connectiontests_simple_read_line_chain_Exp3[] = "toystory\r\n";
ALIGNED_CHAR(8)
connectiontests_simple_read_line_chain_Exp4[] = "letter";
ALIGNED_CHAR(8)
connectiontests_simple_read_line_chain_Exp5[] = "swithoutcarrige";

static int callback_Val = 123;

static int turn = 0;
TEST(connectiontests, simple_read_line_chain)
{

    struct Callback
    {

        static void server_callback(void *ptr, int status)
        {
            /*callback stuff*/
            auto cli = (Connection *)ptr;
            if (turn >= 3)
            {
                cli->close();
                delete cli;
            }
            else
            {
                cli->read_line(read_line_cb, &callback_Val);
            }
        };
        static void read_line_cb(void *ptr, int status)
        {
            EXPECT_TRUE(ptr != nullptr);
            auto ctx = (AsyncContext<Connection *, zcstring, void *, async_cb_t, void *> *)ptr;
            auto buf = ctx->item2;

            if (turn == 0)
            {
                auto cbval = (int *)ctx->item5;
                EXPECT_TRUE(*cbval == callback_Val);
                EXPECT_TRUE(buf.size() == strlen(connectiontests_simple_read_line_chain_Exp1));
                EXPECT_TRUE(strcmp(buf.tostring().c_str(), connectiontests_simple_read_line_chain_Exp1) == 0);
                turn++;
                ctx->item1->read_line(read_line_cb, &callback_Val);
            }
            else if (turn == 1)
            {
                turn++;
                auto cbval = (int *)ctx->item5;
                EXPECT_TRUE(*cbval == callback_Val);
                EXPECT_TRUE(buf.size() == strlen(connectiontests_simple_read_line_chain_Exp2));
                EXPECT_TRUE(strcmp(buf.tostring().c_str(), connectiontests_simple_read_line_chain_Exp2) == 0);
                ctx->item1->read_line(read_line_cb, &callback_Val);
            }
            else if (turn == 2)
            {
                auto cbval = (int *)ctx->item5;
                EXPECT_TRUE(*cbval == callback_Val);
                EXPECT_TRUE(buf.size() == strlen(connectiontests_simple_read_line_chain_Exp3));
                EXPECT_TRUE(strcmp(buf.tostring().c_str(), connectiontests_simple_read_line_chain_Exp3) == 0);
                ctx->item1->read(6, read_cb1, &callback_Val);
                ctx->item1->close();
                delete ctx->item1;
            }
            else
            {

                EXPECT_TRUE(false);
            }
        }
        static void read_cb1(void *ptr, int status)
        {
            EXPECT_TRUE(ptr != nullptr);
            auto ctx = (AsyncContext<Connection *, zcstring, void *, async_cb_t, void *> *)ptr;
            auto cbval = (int *)ctx->item5;
            EXPECT_TRUE(*cbval == callback_Val);

            zcstring str = ctx->item2;
            EXPECT_TRUE(str.size() == strlen(connectiontests_simple_read_line_chain_Exp4));
            EXPECT_TRUE(strcmp(str.tostring().c_str(), connectiontests_simple_read_line_chain_Exp4) == 0);
            ctx->item1->read(15, read_cb2, &callback_Val);
        }
        static void read_cb2(void *ptr, int status)
        {
            EXPECT_TRUE(ptr != nullptr);
            auto ctx = (AsyncContext<Connection *, zcstring, void *, async_cb_t, void *> *)ptr;
            auto cbval = (int *)ctx->item5;
            EXPECT_TRUE(*cbval == callback_Val);
            zcstring str1 = ctx->item2;

            EXPECT_TRUE(str1.size() == strlen(connectiontests_simple_read_line_chain_Exp5));
            EXPECT_TRUE(strcmp(str1.tostring().c_str(), connectiontests_simple_read_line_chain_Exp5) == 0);
            turn++;
        }

        static int recv(tcp_connection_context_t *ctx, unsigned char *buf, size_t len)
        {
            if (turn != 3)
            {
                /*callback stuff*/
                memcpy(buf, connectiontests_simple_read_line_chain, strlen(connectiontests_simple_read_line_chain));
                return strlen(connectiontests_simple_read_line_chain);
            }
            return 0;
        };

        static int accept(tcp_connection_context_t *bind_ctx,
                          tcp_connection_context_t *client_ctx,
                          void *client_ip, size_t buf_size, size_t *ip_len)
        {
            client_ctx->fd = 1;
            return 0;
        }

        static int bind(tcp_connection_context_t *ctx, const char *bind_ip, const char *port)
        {
            ctx->fd = 2;
            return 0;
        }
    };

    auto tp = new MockThreadPool_con(8);
    auto cn = new MockConnection();
    cn->recv_r = Callback::recv;
    cn->acpt_r = Callback::accept;
    cn->bind_r = Callback::bind;
    auto lg = new MockLog();

    auto con = new Connection(tp, Callback::server_callback, cn, lg);
    con->InitializeServer(NULL, "4442");
    con->close();
    delete con;
    delete tp;
    delete cn;
    delete lg;
}

char connectiontests_simple_write_w_callback_sol[] = "123456789";
ALIGNED_CHAR(8)
connectiontests_simple_write_w_callback1[] = "123";
ALIGNED_CHAR(8)
connectiontests_simple_write_w_callback2[] = "456";
ALIGNED_CHAR(8)
connectiontests_simple_write_w_callback3[] = "789";
static int turn1 = 0;
static size_t testval = 3;
TEST(connectiontests, simple_write_w_callback)
{

    struct Callback
    {

        static void server_callback(void *ptr, int status)
        {
            /*callback stuff*/
            auto cli = (Connection *)ptr;

            if (turn1 > 0)
            {
                cli->close();
                delete cli;
                return;
            }

            zcstring buf;
            buf = buf + connectiontests_simple_write_w_callback1;
            buf = buf + connectiontests_simple_write_w_callback2;
            buf = buf + connectiontests_simple_write_w_callback3;

            cli->write(buf, write_line_cb, cli);
        };
        static void write_line_cb(void *ptr, int status)
        {
            auto cbval = (Connection *)ptr;

            EXPECT_TRUE(cbval != nullptr);
            cbval->close();
            delete cbval;
        }

        static int send(tcp_connection_context_t *ctx, const unsigned char *buf, size_t len)
        {
            EXPECT_TRUE(testval == len);
            if (len == 1)
            {
                testval = 3;
            }
            else
            {
                testval--;
            }

            EXPECT_TRUE(connectiontests_simple_write_w_callback_sol[turn1] == buf[0]);
            turn1++;
            return 1;
        };
        static int accept(tcp_connection_context_t *bind_ctx,
                          tcp_connection_context_t *client_ctx,
                          void *client_ip, size_t buf_size, size_t *ip_len)
        {
            client_ctx->fd = 1;
            return 0;
        }
        static int bind(tcp_connection_context_t *ctx, const char *bind_ip, const char *port)
        {
            ctx->fd = 2;
            return 0;
        }
    };

    auto tp = new MockThreadPool_con(12);
    auto cn = new MockConnection();
    cn->send_r = Callback::send;
    cn->acpt_r = Callback::accept;
    cn->bind_r = Callback::bind;
    auto lg = new MockLog();
    auto con = new Connection(tp, Callback::server_callback, cn, lg);
    con->InitializeServer(NULL, "4442");
    con->close();
    delete con;
    delete tp;
    delete cn;
    delete lg;
}
TEST(connectiontests, summary_for_callbacks)
{
    EXPECT_TRUE(turn == 3);
    EXPECT_TRUE(turn1 == 9);
    EXPECT_TRUE(didexecute);
}
