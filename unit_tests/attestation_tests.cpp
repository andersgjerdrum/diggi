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
#include "runtime/DiggiUntrustedRuntime.h"
#include "sgx/DynamicEnclaveMeasurement.h"

static volatile int test_attestation_done = 0;
static volatile int trusted_root_server_done = 0;
void test_attestation_done_cb(void *ptr, int status)
{
    auto resp = (msg_async_response_t *)ptr;
    auto acontext = (DiggiAPI *)resp->context;
    acontext->GetMessageManager()->endAsync(resp->msg);

    test_attestation_done = 1;
}

void test_attestation_start(void *ptr, int status)
{
    auto acontext = (DiggiAPI *)ptr;

    auto msg = acontext->GetMessageManager()->allocateMessage("trusted_root_func", 1024, CALLBACK, ENCRYPTED);
    msg->type = REGULAR_MESSAGE;
    acontext->GetMessageManager()->Send(msg, test_attestation_done_cb, acontext);
}
void trusted_root_server_done_cb(void *ptr, int status)
{
    auto resp = (msg_async_response_t *)ptr;
    auto acontext = (DiggiAPI *)resp->context;
    auto msg = acontext->GetMessageManager()->allocateMessage(resp->msg, 1024);
    msg->dest = resp->msg->src;
    msg->src = resp->msg->dest;
    acontext->GetMessageManager()->Send(msg, nullptr, nullptr);
    acontext->GetMessageManager()->endAsync(resp->msg);
    trusted_root_server_done = 1;
}
void register_callback_handler(void *ptr, int status)
{
    auto acontext1 = (DiggiAPI *)ptr;
    acontext1->GetMessageManager()->registerTypeCallback(trusted_root_server_done_cb, REGULAR_MESSAGE, acontext1);
}

void attest_test_cb(void *ptr, int status)
{
    auto act1 = (DiggiAPI *)ptr;
    uint8_t msrinit[ATTESTATION_HASH_SIZE] = {0};
    auto att = new DynamicEnclaveMeasurement((uint8_t *)&msrinit, ATTESTATION_HASH_SIZE, act1);

    att->update((uint8_t *)&msrinit, ATTESTATION_HASH_SIZE);
    auto ret = att->get();
    EXPECT_TRUE(ret != nullptr);
    delete att;
}
TEST(attestation_tests, dynamic_attestation)
{

    auto threadpool1 = new ThreadPool(1);
    auto mlog1 = new StdLogger(threadpool1);

    aid_t cli = {0};

    auto acontext1 = new DiggiAPI(
        threadpool1,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        mlog1,
        cli,
        nullptr);

    threadpool1->Schedule(attest_test_cb, acontext1, __PRETTY_FUNCTION__);
}

TEST(attestation_tests, no_attest)
{
    test_attestation_done = 0;
    trusted_root_server_done = 0;
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
    serv.fields.att_group = 1;
    cli.raw = 0;
    cli.fields.lib = 2;
    cli.fields.type = LIB;
    cli.fields.att_group = 1;
    auto globuff = provision_memory_buffer(3, MAX_DIGGI_MEM_ITEMS, MAX_DIGGI_MEM_SIZE);
    mlog1->SetFuncId(serv, "trusted_root_func");
    mlog2->SetFuncId(cli, "client_func");
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

    auto mapns = std::map<std::string, aid_t>();
    mapns["trusted_root_func"] = serv;

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
    threadpool1->Schedule(register_callback_handler, acontext1, __PRETTY_FUNCTION__);
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
    amm1->Start();
    amm2->Start();
    threadpool2->Schedule(test_attestation_start, acontext2, __PRETTY_FUNCTION__);
    while (!test_attestation_done)
        ;
    while (!trusted_root_server_done)
        ;
    trusted_root_server_done = 0;
    test_attestation_done = 0;
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
    mapns.clear();
    delete acontext1;
    delete acontext2;

    lf_destroy(in_b);
    lf_destroy(out_b);
    delete_memory_buffer(globuff, MAX_DIGGI_MEM_ITEMS);
    delete mlog1;
    delete mlog2;
}