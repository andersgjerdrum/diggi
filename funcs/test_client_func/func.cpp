#include "runtime/func.h"
#include "messaging/IMessageManager.h"
#include "DiggiAssert.h"
#include <inttypes.h>
#include "runtime/DiggiAPI.h"
#include "messaging/Util.h"
#include "telemetry.h"
/*Simple Client func*/

/*
	Each thread does x iterations
*/
static size_t __thread iterations[[gnu::unused]] = 100000;

/*do not yield here, another thread entirely*/
void func_init(void *ctx, int status)
{

    /*
		Workaround until telemetry code is refactored
	*/
#ifndef DIGGI_ENCLAVE
    //telemetry_init();
    //telemetry_start();
#endif
    DIGGI_ASSERT(ctx);
}

void internal_message_iteration_looper(void *ptr, int status)
{

    auto ctx = (msg_async_response_t *)ptr;
    DIGGI_ASSERT(ctx);
    DIGGI_ASSERT(ctx->msg);
    DIGGI_ASSERT(ctx->context);
    auto a_cont = (DiggiAPI *)ctx->context;
    ctx->msg->data[ctx->msg->size - sizeof(msg_t) - 1] = '\0';

    DIGGI_ASSERT(memcmp(ctx->msg->data, "getdata", 7) == 0);
    a_cont->GetLogObject()->Log(LDEBUG, "Client recieved data from server %" PRIu64 " with id:%lu, size:%lu, message: %s \n",
                                ctx->msg->src.raw,
                                ctx->msg->id,
                                ctx->msg->size - sizeof(msg_t),
                                ctx->msg->data);
    /*
		We expect threads to surface back on the same thread, i.e
		1:1 between threads on client:server.
	*/
    DIGGI_ASSERT(ctx->msg->dest.fields.thread == (uint8_t)a_cont->GetThreadPool()->currentThreadId());
    a_cont->GetMessageManager()->endAsync(ctx->msg);

    auto secmngr = a_cont->GetMessageManager();
    if (iterations > 0)
    {
        iterations--;
        a_cont->GetLogObject()->Log(LDEBUG,
                                    "Client internal_message_iteration_looper iterations left=%lu \n",
                                    iterations);
        auto new_msg = secmngr->allocateMessage(a_cont->GetFuncConfig()["connected-to"].value.tostring(), 1024, CALLBACK,  a_cont->GetFuncConfig().contains("messageencryption") ? ENCRYPTED: CLEARTEXT);
        new_msg->type = REGULAR_MESSAGE;
        memcpy((void *)new_msg->data, (void *)"getdata", 8);
        secmngr->Send(new_msg, internal_message_iteration_looper, a_cont);

        /*nonsense yield for testing yield facility*/
        a_cont->GetThreadPool()->Yield();
    }
    else
    {
        a_cont->GetLogObject()->Log(LRELEASE, "test client func: Test request session done\n");
        a_cont->GetSignalHandler()->Stopfunc();
        //telemetry_capture("funcreq_Stop");
    }
}

void func_start_per_thread(void *ctx, int status)
{
    auto a_cont = (DiggiAPI *)ctx;
    DIGGI_ASSERT(a_cont);
    if (a_cont->GetFuncConfig().contains("request-count"))
    {
        iterations = (size_t)atoi(a_cont->GetFuncConfig()["request-count"].value.tostring().c_str());
    }
    auto secmngr = (IMessageManager *)a_cont->GetMessageManager();
    a_cont->GetLogObject()->Log(LRELEASE, "test client func: Starting request test\n");
    auto new_msg = secmngr->allocateMessage(a_cont->GetFuncConfig()["connected-to"].value.tostring(), 1024, CALLBACK,  a_cont->GetFuncConfig().contains("messageencryption") ? ENCRYPTED: CLEARTEXT);
    memcpy(new_msg->data, (void *)"getdata", 8);
    new_msg->type = REGULAR_MESSAGE;
    //telemetry_capture("funcreq_Start");
    iterations--;
    secmngr->Send(new_msg, internal_message_iteration_looper, a_cont);
}
void func_start(void *ctx, int status)
{

    auto a_cont = (DiggiAPI *)ctx;
    DIGGI_ASSERT(a_cont);
    auto thread_p = a_cont->GetThreadPool();
    for (unsigned th = 0; th < thread_p->physicalThreadCount(); th++)
    {
        a_cont->GetLogObject()->Log(LRELEASE, "Scheduled func_start_per_thread() onto thread %lu\n", th);

        thread_p->ScheduleOn(th, func_start_per_thread, ctx, __PRETTY_FUNCTION__);
    }
}

void func_stop(void *ctx, int status)
{
    DIGGI_ASSERT(ctx);
    auto con = (DiggiAPI *)ctx;
#ifndef DIGGI_ENCLAVE
    //telemetry_write();
#endif
    con->GetLogObject()->Log(LRELEASE, "Stopping Client func\n");
}
