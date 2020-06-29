#include <gtest/gtest.h>
#include "messaging/AsyncMessageManager.h"
#include "messaging/IMessageManager.h"
#include "threading/ThreadPool.h"
#include "messaging/ThreadSafeMessageManager.h"
#include "misc.h"
#include "messaging/SecureMessageManager.h"
#include <unistd.h>
#include "runtime/DiggiUntrustedRuntime.h"

static std::string server_name = "test_server_name";
#define TEST_MESSAGE_QUERY_TYPE (msg_type_t)123
#define CONCURRENCY 8
#define PER_THREAD_ITERATIONS 10000U
uint64_t test_multi_recieve_internal_done[CONCURRENCY] = {};
uint64_t test_multi_recieve_callback_count[CONCURRENCY] = {};
volatile uint64_t concurrrent_threads_started = 0;
volatile uint64_t concurrrent_threads = 8;
static ThreadPool *test_threadpool = nullptr;
volatile msg_delivery_t DELIVERY_TYPE = CLEARTEXT;
class TSMMMockLog : public ILog
{
    void SetFuncId(aid_t aid, std::string name = "func")
    {
    }
    void SetLogLevel(LogLevel lvl)
    {
    }
    std::string GetfuncName()
    {
        return "testfunc";
    }
    void Log(LogLevel lvl, const char *fmt, ...)
    {

        char buf[PRINTF_BUFFERSIZE] = {'\0'};
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, PRINTF_BUFFERSIZE, fmt, ap);
        va_end(ap);
        if (lvl == LRELEASE)
        {
            // printf("TestLog: Thread: %d] -: %s", test_threadpool->currentThreadId(), buf);
        }
    }
    void Log(const char *fmt, ...)
    {

        char buf[PRINTF_BUFFERSIZE] = {'\0'};
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, PRINTF_BUFFERSIZE, fmt, ap);
        va_end(ap);

        //printf("TestLog: Thread: %lu] -: %s", test_threadpool->currentThreadId(), buf);
    }
};

void incmonocounter(volatile uint64_t *monoptr)
{
    unsigned local_id = 0;
    do
    {
        local_id = *monoptr;
    } while (__sync_val_compare_and_swap(monoptr, local_id, local_id + 1) != local_id);
}
void test_type_callback(void *ptr, int status)
{
    auto ctx = (msg_async_response_t *)ptr;
    DIGGI_ASSERT(ctx);
    DIGGI_ASSERT(ctx->msg);
    DIGGI_ASSERT(!ctx->context);
    uint64_t test = *(uint64_t *)ctx->msg->data;
    if (test_multi_recieve_callback_count[test_threadpool->currentThreadId()] != test)
    {
        printf("thread %d, expected%lu, true%lu\n", test_threadpool->currentThreadId(), test_multi_recieve_callback_count[test_threadpool->currentThreadId()], test);
        //DIGGI_DEBUG_BREAK();
    }

    EXPECT_TRUE(test_multi_recieve_callback_count[test_threadpool->currentThreadId()] == test);
    EXPECT_TRUE(ctx->msg->dest.fields.thread == test_threadpool->currentThreadId());
    test_multi_recieve_callback_count[test_threadpool->currentThreadId()]++;
}

void set_register_callback(void *ptr, int status)
{
    auto tsmm = (ThreadSafeMessageManager *)ptr;
    tsmm->registerTypeCallback(test_type_callback, TEST_MESSAGE_QUERY_TYPE, nullptr);
}

void test_multi_recieve_internal(void *ptr, int status)
{
    DIGGI_ASSERT(ptr);
    DIGGI_ASSERT(test_threadpool);
    auto tsmm = (ThreadSafeMessageManager *)ptr;

    if (concurrrent_threads_started < concurrrent_threads)
    {
        incmonocounter(&concurrrent_threads_started);
        /*
			First iteration registers callback to this thread
		*/
        tsmm->registerTypeCallback(test_type_callback, TEST_MESSAGE_QUERY_TYPE, nullptr);
    }
    while (concurrrent_threads_started < concurrrent_threads)
    {
        __sync_synchronize();
    }

    if (test_multi_recieve_internal_done[test_threadpool->currentThreadId()] < PER_THREAD_ITERATIONS)
    {

        auto new_msg = tsmm->allocateMessage("test_server_name", 8, REGULAR, DELIVERY_TYPE);
        memcpy(new_msg->data, &test_multi_recieve_internal_done[test_threadpool->currentThreadId()], 8);
        new_msg->type = TEST_MESSAGE_QUERY_TYPE;
        tsmm->Send(new_msg, nullptr, nullptr);
        test_threadpool->Schedule(test_multi_recieve_internal, tsmm, __PRETTY_FUNCTION__);
        test_multi_recieve_internal_done[test_threadpool->currentThreadId()]++;
    }
}

class MockCryptoImpl : public ICryptoImplementation
{
public:
    sgx_status_t encrypt(
        const sgx_aes_gcm_128bit_key_t *p_key,
        const uint8_t *p_src,
        uint32_t src_len,
        uint8_t *p_dst,
        const uint8_t *p_iv,
        uint32_t iv_len,
        const uint8_t *p_aad,
        uint32_t aad_len,
        sgx_aes_gcm_128bit_tag_t *p_out_mac)
    {
        memcpy(p_dst, p_src, src_len);
        return SGX_SUCCESS;
    }

    sgx_status_t proc_msg1(
        const sgx_dh_msg1_t *msg1,
        sgx_dh_msg2_t *msg2,
        sgx_dh_session_t *dh_session)
    {
        return SGX_SUCCESS;
    }

    sgx_status_t decrypt(
        const sgx_aes_gcm_128bit_key_t *p_key,
        const uint8_t *p_src,
        uint32_t src_len,
        uint8_t *p_dst,
        const uint8_t *p_iv,
        uint32_t iv_len,
        const uint8_t *p_aad,
        uint32_t aad_len,
        const sgx_aes_gcm_128bit_tag_t *p_in_mac)
    {
        memcpy(p_dst, p_src, src_len);
        return SGX_SUCCESS;
    }

    sgx_status_t proc_msg3(
        const sgx_dh_msg3_t *msg3,
        sgx_dh_session_t *dh_session,
        sgx_key_128bit_t *aek,
        sgx_dh_session_enclave_identity_t *responder_identity)
    {
        return SGX_SUCCESS;
    }

    sgx_status_t init_session(sgx_dh_session_role_t role, sgx_dh_session_t *session)
    {
        return SGX_SUCCESS;
    }

    sgx_status_t gen_msg1(sgx_dh_msg1_t *msg1, sgx_dh_session_t *dh_session)
    {
        return SGX_SUCCESS;
    }
    sgx_status_t proc_msg2(
        const sgx_dh_msg2_t *msg2,
        sgx_dh_msg3_t *msg3,
        sgx_dh_session_t *dh_session,
        sgx_key_128bit_t *aek,
        sgx_dh_session_enclave_identity_t *initiator_identity)
    {
        return SGX_SUCCESS;
    }
};

TEST(threadsafe_messagemanager, test_many_send_recieve)
{

    for (unsigned x = 0; x < CONCURRENCY; x++)
    {
        test_multi_recieve_internal_done[x] = 0u;
        test_multi_recieve_callback_count[x] = 0u;
    }
    concurrrent_threads_started = 0;
    auto mlog = new TSMMMockLog();

    test_threadpool = new ThreadPool(CONCURRENCY);

    auto in_b = lf_new(RING_BUFFER_SIZE, CONCURRENCY, CONCURRENCY);
    auto out_b = lf_new(RING_BUFFER_SIZE, CONCURRENCY, CONCURRENCY);

    aid_t serv;
    serv.raw = 0;
    serv.fields.lib = 1;
    serv.fields.type = LIB;

    aid_t cli;
    cli.raw = 0;
    cli.fields.lib = 2;
    cli.fields.type = LIB;

    auto mapns = std::map<std::string, aid_t>();
    mapns[server_name] = serv;
    auto crptr = new MockCryptoImpl();
    auto globuff = provision_memory_buffer(CONCURRENCY + 1, 1024 * 1024, 1024);
    auto diggiapi1 = new DiggiAPI(test_threadpool, nullptr, nullptr, nullptr, nullptr, mlog, cli, nullptr);
    auto diggiapi2 = new DiggiAPI(test_threadpool, nullptr, nullptr, nullptr, nullptr, mlog, serv, nullptr);

    auto tmmngr1 = ThreadSafeMessageManager::Create<SecureMessageManager, AsyncMessageManager>(
        diggiapi1,
        in_b,
        out_b,
        new NoAttestationAPI(),
        mapns,
        std::vector<name_service_update_t>(),
        0,
        globuff,
        false,
        nullptr,
        false,
        crptr);
    auto tmmngr2 = ThreadSafeMessageManager::Create<SecureMessageManager, AsyncMessageManager>(
        diggiapi2,
        out_b,
        in_b,
        new NoAttestationAPI(),
        mapns,
        std::vector<name_service_update_t>(),
        0,
        globuff,
        false,
        nullptr,
        false,
        crptr);
    for (unsigned i = 0; i < CONCURRENCY; i++)
    {
        test_threadpool->ScheduleOn(i, set_register_callback, tmmngr2, __PRETTY_FUNCTION__);
    }

    for (unsigned i = 0; i < CONCURRENCY; i++)
    {
        test_threadpool->ScheduleOn(i, test_multi_recieve_internal, tmmngr1, __PRETTY_FUNCTION__);
    }

    /*
		Wait for all threads
	*/
    for (unsigned i = 0; i < CONCURRENCY; i++)
    {

        while (test_multi_recieve_internal_done[i] < PER_THREAD_ITERATIONS)
        {
            usleep(0);
        }
    }
    for (unsigned i = 0; i < CONCURRENCY; i++)
    {
        while (test_multi_recieve_callback_count[i] < PER_THREAD_ITERATIONS)
        {
            usleep(0);
        }
        EXPECT_TRUE(test_multi_recieve_callback_count[i] == PER_THREAD_ITERATIONS);
    }

    test_threadpool->Stop();
    delete tmmngr1;
    lf_destroy(in_b);
    lf_destroy(out_b);
    delete_memory_buffer(globuff, 1024 * 1024);
    delete mlog;
    delete test_threadpool;
    test_threadpool = nullptr;
}

TEST(threadsafe_messagemanager, secure_test_many_send_recieve)
{

    for (unsigned x = 0; x < CONCURRENCY; x++)
    {
        test_multi_recieve_internal_done[x] = 0u;
        test_multi_recieve_callback_count[x] = 0u;
    }
    concurrrent_threads_started = 0;
    auto mlog = new TSMMMockLog();
    DELIVERY_TYPE = ENCRYPTED;
    test_threadpool = new ThreadPool(CONCURRENCY);

    auto in_b = lf_new(RING_BUFFER_SIZE, CONCURRENCY, CONCURRENCY);
    auto out_b = lf_new(RING_BUFFER_SIZE, CONCURRENCY, CONCURRENCY);

    aid_t serv;
    serv.raw = 0;
    serv.fields.enclave = 1;
    serv.fields.type = ENCLAVE;

    aid_t cli;
    cli.raw = 0;
    cli.fields.enclave = 2;
    cli.fields.type = ENCLAVE;

    auto mapns = std::map<std::string, aid_t>();
    mapns[server_name] = serv;
    auto crptr = new MockCryptoImpl();
    auto globuff = provision_memory_buffer(CONCURRENCY + 1, 1024 * 1024, 1024);
    auto diggiapi1 = new DiggiAPI(test_threadpool, nullptr, nullptr, nullptr, nullptr, mlog, cli, nullptr);
    auto diggiapi2 = new DiggiAPI(test_threadpool, nullptr, nullptr, nullptr, nullptr, mlog, serv, nullptr);

    auto tmmngr1 = ThreadSafeMessageManager::Create<SecureMessageManager, AsyncMessageManager>(
        diggiapi1,
        in_b,
        out_b,
        new NoAttestationAPI(),
        mapns,
        std::vector<name_service_update_t>(),
        0,
        globuff,
        false,
        nullptr,
        false,
        crptr);
    auto tmmngr2 = ThreadSafeMessageManager::Create<SecureMessageManager, AsyncMessageManager>(
        diggiapi2,
        out_b,
        in_b,
        new NoAttestationAPI(),
        mapns,
        std::vector<name_service_update_t>(),
        0,
        globuff,
        false,
        nullptr,
        false,
        crptr);
    for (unsigned i = 0; i < CONCURRENCY; i++)
    {
        test_threadpool->ScheduleOn(i, set_register_callback, tmmngr2, __PRETTY_FUNCTION__);
    }

    for (unsigned i = 0; i < CONCURRENCY; i++)
    {
        test_threadpool->ScheduleOn(i, test_multi_recieve_internal, tmmngr1, __PRETTY_FUNCTION__);
    }

    /*
		Wait for all threads
	*/
    for (unsigned i = 0; i < CONCURRENCY; i++)
    {

        while (test_multi_recieve_internal_done[i] < PER_THREAD_ITERATIONS)
        {
            usleep(0);
        }
    }
    for (unsigned i = 0; i < CONCURRENCY; i++)
    {
        while (test_multi_recieve_callback_count[i] < PER_THREAD_ITERATIONS)
        {
            usleep(0);
        }
        EXPECT_TRUE(test_multi_recieve_callback_count[i] == PER_THREAD_ITERATIONS);
    }

    test_threadpool->Stop();
    delete tmmngr1;
    lf_destroy(in_b);
    lf_destroy(out_b);
    delete_memory_buffer(globuff, 1024 * 1024);
    delete mlog;
    delete test_threadpool;
    test_threadpool = nullptr;
}
