#include <gtest/gtest.h>
#include "messaging/ThreadSafeMessageManager.h"
#include "storage/TamperProofLog.h"
#include "AsyncContext.h"
#include "storage/StorageServer.h"
#include "posix/pthread_stubs.h"
#include "Logging.h"
#include "storage/StorageManager.h"
#include "Seal.h"
#include "threading/ThreadPool.h"
#include "messaging/SecureMessageManager.h"
static volatile int test_db_done = 0;
static const char *testmsg = "hey there!\n";

//offset one encrypted block to test partially filled end block
#define MAX_APPEND_LOOP_TEST_COUNT 1001
static volatile int append_log_count_loop = 0;
typedef AsyncContext<TamperProofLog *, DiggiAPI *> text_context_t;
static std::vector<msg_t *> free_list;
void tamperproof_test_init(void *ptr, int status)
{
    auto storageserv = (StorageServer *)ptr;
    storageserv->initializeServer();
}
void replay_loop_internal(void *ptr, int status)
{

    auto resp = (msg_async_response_t *)ptr;
    DIGGI_ASSERT(resp);
    DIGGI_ASSERT(resp->context);
    DIGGI_ASSERT(resp->msg);
    EXPECT_TRUE(resp->msg->size == 1024 + sizeof(msg_t));
    EXPECT_TRUE(memcmp(resp->msg->data, testmsg, strlen(testmsg)) == 0);
    append_log_count_loop++;
}

void replay_loop_completion_cb(void *ptr, int status)
{
    DIGGI_ASSERT(append_log_count_loop == MAX_APPEND_LOOP_TEST_COUNT);
    for (auto msgptr : free_list)
    {
        free(msgptr);
    }
    test_db_done = 1;
}

void test_method_loop(void *ptr, int status)
{
    auto ctx = (text_context_t *)ptr;
    if (append_log_count_loop++ < MAX_APPEND_LOOP_TEST_COUNT)
    {
        auto msg = ALLOC_P(msg_t, 1024);
        msg->size = sizeof(msg_t) + 1024;
        memcpy(msg->data, testmsg, strlen(testmsg));
        ctx->item1->appendLogEntry(msg);
        free_list.push_back(msg);
        ctx->item2->GetThreadPool()->Schedule(test_method_loop, ctx, __PRETTY_FUNCTION__);
    }
    else
    {
        append_log_count_loop = 0;

        ctx->item1->Stop();
        ctx->item1->replayLogEntry("test_log_identifier", replay_loop_internal, replay_loop_completion_cb, ctx, ctx);
    }
}
void test_method_on_thread(void *ptr, int status)
{
    DIGGI_ASSERT(ptr);
    auto api = (DiggiAPI *)ptr;
    auto tpl = new TamperProofLog(api);
    auto ctx = new text_context_t(tpl, api);
    tpl->initLog(WRITE_LOG, "test_log_identifier", test_method_loop, ctx);
}

TEST(tamperprooflogtests, appendlog)
{
    telemetry_init();
    telemetry_start();

    auto threadpool1 = new ThreadPool(1);
    auto threadpool2 = new ThreadPool(1);
    auto mlog1 = new StdLogger(threadpool1);
    auto mlog2 = new StdLogger(threadpool2);

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
    auto nsl = new NoSeal();
    auto ss2 = new StorageManager(acontext2, nsl);
    acontext2->SetStorageManager(ss2);

    auto storageserv = new StorageServer(acontext1);

    SET_DIGGI_GLOBAL_CONTEXT(acontext2);

    std::string dbname = "test.db";
    threadpool2->Schedule(tamperproof_test_init, storageserv, __PRETTY_FUNCTION__);
    threadpool2->Schedule(test_method_on_thread, acontext2, __PRETTY_FUNCTION__);

    while (!test_db_done)
        ;
    test_db_done = 0;
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
}