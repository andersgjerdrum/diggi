#include "messaging/IMessageManager.h"
#include "messaging/Util.h"
#include "DiggiAssert.h"
#include "runtime/func.h"
#include <inttypes.h>
#include "runtime/DiggiAPI.h"
#include "DiggiGlobal.h"
#include <chrono>
#include <ctime>
#include <numeric>   
/* latency Client func*/

/*
	Each thread does x iterations
*/

#define TEST_BENCHMARK_MESSAGE_TYPE (msg_type_t)911
static volatile size_t threads_done = 0;
static std::vector<long long> latency;
static std::chrono::time_point<std::chrono::high_resolution_clock> start;
static int request_count = 0;
/*do not yield here, another thread entirely*/
void func_init(void *ctx, int status)
{
    DIGGI_ASSERT(ctx);
}

void message_response(void *ptr, int status);

void message_start(void *ptr, int status)
{
    auto a_cont = (DiggiAPI *)ptr;
    DIGGI_ASSERT(a_cont);
    int val = (size_t)atoi(a_cont->GetFuncConfig()["Duration"].value.tostring().c_str());
    size_t packagesize = 1024;
    if(a_cont->GetFuncConfig().contains("package-size")){
        packagesize = (size_t)atoi(a_cont->GetFuncConfig()["package-size"].value.tostring().c_str());
    }
    

    auto duration = std::chrono::duration<double>(val);
    if ((std::chrono::high_resolution_clock::now() - start) < duration)
    {
        a_cont->GetLogObject()->Log(LDEBUG,
                                    "Client internal_message_iteration_looper iterations=%d \n",
                                    request_count);
        auto new_msg = a_cont->GetMessageManager()->allocateMessage(a_cont->GetFuncConfig()["connected-to"].value.tostring(), packagesize, CALLBACK, a_cont->GetFuncConfig().contains("messageencryption") ? ENCRYPTED : CLEARTEXT);
        new_msg->type = REGULAR_MESSAGE;
        memcpy((void *)new_msg->data, (void *)&request_count, 8);
        auto l_start = new std::chrono::time_point<std::chrono::high_resolution_clock>(std::chrono::high_resolution_clock::now());
        a_cont->GetMessageManager()->Send(new_msg, message_response, l_start);
    }
    else
    {
        a_cont->GetLogObject()->Log(LRELEASE, "test client func: Test request session done\n");
        a_cont->GetSignalHandler()->Stopfunc();
        auto stop = std::chrono::high_resolution_clock::now();
        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);

        double req_per_sec = (double)request_count / ((double)diff.count() / 1000);

        auto fname = std::to_string(a_cont->GetId().raw) + ".telemetry.log";
        FILE *pFile = fopen(fname.c_str(), "w+");
        fprintf(pFile, "%d, %d, %f, %f\n",
                request_count,
                (int)diff.count(),
                (double)std::accumulate(latency.begin(), latency.end(), 0.0) / (double)latency.size(),
                req_per_sec);
        a_cont->GetLogObject()->Log(LRELEASE, "Messages total = %d, duration =  %d miliseconds,  avg latency(ns) = %llu, throughput(MB per sec) = %f\n",
                                    request_count,
                                    (int)diff.count(),
                                    (std::accumulate(latency.begin(), latency.end(), 0ll) / latency.size()),
                                    ((double)(req_per_sec * packagesize))/((double)(1024 * 1024)));
        fflush(pFile); 
        fclose(pFile);
        __sync_fetch_and_add(&threads_done, 1);

        if (threads_done == a_cont->GetThreadPool()->physicalThreadCount())
        {
            auto msg = a_cont->GetMessageManager()->allocateMessage(a_cont->GetFuncConfig()["connected-to"].value.tostring(), 8, REGULAR, a_cont->GetFuncConfig().contains("messageencryption") ? ENCRYPTED : CLEARTEXT);
            msg->type = TEST_BENCHMARK_MESSAGE_TYPE;
            a_cont->GetLogObject()->Log(
                LRELEASE, "Notifying %s of test done\n",
                a_cont->GetFuncConfig()["connected-to"].value.tostring().c_str());
            a_cont->GetMessageManager()->Send(msg, nullptr, nullptr);
            a_cont->GetSignalHandler()->Stopfunc();
        }
    }
}

void message_response(void *ptr, int status)
{
    auto ctx = (msg_async_response_t *)ptr;
    DIGGI_ASSERT(ctx);
    DIGGI_ASSERT(ctx->msg);
    DIGGI_ASSERT(ctx->context);
    auto l_start = (std::chrono::time_point<std::chrono::high_resolution_clock>*)ctx->context;
    auto l_stop = std::chrono::high_resolution_clock::now();
    auto a_cont = GET_DIGGI_GLOBAL_CONTEXT();
    latency.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(l_stop - *l_start).count());
    
    delete l_start;
    request_count++;
    int expected_req = *(int*)ctx->msg->data;
    if(expected_req + 1 != request_count){
         a_cont->GetLogObject()->Log(LRELEASE, "ERROR, expected request count is wrong\n");
         DIGGI_ASSERT(false);
    }
    ctx->msg->data[ctx->msg->size - sizeof(msg_t) - 1] = '\0';

    a_cont->GetLogObject()->Log(LDEBUG, "Client recieved data from server %" PRIu64 " with id:%lu, size:%lu, message: %d \n",
                                ctx->msg->src.raw,
                                ctx->msg->id,
                                ctx->msg->size - sizeof(msg_t),
                                *(int*)ctx->msg->data);
    /*
		We expect threads to surface back on the same thread, i.e
		1:1 between threads on client:server.
	*/
    DIGGI_ASSERT(ctx->msg->dest.fields.thread == (uint8_t)a_cont->GetThreadPool()->currentThreadId());
    a_cont->GetMessageManager()->endAsync(ctx->msg);
    message_start(a_cont, 1);
}

void execute_thread_unpack(void *ptr, int status)
{
    auto resp = (msg_async_response_t *)ptr;
    start = std::chrono::high_resolution_clock::now();
    message_start(resp->context, 1);
}

void send_from_thread(void *ptr, int status)
{
    auto a_cont = GET_DIGGI_GLOBAL_CONTEXT();
    auto func_map = a_cont->GetMessageManager()->getfuncNames();
    for (std::map<std::string, aid_t>::const_iterator it = func_map.begin();
         it != func_map.end(); it++)
    {

        if ((it->first.find("test_client_latency_func") != std::string::npos) && (it->second.raw != a_cont->GetId().raw))
        {

            auto msg = a_cont->GetMessageManager()->allocateMessage(it->first, 8, REGULAR, a_cont->GetFuncConfig().contains("messageencryption") ? ENCRYPTED : CLEARTEXT);
            msg->type = TEST_BENCHMARK_MESSAGE_TYPE;
            a_cont->GetLogObject()->Log(
                LRELEASE, "Notifying %s of ready loading complete\n",
                it->first.c_str());
            a_cont->GetMessageManager()->Send(msg, nullptr, nullptr);
        }
    }
}

void register_and_wait(void *ptr, int status)
{
    DIGGI_ASSERT(ptr);
    auto a_cont = GET_DIGGI_GLOBAL_CONTEXT();
    a_cont->GetLogObject()->Log(LRELEASE, "Listening for activate event\n");

    a_cont->GetMessageManager()->registerTypeCallback(
        execute_thread_unpack,
        TEST_BENCHMARK_MESSAGE_TYPE,
        ptr);
}

void func_start(void *ctx, int status)
{
    DIGGI_ASSERT(ctx);
    auto a_cont = (DiggiAPI *)ctx;
    auto thread_p = a_cont->GetThreadPool();

    a_cont->GetLogObject()->Log(LRELEASE, "Starting test request client func\n");
    if (thread_p->physicalThreadCount() > 1)
    {
        a_cont->GetLogObject()->Log(LRELEASE, "ERROR: Multithreaded not supported\n");
        DIGGI_ASSERT(false);
    }
    for (size_t i = 0; i < thread_p->physicalThreadCount(); i++)
    {
        if ((a_cont->GetFuncConfig()["master"].value == "1"))
        {
            /*
				Send message to all other 
			*/
            thread_p->ScheduleOn(i, send_from_thread, nullptr, __PRETTY_FUNCTION__);
        }

        if ((a_cont->GetFuncConfig()["wait-for-load"].value == "1"))
        {
            a_cont->GetLogObject()->Log(LRELEASE, "Registering wait on thread=%lu\n", i);
            thread_p->ScheduleOn(i, register_and_wait, a_cont, __PRETTY_FUNCTION__);
        }
        else
        {
            a_cont->GetLogObject()->Log(LRELEASE, "Registering execute on thread=%lu\n", i);
            start = std::chrono::system_clock::now();

            thread_p->ScheduleOn(i, message_start, a_cont, __PRETTY_FUNCTION__);
        }
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