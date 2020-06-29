#include <gtest/gtest.h>
#include "tpcc.h"
#include "storage/DBClient.h"
#include "dbserver.h"
#include <chrono>
#include <stdio.h>
#include "posix/intercept.h"
#include "messaging/AsyncMessageManager.h"
#include "messaging/IMessageManager.h"
#include "messaging/SecureMessageManager.h"
#include "messaging/ThreadSafeMessageManager.h"
#include "threading/ThreadPool.h"

static const char *dbname = "test.tpcc.db";
class MockDBClient : public IDBClient
{
public:
    uint64_t execute_count;
    uint64_t commit_count;
    uint64_t rollback_count;
    std::string last_ex;

    MockDBClient() : execute_count(0),
                     commit_count(0),
                     rollback_count(0),
                     last_ex("") {}
    void beginTransaction()
    {
    }
    void connect(std::string host)
    {
    }
    int execute(const char *tmpl, ...)
    {
        execute_count++;
        last_ex = tmpl;
        return 0;
    }
    int executemany(std::vector<std::string> statements)
    {
        return 0;
    }
    void commit()
    {
        commit_count++;
        return;
    }

    DBResult fetchone()
    {
        return DBResult();
    }
    std::vector<DBResult> fetchall()
    {
        return std::vector<DBResult>();
    }
    void rollback()
    {
        rollback_count++;
    }
    void freeDBResults()
    {
    }
};
class TPCCMockLog : public ILog
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
void recvbtpcc(void *ptr, int status)
{
    DIGGI_ASSERT(false);
}

TEST(tpcc_frameworktests, rand_string_test)
{
    auto rnd = Random();
    auto str = rnd.astring(TPCC_MIN_I_DATA, TPCC_MAX_I_DATA);
    EXPECT_TRUE((TPCC_MIN_I_DATA <= str.size()) && (TPCC_MAX_I_DATA >= str.size()));

    str = rnd.nstring(TPCC_MIN_I_DATA, TPCC_MAX_I_DATA);
    EXPECT_TRUE((TPCC_MIN_I_DATA <= str.size()) && (TPCC_MAX_I_DATA >= str.size()));
}

TEST(tpcc_frameworktests, simple_object_creation_and_load_reset)
{
    auto testdropstmnt = "testdroptablestatement";
    auto testinitstmnt = "testinittablestatement";
    std::map<std::string, const char *> config = {
        {"server.name", "sql_server_func"},
        {"reset", "yes"},
        {"drop.tables", testdropstmnt},
        {"init.tables", testinitstmnt}};
    auto dbclient = new MockDBClient();
    auto tpcc = new Tpcc(2, 100.0, config, dbclient);
    tpcc->load();
    EXPECT_TRUE(dbclient->execute_count == 2);
    EXPECT_TRUE(dbclient->commit_count == 0);
    EXPECT_TRUE(dbclient->rollback_count == 0);
    EXPECT_TRUE(dbclient->last_ex.compare(testinitstmnt) == 0);
    delete tpcc;
    delete dbclient;
}
TEST(tpcc_frameworktests, simple_object_creation_and_load_no_reset)
{
    auto testinitstmnt = "testinittablestatement";
    std::map<std::string, const char *> config = {
        {"server.name", "sql_server_func"},
        {"init.tables", testinitstmnt}};
    auto dbclient = new MockDBClient();
    auto tpcc = new Tpcc(2, 100.0, config, dbclient);
    tpcc->load();
    EXPECT_TRUE(dbclient->execute_count == 1);
    EXPECT_TRUE(dbclient->commit_count == 0);
    EXPECT_TRUE(dbclient->rollback_count == 0);
    EXPECT_TRUE(dbclient->last_ex.compare(testinitstmnt) == 0);
    delete tpcc;
    delete dbclient;
}

static volatile int run_load_execute_done = 0;
static std::string server_name = "sql_server_func";

static volatile int tpcc_simultaneous_start = 0;
void tpcc_run_load_execute(void *ptr, int status)
{

    auto context = (DiggiAPI *)ptr;
    auto dbclient = new DBClient(context->GetMessageManager(), context->GetThreadPool(), ENCRYPTED);

    std::map<std::string, const char *> config = {
        {"server.name", server_name.c_str()},
        {"init.tables", tpcc_create_tables.c_str()},
        {"reset", "yes"},
        {"drop.tables", tpcc_drop_tables.c_str()}};

    auto tpcc = new Tpcc(1, 900.0, config, dbclient);
    auto duration = tpcc->load();

    tpcc->execute(std::chrono::duration<double>(0.1));
    run_load_execute_done++;
    __sync_synchronize();
    delete tpcc;
    delete dbclient;
}

static DBServer *db_serv_ptr = nullptr;
void setup_server(void *ptr, int status)
{
    auto acontext = (DiggiAPI *)ptr;
    db_serv_ptr = new DBServer(acontext, dbname);
}