/**
 * @file asyncmessagemanager.h
 * @author Anders Gjerdrum (anders.t.gjerdrum@uit.no)
 * @brief header file for AsyncMessageManager class used for asynchronous inbound message processing and outbound message preparation.
 * @version 0.1
 * @date 2020-01-30
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#ifndef ASYNC_MESSAGE_MANAGER_H
#define ASYNC_MESSAGE_MANAGER_H
#include "threading/IThreadPool.h"
#include <map>
#include "messaging/IMessageManager.h"
#include "datatypes.h"
#include <inttypes.h>
#include "Logging.h"
#include "ZeroCopyString.h"
#include "lockfree_rb_q.h"
#include "messaging/IThreadSafeMM.h"
#include "telemetry.h"
#include "misc.h"
#include "runtime/DiggiAPI.h"
#
/**
 * @brief struct for storing destination queue, for direct instance to instance adressing
 * 
 */
typedef struct name_service_update_t
{
    ///uniqe instance identifier
    aid_t physical_address;
    ///input queue for destination instnance
    lf_buffer_t *destination_queue;
    ///string size of human readable name
    size_t name_size;
    ///string holding human readable name.
    char name[256]; //null terminated, hopefully..
} name_service_update_t;

///defines how many poll events a thread is allowed to remain idle (recieve no messages)
#define DIGGI_IDLE_MESSAGE_THRESHOLD (uint64_t)10000
/// base sleep interval for linear backof algorithm
#define DIGGI_BASE_IDLE_SLEEP_USEC (uint64_t)1
/// peak sleep interval for linear backoff algorithm, determines responsiveness of thread to incomming messages.
#define PEAK_LINEAR_BACKOFF (uint64_t)8192
/**
 * @brief class defintion implementing the IAsyncMessageManger interface.
 * 
 */
class AsyncMessageManager : public IAsyncMessageManager
{
    // declares friend classes allowing unit tests to access private members of class
#ifdef TEST_DEBUG
#include <gtest/gtest_prod.h>
    FRIEND_TEST(asyncmessagemanager, simple_source_sink_test);
    FRIEND_TEST(asyncmessagemanager, simple_continuation_source_sink);
#endif
    /// monotonic increasing identifier for creating async flow ids(message ids)
    unsigned long monotonic_msg_id;
    unsigned long monotonic_virtual_msg_id;
    ///current state of linear backof( how many microseconds the next sleep will be)
    uint64_t linearbackoff;
    /// count for occurences of no message recieved during poll operation
    uint64_t nomessage_event_cnt;
    ///signal bolean to stopp polling in the event of shutdown
    volatile bool stop;
    static bool async_source_cb(msg_async_response_t *resp);
    static void defered_async_source_cb(void *ptr, int status);
    static void async_source_cb_thread_change(void *ptr, int status);
    static void async_message_pump(void *ctx, int status);
    lf_buffer_t *getTargetBuffer(aid_t destination);

    /*Concurrent access is not allowed, all acces by single thread*/
    std::map<uint64_t, async_work_t> async_handler_map;
    std::map<msg_type_t, async_work_t> type_handler_map;
    std::map<uint64_t, lf_buffer_t *> outbound_map;
    /// thread safe message manager implements the IMessageManager interface, which returns the correct SecureMessageManager based on threadid
    IThreadSafeMM *tsafemm;
    /// own instances unique identifier
public:
    IDiggiAPI *diggiapi;
    ///own input queue, polled by AMM
    lf_buffer_t *input;
    /// outbound queue used for sending messages to remote hosts or untrusted runtime.
    lf_buffer_t *output;
    /// thread id relative to global ordering to ensure correct lockfree thread addressing on outbound queues.
    size_t global_thread_id;
    /// global message object buffer storing preallocated message objects for quick consumption by instances.
    lf_buffer_t *global_mem_buf;

    AsyncMessageManager(
        IDiggiAPI *dapi,
        lf_buffer_t *input_q,
        lf_buffer_t *output_q,
        std::vector<name_service_update_t> outbound_queues,
        size_t global_thread_id,
        lf_buffer_t *global_mem_buf);
    AsyncMessageManager(
        IDiggiAPI *dapi,
        lf_buffer_t *input_q,
        lf_buffer_t *output_q,
        std::vector<name_service_update_t> outbound_queues,
        size_t global_thread_id,
        lf_buffer_t *global_mem_buf,
        IThreadSafeMM *tsafemm);
    ~AsyncMessageManager();

    /*Message id is combination of monotonic counter and source id*/
    unsigned long getMessageId(unsigned long func_identifier);
    unsigned long getVirtualMessageId(unsigned long func_identifier);
    void Stop();
    void Start();

    void registerTypeCallback(async_cb_t cb, msg_type_t ty, void *arg);
    void UnregisterTypeCallback(msg_type_t ty);
    /*
	different thread
	*/

    msg_t *allocateMessage(msg_t *msg, size_t payload_size);

    msg_t *allocateMessage(aid_t source, aid_t dest, size_t payload_size, msg_convention_t async);
    /*await response*/
    void sendMessageAsync(msg_t *msg, async_cb_t cb, void *ptr);
    void endAsync(msg_t *msg);
    /*one way*/
    void sendMessage(msg_t *msg);
};

#endif
