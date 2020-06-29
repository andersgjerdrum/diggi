/**
 * @file ThreadSafeMessageManager.cpp
 * @author Anders Gjerdrum (anders.t.gjerdrum@uit.no)
 * @brief Implementation of thread safe wrapper for message manager interface.
 * @version 0.1
 * @date 2020-01-31
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#include "messaging/ThreadSafeMessageManager.h"

/// callback invoked on correct thread to enable AMM polling loop (message pump)
void ThreadSafeMessageManager::StartPolling(void *ptr, int status)
{
    DIGGI_ASSERT(ptr);
    auto amm = (AsyncMessageManager *)ptr;
    amm->Start();
}

void ThreadSafeMessageManager::StartReplay(void *ptr, int status)
{
    DIGGI_ASSERT(ptr);
    auto ctx = (repl_ctx_t *)ptr;
    auto rplmm = ctx->item1;
    rplmm->Start(ctx->item2, ctx->item3);
}
/// per-thread, threadsafe version @see SecureMessageManager::registerTypeCallback
void ThreadSafeMessageManager::registerTypeCallback(async_cb_t cb, msg_type_t type, void *ctx)
{
    auto thrid = threadpool->currentThreadId();
    DIGGI_ASSERT(thrid >= 0);
    DIGGI_ASSERT(perthreadMngr.size() > (size_t)thrid);
    auto ptmngr = perthreadMngr[thrid];
    DIGGI_ASSERT(ptmngr);
    ptmngr->registerTypeCallback(cb, type, ctx);
}
/// per-thread, threadsafe version @see SecureMessageManager::endAsync
void ThreadSafeMessageManager::endAsync(msg_t *msg)
{
    auto thrid = threadpool->currentThreadId();
    DIGGI_ASSERT(thrid >= 0);
    DIGGI_ASSERT(perthreadMngr.size() > (size_t)thrid);
    auto ptmngr = perthreadMngr[thrid];
    ptmngr->endAsync(msg);
}

///retrive thread-specific asyncmessagemanager @warning do not hand-off to another thread.
AsyncMessageManager *ThreadSafeMessageManager::getAsyncMessageManager()
{

    auto thrid = threadpool->currentThreadId();
    DIGGI_ASSERT(thrid >= 0);
    DIGGI_ASSERT(perthreadMngr.size() > (size_t)thrid);
    return perthreadAMngr[thrid];
}

///retrive thread-specific IAsyncMessageManager @warning do not hand-off to another thread.
IAsyncMessageManager *ThreadSafeMessageManager::getIAsyncMessageManager()
{

    auto thrid = threadpool->currentThreadId();
    DIGGI_ASSERT(thrid >= 0);
    DIGGI_ASSERT(perthreadMngr.size() > (size_t)thrid);
    return perthreadAMngr[thrid];
}

/// per-thread, threadsafe version @see SecureMessageManager::Send
void ThreadSafeMessageManager::Send(msg_t *msg, async_cb_t cb, void *cb_context)
{
    auto thrid = threadpool->currentThreadId();
    DIGGI_ASSERT(thrid >= 0);
    DIGGI_ASSERT(perthreadMngr.size() > (size_t)thrid);
    auto ptmngr = perthreadMngr[thrid];
    DIGGI_ASSERT(ptmngr);
    ptmngr->Send(msg, cb, cb_context);
}

/// per-thread, threadsafe version @see SecureMessageManager::allocateMessage
msg_t *ThreadSafeMessageManager::allocateMessage(
    std::string destination,
    size_t payload_size,
    msg_convention_t async,
    msg_delivery_t delivery)
{
    auto thrid = threadpool->currentThreadId();
    DIGGI_ASSERT(thrid >= 0);
    DIGGI_ASSERT(perthreadMngr.size() > (size_t)thrid);
    auto ptmngr = perthreadMngr[thrid];
    DIGGI_ASSERT(ptmngr);
    return ptmngr->allocateMessage(destination, payload_size, async, delivery);
}

/// per-thread, threadsafe version @see SecureMessageManager::allocateMessage
msg_t *ThreadSafeMessageManager::allocateMessage(
    aid_t destination,
    size_t payload_size,
    msg_convention_t async,
    msg_delivery_t delivery)
{
    auto thrid = threadpool->currentThreadId();
    DIGGI_ASSERT(thrid >= 0);
    DIGGI_ASSERT(perthreadMngr.size() > (size_t)thrid);
    auto ptmngr = perthreadMngr[thrid];
    DIGGI_ASSERT(ptmngr);
    return ptmngr->allocateMessage(destination, payload_size, async, delivery);
}

/// per-thread, threadsafe version @see SecureMessageManager::allocateMessage
msg_t *ThreadSafeMessageManager::allocateMessage(msg_t *msg, size_t payload_size)
{
    auto thrid = threadpool->currentThreadId();
    DIGGI_ASSERT(thrid >= 0);
    DIGGI_ASSERT(perthreadMngr.size() > (size_t)thrid);
    auto ptmngr = perthreadMngr[thrid];
    DIGGI_ASSERT(ptmngr);
    return ptmngr->allocateMessage(msg, payload_size);
}

/// per-thread, threadsafe version @see SecureMessageManager::getfuncNames
std::map<std::string, aid_t> ThreadSafeMessageManager::getfuncNames()
{
    auto thrid = threadpool->currentThreadId();
    DIGGI_ASSERT(thrid >= 0);
    DIGGI_ASSERT(perthreadMngr.size() > (size_t)thrid);
    auto ptmngr = perthreadMngr[thrid];
    DIGGI_ASSERT(ptmngr);
    return ptmngr->getfuncNames();
}
