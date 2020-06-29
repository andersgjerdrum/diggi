#ifndef THREADSAFEMM_H
#define THREADSAFEMM_H
/**
 * @file ThreadSafeMessageManager.h
 * @author Anders Gjerdrum (anders.t.gjerdrum@uit.no)
 * @brief Header file implementing thread safe message manager
 * @version 0.1
 * @date 2020-01-31
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#include "datatypes.h"
#include "DiggiAssert.h"
#include "misc.h"
#include "messaging/IMessageManager.h"
#include <map>
#include "Logging.h"
#include "threading/IThreadPool.h"
#include "messaging/AsyncMessageManager.h"
#include "messaging/IIASAPI.h"
#include "sgx/DynamicEnclaveMeasurement.h"
#include "runtime/DiggiReplayManager.h"
#include "storage/TamperProofLog.h"

/**
 * @brief class definition for threadsafe messagemanager implementing the imessagemanager interface.
 * Provide convenient threadsafe wrapper to SecureMessageManager(SMM).
 * Threadsafemessagemanager initializes one SMM,AMM pair for each physical thread, and routes all api requests to the correct per-thread instance.
 * Each thread is indentified using thread-local-storage, set during the init procedure of runtime.
 */
class ThreadSafeMessageManager : public IMessageManager, public IThreadSafeMM
{
    ///friend classes to allow unit test access to private members
#ifdef TEST_DEBUG
#include <gtest/gtest_prod.h>
    FRIEND_TEST(threadsafe_messagemanager, secure_test_many_send_recieve);
    FRIEND_TEST(threadsafe_messagemanager, test_many_send_recieve);
#endif
    ///Array holding all SMMs indexed on thread id
    std::vector<IMessageManager *> perthreadMngr;
    ///Array holding all AMMs indexec on thread id
    std::vector<AsyncMessageManager *> perthreadAMngr;
    /// total thread count for instance, determines how many SMM,AMM pairs we need
    size_t threadCount;
    ///threadpool api reference.
    IThreadPool *threadpool;
    static void StartPolling(void *ptr, int status);
    static void StartReplay(void *ptr, int status);

public:
    /**
     * @brief static factory for creating a ThreadSafeMessageManager.
     * @warning Not threadsafe, must be invoked on intitialization thread (external to threadpool).
     * @tparam T SecureMessageManager, templated to enable replacements for future and testing.
     * @tparam Y AsynchronousMessageManager, templated to enable replacements for future and testing
     * @param input_q Input queue to diggi instiance
     * @param output_q Output queue used for remote messages and messages to untrusted runtime
     * @param iasapi attestation api implementation reference
     * @param threadp diggi api threadpool reference
     * @param nameservice_updates map of Human Readable Name to unique instance identifier 
     * @param outbound_queues list of queues for outbound direct communication with cohosted instances.
     * @param self_id own unique instance identifier
     * @param base_thread base host-relative thread, used for thread addressing when accessing global message object pool
     * @param global_mem_buf Global message object pool, used to allocate outbound messages, and relinquish incomming messages, shared among all co-hosted instances.
     * @param log diggi api logging object
     * @param trusted_root_func_role boolean specifying if current instance is a trusted root. Hardcoded into binary as part of configuration.
     * @param mrmnt reference to dynamic enclave measurement object, which updates enclave measurement based on message state incomming to enclave.
     * @param crypto implementation api of message crypto.
     * @return ThreadSafeMessageManager* new instance of threadsafemessagemanager, consumable for diggi instances.
     */
    template <class T, class Y>
    static ThreadSafeMessageManager *Create(IDiggiAPI *dapi,
                                            lf_buffer_t *input_q,
                                            lf_buffer_t *output_q,
                                            IIASAPI *iasapi,
                                            std::map<std::string, aid_t> nameservice_updates,
                                            std::vector<name_service_update_t> outbound_queues,
                                            size_t base_thread,
                                            lf_buffer_t *global_mem_buf,
                                            bool trusted_root_func_role,
                                            IDynamicEnclaveMeasurement *mrmnt,
                                            bool record_func,
                                            ICryptoImplementation *crypto = nullptr)
    {
        auto rettsmm = new ThreadSafeMessageManager(dapi->GetThreadPool());
        rettsmm->threadCount = dapi->GetThreadPool()->physicalThreadCount();
        DIGGI_ASSERT(rettsmm->threadCount);

        for (unsigned i = 0; i < rettsmm->threadCount; i++)
        {
            DIGGI_TRACE(dapi->GetLogObject(), LDEBUG, "Creating Async message object for thread %u\n", i);

            auto ymngr = new Y(
                dapi,
                input_q,
                output_q,
                outbound_queues,
                base_thread + i,
                global_mem_buf,
                rettsmm);
            rettsmm->perthreadAMngr.push_back(ymngr);
            DIGGI_TRACE(dapi->GetLogObject(), LDEBUG, "Creating secure message object for thread %u\n", i);

            rettsmm->perthreadMngr.push_back(new T(
                dapi,
                iasapi,
                ymngr,
                nameservice_updates,
                i,
                mrmnt,
                crypto,
                record_func,
                trusted_root_func_role));
            __sync_synchronize();

            dapi->GetThreadPool()->ScheduleOn(i, ThreadSafeMessageManager::StartPolling, ymngr, __PRETTY_FUNCTION__);
        }
        return rettsmm;
    }

    typedef struct AsyncContext<DiggiReplayManager *, async_cb_t, void*> repl_ctx_t;
    static ThreadSafeMessageManager *CreateReplay(
        IDiggiAPI *dapi,
        std::map<std::string, aid_t> nameservice_updates,
        aid_t self_id,
        async_cb_t cb,
        void *ptr)
    {
        auto rettsmm = new ThreadSafeMessageManager(dapi->GetThreadPool());
        rettsmm->threadCount = dapi->GetThreadPool()->physicalThreadCount();
        for (unsigned i = 0; i < rettsmm->threadCount; i++)
        {
            std::string inp = std::to_string(i) + ".replay.input";
            std::string outp = std::to_string(i) + ".replay.output";
            auto repl_mgngr = new DiggiReplayManager(
                dapi->GetThreadPool(),
                nameservice_updates,
                self_id,
                new TamperProofLog(dapi),
                inp,
                new TamperProofLog(dapi),
                outp,
                dapi->GetLogObject(),
                i);
            rettsmm->perthreadMngr.push_back(repl_mgngr);
            auto ctx = new repl_ctx_t(repl_mgngr, cb, ptr);
            dapi->GetThreadPool()->ScheduleOn(i, ThreadSafeMessageManager::StartReplay, ctx, __PRETTY_FUNCTION__);
        }

        return rettsmm;
    }

    /**
     * @brief Construct a new Thread Safe Message Manager object
     * only sets threadpool property, configured by factory.
     * @warning should not be invoked outside of factory.
     * @param threadpool 
     */
    ThreadSafeMessageManager(IThreadPool *threadpool) : threadpool(threadpool)
    {
    }
    /**
     * @brief Destroy the Thread Safe Message Manager object.
     * May safefly be infoked by client.
     * @warning Stop threadpool prior to invoking this!
     * destroys all SMM and AMM.
     * First stop polling for each AMM
     */
    ~ThreadSafeMessageManager()
    {
        for (unsigned i = 0; i < threadCount; i++)
        {
            auto ptmngr = perthreadMngr[i];
            auto ymngr = perthreadAMngr[i];
            DIGGI_ASSERT(ptmngr);
            DIGGI_ASSERT(ymngr);
            ymngr->Stop();
            delete ptmngr;
            delete ymngr;
        }
    }
    AsyncMessageManager *getAsyncMessageManager();
    IAsyncMessageManager *getIAsyncMessageManager();
    void registerTypeCallback(async_cb_t cb, msg_type_t type, void *ctx);
    void endAsync(msg_t *msg);
    void Send(msg_t *msg, async_cb_t cb, void *cb_context);
    msg_t *allocateMessage(
        std::string destination,
        size_t payload_size,
        msg_convention_t async,
        msg_delivery_t delivery);
    msg_t *allocateMessage(
        aid_t destination,
        size_t payload_size,
        msg_convention_t async,
        msg_delivery_t delivery);
    msg_t *allocateMessage(msg_t *msg, size_t payload_size);
    std::map<std::string, aid_t> getfuncNames();
};

#endif