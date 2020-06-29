
#include <gtest/gtest.h>
#include "storage/StorageServer.h"
#include "storage/StorageManager.h"
#include "messaging/AsyncMessageManager.h"
#include "runtime/DiggiAPI.h"
#include "threading/ThreadPool.h"
#include <map>
#include "messaging/SecureMessageManager.h"
#include <string>
#include "datatypes.h"
#include "messaging/Util.h"
#include "posix/intercept.h"
#include "sqlite3.h"
#include "telemetry.h"
#include "DiggiGlobal.h"
#include "posix/pthread_stubs.h"
#include "posix/io_stubs.h"
#include "runtime/DiggiUntrustedRuntime.h"
#include "runtime/DiggiReplayManager.h"
#include <stdio.h>

static crc_vector_t vector_to_replay_stuff;

void test_init_(void *ptr, int status)
{
    auto storageserv = (StorageServer *)ptr;
    storageserv->initializeServer();
}

int callback_(void *NotUsed, int argc, char **argv, char **azColName)
{
    int i;
    for (i = 0; i < argc; i++)
    {
        printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
    }

    printf("\n");
    return 0;
}

static volatile int test_db_done = 0;
void test_db_cb_(void *ptr, int status)
{

    sqlite3 *db;
    char *zErrMsg = 0;
    int rc = sqlite3_open((char *)ptr, &db);

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

    rc = sqlite3_exec(db, sql.c_str(), callback_, 0, &zErrMsg);
    EXPECT_TRUE(rc == SQLITE_OK);
    for (int i = 0; i < 10; i++)
    {

        std::stringstream ss;
        ss << "INSERT INTO COMPANY VALUES(" << i << ",'Anders', 9000, '123 party avenue', 123);";

        rc = sqlite3_exec(db, ss.str().c_str(), 0, 0, &zErrMsg);

        telemetry_capture("Insert cost crypto");

        EXPECT_TRUE(rc == SQLITE_OK);
    }

    sqlite3_close(db);
    test_db_done = 1;
    __sync_synchronize();
}
static int dat_log_no_stop = 0;
void stop_that_logging(void *ptr, int status)
{
    DIGGI_ASSERT(ptr);
    auto mm = (SecureMessageManager *)ptr;
    ///stop recording efforts
    mm->StopRecording();
    dat_log_no_stop = 1;
}

static int wait_for_replay_done = 0;

static void replay_done(void *ptr, int status)
{
    auto repl_mm = (DiggiReplayManager *)ptr;
    delete repl_mm;
    wait_for_replay_done = 1;
}
static int wait_for_start_done = 0;

static void start_replay(void *ptr, int status)
{
    DIGGI_ASSERT(ptr);
    auto repl_mm = (DiggiReplayManager *)ptr;
    repl_mm->Start(replay_done, repl_mm);
    wait_for_start_done = 1;
}

TEST(diggireplay_tests, sqlite_port_record)
{
    reset_time_mock();

    telemetry_init();
    telemetry_start();

    auto threadpool1 = new ThreadPool(1);
    auto threadpool2 = new ThreadPool(1);
    auto mlog1 = new StdLogger(threadpool1);
    auto mlog2 = new StdLogger(threadpool2);

    set_syscall_interposition(1);
    remove("storage_manager0.replay.input.tamperproof.log");
    remove("storage_manager0.replay.output.tamperproof.log");
    std::string dbname = "test.db";
    remove(dbname.c_str());
    remove("test.db-journal");
    auto in_b = lf_new(RING_BUFFER_SIZE, 2, 2);
    auto out_b = lf_new(RING_BUFFER_SIZE, 2, 2);
    aid_t serv;
    aid_t cli;
    serv.raw = 0;
    serv.fields.lib = 1;
    serv.fields.type = LIB;
    cli.raw = 0;
    cli.fields.lib = 2;
    cli.fields.type = LIB;
    auto globuff = provision_memory_buffer(3, MAX_DIGGI_MEM_ITEMS, MAX_DIGGI_MEM_SIZE);
    mlog1->SetFuncId(serv, "storage_server");
    mlog2->SetFuncId(cli, "storage_manager");
    mlog1->SetLogLevel(LRELEASE);
    mlog2->SetLogLevel(LRELEASE);
    auto acontext1 = new DiggiAPI(
        threadpool1,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        mlog1,
        serv,
        nullptr);

    auto acontext2 = new DiggiAPI(
        threadpool2,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        mlog2,
        cli,
        nullptr);
    auto amm1 = new AsyncMessageManager(
        acontext1,
        in_b,
        out_b,
        std::vector<name_service_update_t>(),
        0,
        globuff);
    auto amm2 = new AsyncMessageManager(
        acontext2,
        out_b,
        in_b,
        std::vector<name_service_update_t>(),
        1,
        globuff);
    amm1->Start();
    amm2->Start();

    iostub_setcontext(acontext2, 1);

    auto mapns = std::map<std::string, aid_t>();
    mapns["file_io_func"] = serv;
    auto mm1 = new SecureMessageManager(
        acontext1,
        new NoAttestationAPI(),
        amm1,
        mapns,
        0,
        new DynamicEnclaveMeasurement(acontext1),
        new DebugCrypto(),
        false,
        false);

    acontext1->SetMessageManager(mm1);
    auto nsl = new NoSeal();
    auto ss2 = new StorageManager(acontext2, nsl);
    acontext2->SetStorageManager(ss2);
    auto mm2 = new SecureMessageManager(
        acontext2,
        new NoAttestationAPI(),
        amm2,
        mapns,
        0,
        new DynamicEnclaveMeasurement(acontext2),
        new DebugCrypto(),
        true,
        false);

    acontext2->SetMessageManager(mm2);

    auto storageserv = new StorageServer(acontext1);

    SET_DIGGI_GLOBAL_CONTEXT(acontext2);

    pthread_stubs_set_thread_manager(threadpool2);
    threadpool2->Schedule(test_init_, storageserv, __PRETTY_FUNCTION__);

    threadpool2->Schedule(test_db_cb_, (void *)dbname.c_str(), __PRETTY_FUNCTION__);
    while (!test_db_done)
        ;
    test_db_done = 0;
    remove(dbname.c_str());
    remove("test.db-journal");

    dat_log_no_stop = 0;
    threadpool2->Schedule(stop_that_logging, mm2, __PRETTY_FUNCTION__);
    while (!dat_log_no_stop)
        ;
    crc_vector_t *test = nullptr;
    ss2->GetCRCReplayVector(&test);
    vector_to_replay_stuff = *test;

    set_syscall_interposition(0);
    /*
		stop syscall interposition before stopping real threads, as this would cause an infinite loop where 
		threads join into the same threadpool its trying to stop
	*/
    threadpool1->Stop();
    threadpool2->Stop();
    amm1->Stop();
    amm2->Stop();
    delete threadpool1;
    delete threadpool2;
    delete mm1;
    delete mm2;
    delete amm1;
    delete amm2;
    delete ss2;
    mapns.clear();
    delete storageserv;
    delete acontext1;
    delete acontext2;
    delete nsl;

    lf_destroy(in_b);
    lf_destroy(out_b);
    delete_memory_buffer(globuff, MAX_DIGGI_MEM_ITEMS);
    delete mlog1;
    delete mlog2;

    pthread_stubs_unset_thread_manager();
}

TEST(diggireplay_tests, sqlite_port_replay)
{
    reset_time_mock();
    telemetry_init();
    telemetry_start();

    auto threadpool1 = new ThreadPool(1);
    auto threadpool2 = new ThreadPool(1);
    auto mlog1 = new StdLogger(threadpool1);
    auto mlog2 = new StdLogger(threadpool2);

    set_syscall_interposition(1);
    std::string dbname = "test.db";

    auto in_b = lf_new(RING_BUFFER_SIZE, 2, 2);
    auto out_b = lf_new(RING_BUFFER_SIZE, 2, 2);
    aid_t serv;
    aid_t cli;
    serv.raw = 0;
    serv.fields.lib = 1;
    serv.fields.type = LIB;
    cli.raw = 0;
    cli.fields.lib = 2;
    cli.fields.type = LIB;
    auto globuff = provision_memory_buffer(3, MAX_DIGGI_MEM_ITEMS, MAX_DIGGI_MEM_SIZE);
    mlog1->SetFuncId(serv, "storage_server");
    mlog2->SetFuncId(cli, "storage_manager");
    mlog1->SetLogLevel(LRELEASE);
    mlog2->SetLogLevel(LRELEASE);
    auto acontext1 = new DiggiAPI(
        threadpool1,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        mlog1,
        serv,
        nullptr);

    auto acontext2 = new DiggiAPI(
        threadpool2,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        mlog2,
        cli,
        nullptr);
    auto amm1 = new AsyncMessageManager(
        acontext1,
        in_b,
        out_b,
        std::vector<name_service_update_t>(),
        0,
        globuff);
    auto amm2 = new AsyncMessageManager(
        acontext2,
        out_b,
        in_b,
        std::vector<name_service_update_t>(),
        1,
        globuff);
    amm1->Start();
    amm2->Start();

    iostub_setcontext(acontext2, 1);

    auto mapns = std::map<std::string, aid_t>();
    mapns["file_io_func"] = serv;
    auto mm1 = new SecureMessageManager(
        acontext1,
        new NoAttestationAPI(),
        amm1,
        mapns,
        0,
        new DynamicEnclaveMeasurement(acontext1),
        new DebugCrypto(),
        false,
        false);

    acontext1->SetMessageManager(mm1);
    auto nsl = new NoSeal(false);
    auto ss2 = new StorageManager(acontext2, nsl);
    acontext2->SetStorageManager(ss2);
    auto mm2 = new SecureMessageManager(
        acontext2,
        new NoAttestationAPI(),
        amm2,
        mapns,
        0,
        new DynamicEnclaveMeasurement(acontext2),
        new DebugCrypto(),
        false,
        false);

    acontext2->SetMessageManager(mm2);

    auto storageserv = new StorageServer(acontext1);

    SET_DIGGI_GLOBAL_CONTEXT(acontext2);

    pthread_stubs_set_thread_manager(threadpool2);
    threadpool2->Schedule(test_init_, storageserv, __PRETTY_FUNCTION__);

    auto safe_api = new DiggiAPI(acontext2);
    std::string inp = "0.replay.input";
    std::string outp = "0.replay.output.actual";
    auto nsl2 = new NoSeal();

    auto ss_repl = new StorageManager(safe_api, nsl2);

    safe_api->SetMessageManager(mm2);

    ss_repl->SetCRCReplayVector(&vector_to_replay_stuff);
    safe_api->SetStorageManager(ss_repl);
    auto repl_mm = new DiggiReplayManager(
        safe_api->GetThreadPool(),
        mapns,
        safe_api->GetId(),
        new TamperProofLog(safe_api),
        inp,
        new TamperProofLog(safe_api),
        outp,
        safe_api->GetLogObject(),
        0);

    acontext2->SetMessageManager(repl_mm);
    EXPECT_TRUE(safe_api->GetStorageManager() != acontext2->GetStorageManager());

    /// To ensure determinism in replay we reset the time mocking.
    remove(dbname.c_str());
    remove("test.db-journal");
    threadpool2->Schedule(start_replay, repl_mm, __PRETTY_FUNCTION__);
    while (!wait_for_start_done)
        ;
    wait_for_start_done = 0;
    threadpool2->Schedule(test_db_cb_, (void *)dbname.c_str(), __PRETTY_FUNCTION__);
    while (!test_db_done)
        ;
    test_db_done = 0;
    while (!wait_for_replay_done)
        ;
    wait_for_replay_done = 0;

    set_syscall_interposition(0);
    /*
		stop syscall interposition before stopping real threads, as this would cause an infinite loop where 
		threads join into the same threadpool its trying to stop
	*/
    threadpool1->Stop();
    threadpool2->Stop();
    amm1->Stop();
    amm2->Stop();
    delete threadpool1;
    delete threadpool2;
    delete mm1;
    delete mm2;
    delete amm1;
    delete amm2;
    delete ss2;
    mapns.clear();
    delete storageserv;
    delete acontext1;
    delete acontext2;
    delete nsl;
    delete nsl2;
    delete safe_api;
    delete ss_repl;
    lf_destroy(in_b);
    lf_destroy(out_b);
    delete_memory_buffer(globuff, MAX_DIGGI_MEM_ITEMS);
    delete mlog1;
    delete mlog2;

    pthread_stubs_unset_thread_manager();
}
