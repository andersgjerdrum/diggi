#include <gtest/gtest.h>
#include "datatypes.h"
#include "messaging/IAsyncMessageManager.h"
#include "messaging/SecureMessageManager.h"
#include "DiggiAssert.h"
#include "Logging.h"

static bool handshake_response = false;
static bool handle_sink_done = false;
static const char *test_data = "getdata";

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
        if (rounds-- > 0)
        {
            cb(args, 1);
        }
    }
    size_t physicalThreadCount()
    {
        return 1;
    }
    void ScheduleOn(size_t id, async_cb_t cb, void *args, const char *label)
    {
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

unsigned long unique_identifier = 0;

class MockClientAsyncMessageManager : public IAsyncMessageManager
{
    async_cb_t cb_r;
    void *ptr_r;
    async_cb_t cb_s = nullptr;
    void *ptr_s;
    aid_t self_r;
    aid_t other_r;

public:
    size_t payload_r;
    async_cb_t sinkr;

    MockClientAsyncMessageManager(aid_t self, aid_t other, size_t payload, async_cb_t sink) : self_r(self), other_r(other), payload_r(payload), sinkr(sink) {}
    MockClientAsyncMessageManager(aid_t self, aid_t other, size_t payload) : self_r(self), other_r(other), payload_r(payload) {}
    ~MockClientAsyncMessageManager(){};
    void UnregisterTypeCallback(msg_type_t ty)
    {
    }
    unsigned long getMessageId(unsigned long func_identifier)
    {
        return unique_identifier++;
    }
    void registerTypeCallback(async_cb_t cb, msg_type_t ty, void *arg)
    {
        if (ty == SESSION_REQUEST)
        {
            cb_s = cb;
            EXPECT_TRUE(cb_s != nullptr);

            ptr_s = arg;
            EXPECT_TRUE(ptr_s != nullptr);
        }
        else if (ty == REGULAR_MESSAGE)
        {
            cb_r = cb;
            EXPECT_TRUE(cb_r != nullptr);
            ptr_r = arg;
            EXPECT_TRUE(ptr_r != nullptr);
        }
        else
        {
            EXPECT_TRUE(false);
        }
    }
    void Start()
    {
    }
    /*
	different thread
	*/
    void handleIncommingMessage(msg_t *msg)
    {
        if (cb_s != nullptr && msg->type == SESSION_REQUEST)
        {
            auto async_s = new msg_async_response_t();
            async_s->msg = msg;
            async_s->context = ptr_s;
            cb_s(async_s, 1);
            delete async_s;
        }
        else if (msg->type == REGULAR_MESSAGE)
        {
            EXPECT_TRUE(cb_r != nullptr);
            EXPECT_TRUE(ptr_r != nullptr);
            auto async_r = new msg_async_response_t();
            async_r->msg = msg;
            async_r->context = ptr_r;
            cb_r(async_r, 1);
            delete async_r;
        }
        else
        {
            DIGGI_ASSERT(msg->type == SESSION_REQUEST);
            handshake_response = true;
        }
    }
    void Stop()
    {
    }

    msg_t *allocateMessage(msg_t *msg, size_t payload_size)
    {
        EXPECT_TRUE(payload_size == payload_r);
        EXPECT_TRUE(msg->src.raw == other_r.raw);
        EXPECT_TRUE(msg->dest.raw == self_r.raw);
        auto msg_n = ALLOC_P(msg_t, payload_size);
        msg_n->size = sizeof(msg_t) + payload_size;
        msg_n->src = msg->src;
        msg_n->type = msg->type;
        msg_n->dest = msg->dest;
        msg_n->id = msg->id;
        return msg_n;
    }

    msg_t *allocateMessage(aid_t source, aid_t dest, size_t payload_size, msg_convention_t async)
    {
        EXPECT_TRUE(payload_size == payload_r);
        auto msg = ALLOC_P(msg_t, payload_size);
        msg->size = sizeof(msg_t) + payload_size;
        msg->src = source;
        msg->dest = dest;
        return msg;
    }

    /*await response*/
    void sendMessageAsync(msg_t *msg, async_cb_t cb, void *ptr)
    {
        if (msg->type == SESSION_REQUEST)
        {

            EXPECT_TRUE(cb != nullptr);
            auto ctx = (key_exchange_context_t *)ptr;
            EXPECT_TRUE(ctx->self_id.raw == msg->src.raw);
            EXPECT_TRUE(ctx->other_id.raw == msg->dest.raw);
            EXPECT_TRUE(ctx->session_id_inbound == 0);
            EXPECT_TRUE(ctx->session_id_outbound == 0);
            EXPECT_TRUE(ctx->outputqueue.size() > 0);
            EXPECT_TRUE(ctx != nullptr);
            auto asresp = new msg_async_response_t();
            asresp->msg = msg;
            asresp->context = ptr;
            cb(asresp, 1);
        }
        else
        {
            DIGGI_ASSERT(msg->type == REGULAR_MESSAGE);
            sinkr(msg, 1);
        }
    }

    void endAsync(msg_t *msg)
    {
    }

    /*one way*/
    void sendMessage(msg_t *msg)
    {
        EXPECT_TRUE(msg->src.raw == self_r.raw);
        EXPECT_TRUE(msg->dest.raw == other_r.raw);

        if (msg->type == REGULAR_MESSAGE)
        {
            sinkr(msg, 1);
        }
        else
        {
            EXPECT_TRUE(msg->size == sizeof(msg_t));
        }
    }
};

TEST(messagemanager, simple_client_side_handshake_test)
{

    aid_t d;
    aid_t serv;
    d.raw = 0;
    d.fields.lib = 2;
    d.fields.type = LIB;
    serv.raw = 0;
    serv.fields.lib = 0;
    auto async = new MockClientAsyncMessageManager(d, serv, 0);
    auto mktpl = new MockThreadPool(0);
    auto mklog = new MockLog();
    auto diggiapi = new DiggiAPI(mktpl, nullptr, nullptr, nullptr, nullptr, mklog, d, nullptr);
    auto mngr = new SecureMessageManager(
        diggiapi,
        new NoAttestationAPI(),
        async,
        std::map<std::string, aid_t>(),
        0,
        nullptr,
        new DebugCrypto(),
        false,
        false);
    auto incoming_msg = (msg_t *)malloc(sizeof(msg_t));
    incoming_msg->src = serv;
    incoming_msg->type = SESSION_REQUEST;
    incoming_msg->dest = d;
    incoming_msg->session_count = 0;
    async->handleIncommingMessage(incoming_msg);
    delete mngr;
    delete async;
    delete mktpl;
    delete mklog;
    free(incoming_msg);
}

TEST(messagemanager, simple_server_side_handshake_test)
{

    std::map<std::string, aid_t> name_servicemap;

    aid_t d;
    aid_t serv;
    d.raw = 0;
    d.fields.lib = 2;
    d.fields.type = LIB;
    serv.raw = 0;
    serv.fields.lib = 0;
    auto async = new MockClientAsyncMessageManager(serv, d, 0);
    auto mktpl = new MockThreadPool(0);
    auto mklog = new MockLog();
    auto diggiapi = new DiggiAPI(mktpl, nullptr, nullptr, nullptr, nullptr, mklog, serv, nullptr);
    auto mngr = new SecureMessageManager(
        diggiapi,
        new NoAttestationAPI(),
        async,
        std::map<std::string, aid_t>(),
        0,
        nullptr,
        new DebugCrypto(),
        false,
        false);

    auto incoming_msg = (msg_t *)malloc(sizeof(msg_t));
    incoming_msg->src = d;
    incoming_msg->type = SESSION_REQUEST;
    incoming_msg->delivery = CLEARTEXT;
    incoming_msg->dest = serv;
    incoming_msg->session_count = 0;
    async->handleIncommingMessage(incoming_msg);
    delete mngr;
    delete async;
    free(incoming_msg);
    delete mklog;
    delete mktpl;
}

void mock_sink_handler(void *ptr, int status)
{

    handle_sink_done = true;
    auto msg = (msg_t *)ptr;
    EXPECT_TRUE(msg->size == sizeof(msg_t) + 8);
    std::string str((char *)&(msg->data[0]), 8);
    EXPECT_TRUE(strcmp(test_data, str.c_str()) == 0);
}

TEST(messagemanager, simple_server_side_send)
{

    std::map<std::string, aid_t> name_servicemap;

    aid_t d;
    aid_t serv;
    d.raw = 0;
    d.fields.lib = 2;
    serv.raw = 0;
    d.fields.type = LIB;
    serv.fields.type = LIB;
    serv.fields.lib = 0;
    name_servicemap["test_dest"] = d;
    auto async = new MockClientAsyncMessageManager(serv, d, 0, mock_sink_handler);
    auto mktpl = new MockThreadPool(0);
    auto mklog = new MockLog();
    auto diggiapi = new DiggiAPI(mktpl, nullptr, nullptr, nullptr, nullptr, mklog, serv, nullptr);
    auto mngr = new SecureMessageManager(
        diggiapi,
        new NoAttestationAPI(),
        async,
        name_servicemap,
        0,
        nullptr,
        new DebugCrypto(),
        false,
        false);

    auto incoming_msg = (msg_t *)malloc(sizeof(msg_t));
    incoming_msg->src = d;
    incoming_msg->type = SESSION_REQUEST;
    incoming_msg->delivery = CLEARTEXT;
    incoming_msg->dest = serv;
    incoming_msg->session_count = 0;
    async->handleIncommingMessage(incoming_msg);
    async->payload_r = 8;

    auto new_msg = mngr->allocateMessage("test_dest", 8, REGULAR, CLEARTEXT);
    new_msg->type = REGULAR_MESSAGE;

    memcpy((void *)new_msg->data, (void *)test_data, 8);
    mngr->Send(new_msg, nullptr, nullptr);

    free(new_msg);
    delete mngr;
    delete async;
    free(incoming_msg);
    delete mklog;
    delete mktpl;
}
