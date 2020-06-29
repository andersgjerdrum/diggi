/**
 * @file cryptoStubs.cpp
 * @author Anders Gjerdrum (anders.gjedrum@uit.no)
 * @brief implementation of cryptographic POSIX primitives.
 * @version 0.1
 * @date 2020-02-03
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#include "DiggiAssert.h"
#include "posix/crypto_stubs.h"
#include "posix/io_types.h"
#include "messaging/Pack.h"
#include "DiggiGlobal.h"
#include "datatypes.h"

void cryptostubs_setresponse(void *ptr, int status)
{ // why can't parameter 1 be a msg_async_response_t* ?
    auto rsp = (msg_async_response_t *)ptr;
    DIGGI_ASSERT(ptr);
    DIGGI_ASSERT(status); // status isn't really used, just magic number 1 for now.
    IDiggiAPI *global_context = GET_DIGGI_GLOBAL_CONTEXT();

    global_context->GetLogObject()->Log(LDEBUG, "Setting response from call\n");

    msg_t **mr = (msg_t **)rsp->context;
    *mr = COPY(msg_t, rsp->msg, rsp->msg->size);
    global_context->GetMessageManager()->endAsync(rsp->msg);

    return;
}

msg_t *cryptostubs_wait_for_response(msg_t **ptr)
{

    while (*ptr == nullptr)
    {
        IDiggiAPI *global_context = GET_DIGGI_GLOBAL_CONTEXT();
        auto tp = global_context->GetThreadPool();
        DIGGI_ASSERT(tp != nullptr);
        tp->Yield();
    }
    return *ptr;
}
void cryptostubs_freeresponse(msg_t **ptr)
{
    DIGGI_ASSERT(*ptr != nullptr);
    free(*ptr);
    *ptr = nullptr;
}

/**
 * @brief pseudo random number genrerator.
 * Request for random number.
 * not implemented, will throw assertion failure.
 * 
 * @return int 
 */
int i_rand(void)
{
    msg_t *retmsg = nullptr;
    msg_t **put = &retmsg;

    IDiggiAPI *global_context = GET_DIGGI_GLOBAL_CONTEXT();
    global_context->GetLogObject()->Log(LogLevel::LDEBUG, "Intercepted rand syscall\n");
    if (global_context->GetNetworkManager())
    {
        global_context->GetNetworkManager()->AsyncRand(cryptostubs_setresponse, put);
        cryptostubs_wait_for_response(put);

        // fetch stuff from put and place in pointers given from caller
        auto *ptr = retmsg->data;
        int retval = Pack::unpack<int>(&ptr);
        cryptostubs_freeresponse(put);
        return retval;
    }
    else
    {
        auto mngr = global_context->GetMessageManager();
        auto msg = mngr->allocateMessage("file_io_func", 0, CALLBACK, ENCRYPTED);
        msg->type = NET_RAND_MSG_TYPE;

        mngr->Send(msg, cryptostubs_setresponse, put);
        cryptostubs_wait_for_response(put);

        auto *ptr = retmsg->data;
        int retval = Pack::unpack<int>(&ptr);
        cryptostubs_freeresponse(put);

        return retval;
    }
}
