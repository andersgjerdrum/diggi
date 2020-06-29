#include <gtest/gtest.h>
#include "misc.h"
#include "datatypes.h"
#include "messaging/AsyncMessageManager.h"
#include "Logging.h"
#include "runtime/DiggiUntrustedRuntime.h"
#include "runtime/DiggiAPI.h"
class MockLog : public ILog
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
static MockLog loginstance;

class MockThreadPool : public IThreadPool
{
    int rounds;

public:
    MockThreadPool(int rnds) : rounds(rnds) {}

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

static msg_t *last_recv = nullptr;

void test_recv_handler(void *ptr, int status)
{
    auto last_recv_ctx = (msg_async_response_t *)ptr;
    EXPECT_TRUE(last_recv_ctx != nullptr);
    EXPECT_TRUE(last_recv_ctx->msg != nullptr);
    EXPECT_TRUE(last_recv_ctx->context == nullptr);
    last_recv = COPY(msg_t, last_recv_ctx->msg, last_recv_ctx->msg->size);
}

TEST(asyncmessagemanager, simple_source_sink_test)
{
    auto mktp = new MockThreadPool(0);
    auto input = lf_new(RING_BUFFER_SIZE, 1, 1);
    auto output = lf_new(RING_BUFFER_SIZE, 1, 1);
    aid_t src;
    src.raw = 0;
    src.fields.lib = 2;
    auto globuff = provision_memory_buffer(1, MAX_DIGGI_MEM_ITEMS, MAX_DIGGI_MEM_SIZE);
    auto dapi = new DiggiAPI(mktp, nullptr, nullptr, nullptr, nullptr, &loginstance, src, nullptr);
    auto amm = new AsyncMessageManager(dapi, input, output, std::vector<name_service_update_t>(), 0, globuff);
    auto str = new std::string("heyman");

    aid_t dest;
    dest.raw = 0;
    dest.fields.lib = 1;

    amm->registerTypeCallback(test_recv_handler, REGULAR_MESSAGE, nullptr);
    amm->Start();
    auto itm = lf_recieve(globuff, 0);
    auto msg = (msg_t *)itm;
    msg->type = REGULAR_MESSAGE;
    msg->id = 0;
    msg->src = src;
    msg->dest = dest;
    msg->size = str->size() + sizeof(msg_t);
    memcpy(msg->data, str->c_str(), str->size());

    auto forcompare = COPY(msg_t, msg, msg->size);
    lf_send(input, itm, 0);
    AsyncMessageManager::async_message_pump(amm, 1);

    EXPECT_TRUE(last_recv->size == forcompare->size);
    EXPECT_TRUE(memcmp(last_recv->data, forcompare->data, forcompare->size - sizeof(msg_t)) == 0);

    auto msg2 = amm->allocateMessage(forcompare, str->size());
    memcpy(msg2->data, str->c_str(), str->size());

    free(forcompare);
    amm->sendMessage(msg2);

    auto itm2 = lf_recieve(output, 0);
    EXPECT_TRUE(itm2 != nullptr);
    auto last_sent = (msg_t *)itm2;
    EXPECT_TRUE(last_sent->size == msg2->size);
    lf_send(globuff, itm2, 0);

    EXPECT_TRUE(memcmp(last_sent->data, msg2->data, msg2->size - sizeof(msg_t)) == 0);

    lf_destroy(input);
    lf_destroy(output);
    delete_memory_buffer(globuff, MAX_DIGGI_MEM_ITEMS);
    delete amm;
    delete mktp;
    delete str;
}

static bool sink_called = false;

TEST(asyncmessagemanager, simple_continuation_source_sink)
{

    struct Callback
    {
        static void simple_source_sink_test_cb(void *ptr, int status)
        {
            auto resp = (msg_async_response_t *)ptr;
            EXPECT_TRUE(resp != nullptr);
            EXPECT_TRUE(resp->msg->id != 0);
            sink_called = true;
            auto amm_t = (AsyncMessageManager *)resp->context;
            amm_t->endAsync(resp->msg);
            EXPECT_TRUE(resp->msg->id != 0);
            // changed because not copying messages between endpoints requires us to not
            // change message once put on outbound queue
        };
    };

    auto input = lf_new(RING_BUFFER_SIZE, 1, 1);
    auto output = lf_new(RING_BUFFER_SIZE, 1, 1);

    auto mktp = new MockThreadPool(0);
    aid_t src;
    src.raw = 0;
    src.fields.lib = 2;
    auto globuff = provision_memory_buffer(1, MAX_DIGGI_MEM_ITEMS, MAX_DIGGI_MEM_SIZE);
    auto dapi = new DiggiAPI(mktp, nullptr, nullptr, nullptr, nullptr, &loginstance, src, nullptr);

    auto amm = new AsyncMessageManager(dapi, input, output, std::vector<name_service_update_t>(), 0, globuff);
    amm->Start();

    aid_t dest;
    dest.raw = 0;
    dest.fields.lib = 0;

    auto str = new std::string("heyman");
    auto itm = lf_recieve(globuff, 0);
    auto msg = (msg_t *)itm;
    msg->type = REGULAR_MESSAGE;
    msg->id = 1234;
    msg->src = src;
    msg->dest = dest;
    msg->size = str->size() + sizeof(msg_t);

    memcpy(msg->data, str->c_str(), str->size());
    amm->sendMessageAsync(msg, Callback::simple_source_sink_test_cb, amm);
    auto itm2 = lf_recieve(output, 0);
    auto last_sent = (msg_t *)itm2;
    EXPECT_TRUE(msg->size == last_sent->size);
    EXPECT_TRUE(memcmp(last_sent->data, msg->data, msg->size - sizeof(msg_t)) == 0);
    EXPECT_TRUE(!sink_called);
    auto itm3 = lf_recieve(globuff, 0);
    memcpy(itm3, last_sent, last_sent->size);
    lf_send(globuff, last_sent, 0);

    auto lastmsg = (msg_t *)itm3;

    lastmsg->dest = msg->src;
    lastmsg->src = msg->dest;

    lf_send(input, itm3, 0);
    AsyncMessageManager::async_message_pump(amm, 1);

    EXPECT_TRUE(sink_called);
    lf_destroy(input);
    lf_destroy(output);
    delete_memory_buffer(globuff, MAX_DIGGI_MEM_ITEMS);

    delete amm;
    delete mktp;
    delete str;
}