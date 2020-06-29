#include "runtime/func.h"

#include "messaging/IMessageManager.h"
#include "DiggiAssert.h"
#include <inttypes.h>
#include "runtime/DiggiAPI.h"
/*Com func*/
#define TEST_BENCHMARK_MESSAGE_TYPE (msg_type_t)911

static size_t __thread iterations[[gnu::unused]] = 100000;
static bool infinite = false;
void recieve_message_callback(void *msg, int status)
{

    auto ctx = (msg_async_response_t *)msg;
    DIGGI_ASSERT(ctx);
    DIGGI_ASSERT(ctx->msg);
    DIGGI_ASSERT(ctx->context);

    ctx->msg->data[ctx->msg->size - sizeof(msg_t) - 1] = '\0';
    auto a_cont = (DiggiAPI *)ctx->context;
    a_cont->GetLogObject()->Log(LDEBUG,
                                "Server recieved data from enclave %" PRIu64 " of size: %lu, id:%lu, expected thread:%lu, actual thread %d, with message: %d\n",
                                ctx->msg->src.raw,
                                ctx->msg->size - sizeof(msg_t),
                                ctx->msg->id,
                                (size_t)ctx->msg->dest.fields.thread,
                                a_cont->GetThreadPool()->currentThreadId(),
                                *(int*)ctx->msg->data);
    /* Do not reuse messages
	TODO: support detecting message reuse */ 
    auto new_msg = a_cont->GetMessageManager()->allocateMessage(ctx->msg, ctx->msg->size - sizeof(msg_t));

    /*
		We expect threads to surface back on the same thread, i.e 
		1:1 between threads on client:server.
	*/

    DIGGI_ASSERT(ctx->msg->dest.fields.thread == (uint8_t)a_cont->GetThreadPool()->currentThreadId());
    new_msg->dest = ctx->msg->src;
    new_msg->src = ctx->msg->dest;
    memcpy((void *)new_msg->data, (void * )ctx->msg->data, 8);

    a_cont->GetMessageManager()->Send(new_msg, nullptr, nullptr);
    if (!infinite)
    {
        iterations--;
        if (iterations == 0)
        {
            a_cont->GetSignalHandler()->Stopfunc();
        }
    }
}
void stop_test_server_func(void *ptr, int status)
{
    auto a_cont = (DiggiAPI *)ptr;
    DIGGI_ASSERT(a_cont);
    a_cont->GetLogObject()->Log(LRELEASE, "Stop Message Recieved\n");

    a_cont->GetSignalHandler()->Stopfunc();
}
void register_callback_per_thread(void *ptr, int status)
{
    auto a_cont = (DiggiAPI *)ptr;
    DIGGI_ASSERT(a_cont);
    if (a_cont->GetFuncConfig().contains("request-count"))
    {
        iterations = (size_t)atoi(a_cont->GetFuncConfig()["request-count"].value.tostring().c_str());
    }
    else
    {
        infinite = true;
    }

    a_cont->GetMessageManager()->registerTypeCallback(recieve_message_callback, REGULAR_MESSAGE, a_cont);
    a_cont->GetMessageManager()->registerTypeCallback(stop_test_server_func, TEST_BENCHMARK_MESSAGE_TYPE, a_cont);
}

void func_init(void *ctx, int status)
{
    DIGGI_ASSERT(ctx);
    auto a_cont = (DiggiAPI *)ctx;
    auto thread_p = a_cont->GetThreadPool();

    for (unsigned th = 0; th < thread_p->physicalThreadCount(); th++)
    {
        thread_p->ScheduleOn(th, register_callback_per_thread, ctx, __PRETTY_FUNCTION__);
    }
}
void func_start(void *ctx, int status)
{
    DIGGI_ASSERT(ctx);

    //do nothing
}

/*Simple loopback test*/

void func_stop(void *ctx, int status)
{
    DIGGI_ASSERT(ctx);
    auto a_cont = (DiggiAPI *)ctx;
    a_cont->GetLogObject()->Log(LRELEASE, "Stopping test server func\n");
}