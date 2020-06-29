#include <gtest/gtest.h>
#include <stdlib.h>

#include "dbserver.h"
#include "runtime/DiggiAPI.h"
#include "messaging/IMessageManager.h"
#include "messaging/IAsyncMessageManager.h"
#include "network/INetworkManager.h"
#include "posix/intercept.h"

class SQLMockMessageManager : public IMessageManager
{

    msg_t *responseMessage;

public:
    msg_t *getRespMsg()
    {
        return responseMessage;
    }
    SQLMockMessageManager() : responseMessage(nullptr)
    {
    }
    // Inherited via IMessageManager
    void endAsync(msg_t *msg)
    {
        return;
    }
    virtual void Send(msg_t *msg, async_cb_t cb, void *cb_context)
    {
        responseMessage = msg;
    }
    virtual msg_t *allocateMessage(msg_t *msg, size_t payload_size)
    {
        return (msg_t *)ALLOC_P(msg_t, payload_size);
    }
    virtual msg_t *allocateMessage(aid_t destination, size_t payload_size, msg_convention_t async, msg_delivery_t delivery)
    {
        return nullptr;
    }
    virtual msg_t *allocateMessage(std::string destination, size_t payload_size, msg_convention_t async, msg_delivery_t delivery)
    {
        return nullptr;
    }
    virtual void registerTypeCallback(async_cb_t cb, msg_type_t type, void *ctx)
    {
    }
    virtual std::map<std::string, aid_t> getfuncNames()
    {
        return std::map<std::string, aid_t>();
    }
};
class AMockMessageManager : public IAsyncMessageManager
{

    void registerTypeCallback(async_cb_t cb, msg_type_t ty, void *arg) {}
    void UnregisterTypeCallback(msg_type_t ty) {}

    unsigned long getMessageId(unsigned long func_identifier) { return 0; }

    /*
	different thread
	*/
    msg_t *allocateMessage(msg_t *msg, size_t payload_size) { return nullptr; }

    msg_t *allocateMessage(aid_t source, aid_t dest, size_t payload_size, msg_convention_t async) { return nullptr; }

    void Stop() {}
    void Start() {}

    /*await response*/
    void sendMessageAsync(msg_t *msg, async_cb_t cb, void *ptr) {}

    void endAsync(msg_t *msg) {}

    /*one way*/
    void sendMessage(msg_t *msg) {}
};

class MockNetworkManager : public INetworkManager
{

    void AsyncSocket(int domain, int type, int protocol, async_cb_t cb, void *context)
    {
    }
    void AsyncBind(int sockfd, const struct sockaddr *addr, socklen_t addrlen, async_cb_t cb, void *context)
    {
    }

    void AsyncGetsockname(int sockfd, const struct sockaddr *addr, socklen_t *addrlen, async_cb_t cb, void *context) {}
    void AsyncGetsockopt(int sockfd, int level, int optname, socklen_t *optlen, async_cb_t cb, void *context) {}
    void AsyncConnect(int sockfd, const struct sockaddr *addr, socklen_t addrlen, async_cb_t cb, void *context) {}
    void AsyncFcntl(int sockfd, int cmd, int flag, async_cb_t cb, void *context) {}
    void AsyncSetsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen, async_cb_t cb, void *context) {}
    void AsyncListen(int sockfd, int backlog, async_cb_t cb, void *context) {}
    void AsyncSelect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, timeval *timeout, async_cb_t cb, void *context) {}
    void AsyncRecv(int sockfd, void *buf, size_t len, int flags, async_cb_t cb, void *context) {}
    void AsyncSend(int sockfd, const void *buf, size_t len, int flags, async_cb_t cb, void *context) {}
    void AsyncAccept(int sockfd, const struct sockaddr *addr, socklen_t *addrlen, async_cb_t cb, void *context) {}
    void AsyncClose(int sockfd, async_cb_t cb, void *context) {}

    void AsyncRand(async_cb_t cb, void *context) {}
};

class MockThreadPool : public IThreadPool
{
    int rounds;

public:
    MockThreadPool() {}

    void Schedule(async_cb_t cb, void *args, const char *label)
    {
    }
    void ScheduleOn(size_t id, async_cb_t cb, void *args, const char *label)
    {
    }
    size_t physicalThreadCount()
    {
        return 1;
    }
    void Stop() {}
    void SetThreadId(void) {}

    void SchedulerLoop() {}

    void Yield() {}

    void InitializeThread() {}

    // Inherited via IThreadPool
    int currentThreadId()
    {
        return 0;
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
class SQLMockLog : public ILog
{
    std::string GetfuncName()
    {
        return "testfunc";
    }
    void SetFuncId(aid_t aid, std::string name = "func")
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
class MockContext : public IDiggiAPI
{
    IAsyncMessageManager *amm;
    IMessageManager *mm;
    INetworkManager *nm;
    IThreadPool *th;
    ILog *lgg;
    json_node nodes;

public:
    MockContext(AMockMessageManager *amm, SQLMockMessageManager *mm, MockNetworkManager *nm, MockThreadPool *threadpl, SQLMockLog *lg) : amm(amm),
                                                                                                                                         mm(mm),
                                                                                                                                         nm(nm),
                                                                                                                                         th(threadpl),
                                                                                                                                         lgg(lg)
    {
    }
    ISignalHandler *GetSignalHandler()
    {
        return nullptr;
    }

    IThreadPool *GetThreadPool()
    {
        return th;
    }
    IMessageManager *GetMessageManager()
    {
        return this->mm;
    }

    INetworkManager *GetNetworkManager()
    {
        return this->nm;
    }

    ILog *GetLogObject()
    {
        return lgg;
    }
    aid_t GetId()
    {
        return aid_t();
    }
    IStorageManager *GetStorageManager()
    {
        return nullptr;
    }
    json_node &GetFuncConfig()
    {
        return nodes;
    }
    void SetFuncConfig(json_node conf)
    {
    }
    void SetThreadPool(IThreadPool *pool)
    {
        return;
    }
    void SetMessageManager(IMessageManager *mngr)
    {
        return;
    }
    void SetNetworkManager(INetworkManager *nmngr)
    {
        return;
    }

    void SetLogObject(ILog *lg)
    {
        return;
    }
    void SetId(aid_t id)
    {
        return;
    }
};

TEST(sqlservertest, simple_select_test)
{

    set_syscall_interposition(0);
    //Setup db
    sqlite3 *db;
    char *zErrMsg = 0;
    int rc = sqlite3_open("test.dbserver.db", &db);

    EXPECT_TRUE(rc == SQLITE_OK);

    /* Create SQL statement */
    std::string sql = "DROP TABLE IF EXISTS COMPANY;"
                      "CREATE TABLE COMPANY("
                      "ID INT PRIMARY KEY     NOT NULL,"
                      "NAME           TEXT    NOT NULL,"
                      "AGE            INT     NOT NULL,"
                      "ADDRESS        CHAR(50),"
                      "SALARY         REAL );";

    /* Execute SQL statement */

    rc = sqlite3_exec(db, sql.c_str(), nullptr, 0, &zErrMsg);

    EXPECT_TRUE(rc == SQLITE_OK);

    for (int i = 0; i < 10; i++)
    {

        std::stringstream ss;
        ss << "INSERT INTO COMPANY VALUES(" << i << ",'Anders', 9000, '123 party avenue', 123.00);";

        rc = sqlite3_exec(db, ss.str().c_str(), 0, 0, &zErrMsg);

        EXPECT_TRUE(rc == SQLITE_OK);
    }

    sqlite3_close(db);

    //execute test

    auto amm = new AMockMessageManager();
    auto mm = new SQLMockMessageManager();
    auto threadp = new MockThreadPool();
    auto lg = new SQLMockLog();
    auto context = new MockContext(amm, mm, nullptr, threadp, lg);

    auto rsp = new msg_async_response_t();
    std::string sql_query = "SELECT * FROM COMPANY";

    auto msg = (msg_t *)calloc(1, sizeof(msg_t) + sql_query.size() + 1);
    msg->type = SQL_QUERY_MESSAGE_TYPE;
    memcpy(msg->data, sql_query.c_str(), sql_query.size());
    rsp->msg = msg;
    auto _this = new DBServer(context, "test.dbserver.db");
    rsp->context = _this;

    DBServer::executeQuery(rsp, 1);
    auto respMsg = mm->getRespMsg();
    auto zstr = zcstring((char *)respMsg->data, respMsg->size - sizeof(msg_t));
    auto results = json_node(zstr);
    int i = 0;
    for (auto result : results.children)
    {
        EXPECT_TRUE(result.children.size() == 5);
        EXPECT_TRUE(result.children[0].value.tostring().compare(std::to_string(i)) == 0);
        EXPECT_TRUE(result.children[1].value.tostring().compare("Anders") == 0);
        EXPECT_TRUE(result.children[2].value.tostring().compare(std::to_string(9000)) == 0);
        EXPECT_TRUE(result.children[3].value.tostring().compare("123 party avenue") == 0);
        EXPECT_TRUE(result.children[4].value.tostring().compare("123.0") == 0);
        i++;
    }
    delete _this;
}

TEST(sqlservertest, simple_insert_test)
{
    set_syscall_interposition(0);
    sqlite3 *db;
    char *zErrMsg = 0;
    int rc = sqlite3_open("test.dbserver.db", &db);

    EXPECT_TRUE(rc == SQLITE_OK);

    /* Create SQL statement */
    std::string sql = "DROP TABLE IF EXISTS COMPANY;"
                      "CREATE TABLE COMPANY("
                      "ID INT PRIMARY KEY     NOT NULL,"
                      "NAME           TEXT    NOT NULL,"
                      "AGE            INT     NOT NULL,"
                      "ADDRESS        CHAR(50),"
                      "SALARY         REAL );";

    /* Execute SQL statement */

    rc = sqlite3_exec(db, sql.c_str(), nullptr, 0, &zErrMsg);
    sqlite3_close(db);

    auto amm = new AMockMessageManager();
    auto mm = new SQLMockMessageManager();
    auto nm = new MockNetworkManager();
    auto threadp = new MockThreadPool();
    auto lg = new SQLMockLog();

    auto context = new MockContext(amm, mm, nm, threadp, lg);
    auto rsp = new msg_async_response_t();

    std::stringstream ss;

    for (int i = 0; i < 10; i++)
    {

        ss << "INSERT INTO COMPANY VALUES(" << i << ",'Anders', 9000, '123 party avenue', 123.00);";
    }

    std::string sql_query = ss.str();

    auto msg = (msg_t *)calloc(1, sizeof(msg_t) + sql_query.size() + 1);
    msg->type = SQL_QUERY_MESSAGE_TYPE;
    memcpy(msg->data, sql_query.c_str(), sql_query.size());
    msg->size = sql_query.size() + sizeof(msg_t) + 1;
    msg->data[sql_query.size()] = '\0';
    rsp->msg = msg;
    auto _this = new DBServer(context, "test.dbserver.db");
    rsp->context = _this;

    DBServer::executeQuery(rsp, 1);
    auto respMsg = mm->getRespMsg();
    auto zstr = zcstring((char *)respMsg->data, respMsg->size - sizeof(msg_t));
    auto results = json_node(zstr);
    EXPECT_TRUE(results.children.size() == 0);
    delete _this;
}
