/**
 * @file asyncmessagemanager.cpp
 * @author Anders Gjerdrum (anders.t.gjerdrum@uit.no)
 * @brief message scheduling interfaces inside trusted runtime/diggi instance
 * handles message allocation, thread bound messaging, asynchronous callback flow engine, and message polling.
 * @version 0.1
 * @date 2020-01-30
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#include "messaging/AsyncMessageManager.h"

/**
 * @brief Construct a new Async Message Manager:: Async Message Manager object
 * Unique object allocated per physical thread.
 * setup initial values for message indentity
 * Setup linear backof algoritmic initial state.
 * Parses outbound queues into map for direct addressing target instances, situated on same host.
 * Avoid resource consumption in untrusted runtime.
 * @param input_q input queue for all inbound messages, polled by AMM periodically.
 * @param output_q Outbound queue for untrusted runtime /  remote diggi host instances
 * @param outbound_queues Outbound queues for local diggi instances, direct delivery onto input queue.
 * @param global_thread_id global thread id for identifying concurrent producer onto lock-free queues @see lockfree_rb_q.cpp
 * @param global_mem_buf global preprovisioned buffer for allocating outbound message objects onto. requires encryption prior to use.
 * @param tsafemm thread safe message manager reference, for rescheduling thread specic adressable messages onto correct AMM (one per thread)
 */
AsyncMessageManager::AsyncMessageManager(
    IDiggiAPI *dapi,
    lf_buffer_t *input_q,
    lf_buffer_t *output_q,
    std::vector<name_service_update_t> outbound_queues,
    size_t global_thread_id,
    lf_buffer_t *global_mem_buf,
    IThreadSafeMM *tsafemm) : monotonic_msg_id(1),
                              linearbackoff(1),
                              nomessage_event_cnt(0),
                              stop(false),
                              tsafemm(tsafemm),
                              diggiapi(dapi),
                              input(input_q),
                              output(output_q),
                              global_thread_id(global_thread_id),
                              global_mem_buf(global_mem_buf)

{
    monotonic_virtual_msg_id = UINT_MAX;
    DIGGI_ASSERT(global_mem_buf != nullptr);
    for (auto item : outbound_queues)
    {
        item.physical_address.fields.thread = 0;
        DIGGI_TRACE(diggiapi->GetLogObject(), LDEBUG, "outbound.queue.entry: address = %lu, ptr = %p\n", item.physical_address.raw, item.destination_queue);
        outbound_map[item.physical_address.raw] = item.destination_queue;
    }
}
/**
 * @brief Construct a new Async Message Manager:: Async Message Manager object
 * Duplicate, for use in unit testing.
 * @param input_q 
 * @param output_q 
 * @param outbound_queues 
 * @param global_thread_id 
 * @param global_mem_buf 
 */
AsyncMessageManager::AsyncMessageManager(
    IDiggiAPI *dapi,
    lf_buffer_t *input_q,
    lf_buffer_t *output_q,
    std::vector<name_service_update_t> outbound_queues,
    size_t global_thread_id,
    lf_buffer_t *global_mem_buf) : monotonic_msg_id(1),
                                   linearbackoff(1),
                                   nomessage_event_cnt(0),
                                   stop(false),
                                   tsafemm(nullptr),
                                   diggiapi(dapi),
                                   input(input_q),
                                   output(output_q),
                                   global_thread_id(global_thread_id),
                                   global_mem_buf(global_mem_buf)
{
    monotonic_virtual_msg_id = UINT_MAX;
    DIGGI_ASSERT(global_mem_buf != nullptr);

    for (auto item : outbound_queues)
    {
        item.physical_address.fields.thread = 0;
        DIGGI_TRACE(diggiapi->GetLogObject(), LDEBUG, "outbound.queue.entry: address%lu, ptr= %p\n", item.physical_address.raw, item.destination_queue);
        outbound_map[item.physical_address.raw] = item.destination_queue;
    }
}
/**
 * @brief start message polling
 * Invokes thread scheduler to start message pump callback.
 */
void AsyncMessageManager::Start()
{
    DIGGI_TRACE(diggiapi->GetLogObject(), LDEBUG, "async message manager: starting message pump\n");

    diggiapi->GetThreadPool()->Schedule(AsyncMessageManager::async_message_pump, this, __PRETTY_FUNCTION__);
}
/**
 * @brief Destroy the Async Message Manager:: Async Message Manager object
 * clears all internally allocated state for maps.
 * Must only be invoked after Stop()
 */
AsyncMessageManager::~AsyncMessageManager()
{

    type_handler_map.clear();
    async_handler_map.clear();
    outbound_map.clear();
}
/**
 * @brief stop message polling
 * sets stop boolean regularly checked by message pump to shut it down.
 * use memory barrier to ensure other threads see write operation immediately.
 */
void AsyncMessageManager::Stop()
{
    DIGGI_TRACE(diggiapi->GetLogObject(), LDEBUG, "async message manager: stopping message pump\n");
    stop = true;
    __sync_synchronize();
}

/**
 * @brief generates unique message id, used for creating asynchronous callback flows.
 * uses lowermost bits of uniqe diggi identifier(aid) and monotonic increasing counter.
 * Reduces collition probability across diggi network.
 * @param func_identifier 
 * @return unsigned long new message id
 */
unsigned long AsyncMessageManager::getMessageId(unsigned long func_identifier)
{
    monotonic_msg_id++;

    auto retval = (func_identifier << 32) | (monotonic_msg_id & 0x00000000ffffffff);
    DIGGI_TRACE(diggiapi->GetLogObject(), LDEBUG, "new id=%lu\n", retval);

    return retval;
}

unsigned long AsyncMessageManager::getVirtualMessageId(unsigned long func_identifier)
{
    monotonic_virtual_msg_id--;
    auto retval = (func_identifier << 32) | (monotonic_virtual_msg_id & 0x00000000ffffffff);
    DIGGI_TRACE(diggiapi->GetLogObject(), LDEBUG, "new virtual id=%lu\n", retval);

    return retval;
}
/**
 * @brief finds target inbound queue based on instance identifier.
 * used for direct instance to instance message delivery, without involving the untrusted runtime scheduler.
 * retrieves untrusted runtime target buffer for remote identifiers
 * @param destination unique instance identifier @see aid_t
 * @return lf_buffer_t* target inbound buffer.
 */
lf_buffer_t *AsyncMessageManager::getTargetBuffer(aid_t destination)
{
    destination.fields.thread = 0;

    DIGGI_ASSERT(destination.raw != diggiapi->GetId().raw);
    auto outbf = outbound_map[destination.raw];

    if (outbf)
    {
        DIGGI_ASSERT(outbf != output);
        return outbf;
    }
    return output;
}
/**
 * @brief register a type-based callback.
 * Registers a callback used for all messages recieved of a particular msg_type_t @see msg_type_t
 * all messages of that particular type, who are not assoicated with a message flow ( has a message id)
 * are routed to this particular callback.
 * Only valid for the calling thread. for multithreaded diggi instances, each must individually register an identical typed callback.
 * Reciept of typed messages not pre registered for reciept, cause an assertion failure.
 * may be used  as initial step of multi-step flows.
 * @param cb callback whic is invoked upon reciept of a message 
 * @param ty type associated with the callback
 * @param arg convenience pointer to voulentary associated context object managed by calle.
 */
void AsyncMessageManager::registerTypeCallback(async_cb_t cb, msg_type_t ty, void *arg)
{
    DIGGI_ASSERT(cb);
    type_handler_map[ty].cb = cb;
    type_handler_map[ty].status = 1;
    type_handler_map[ty].arg = arg;
}
/**
 * @brief unregister a typed callback. 
 * will, after invocation, delete typed callabck from type_handler_map.
 * will cause assertions if an inbound message containing said type is recieved after this call is performed.
 * 
 * @param ty message type
 */
void AsyncMessageManager::UnregisterTypeCallback(msg_type_t ty)
{
    type_handler_map.erase(ty);
}
/**
 * @brief allocate a response message object buffer for populating with data.
 * allocated onto global memory buffer managed in untruster memory. 
 * this method allocates a message as a response for previously recieved message input as parameter
 * message identity is copied to the new message, 
 * which allow it to be used in response for which the recipient has registered a one-off callback awaiting the recipt of a response.
 * Messages may flow indefinently back and forth using the message id to identify unique onetime callbacks.
 * @warning integrity and confidentiality should be handled prior to populating buffer as contents are visible to host.
 * @param msg message recieved for which this message is a response to.
 * @param payload_size size of new message payload.
 * @return msg_t* new message with same id and new payload.
 */
msg_t *AsyncMessageManager::allocateMessage(msg_t *msg, size_t payload_size)
{

    DIGGI_ASSERT(output);
    DIGGI_ASSERT(msg->src.raw != 0);
    DIGGI_ASSERT(msg->dest.raw != 0);
    DIGGI_ASSERT((payload_size + sizeof(msg_t)) < MAX_DIGGI_MEM_SIZE);

    msg_t *msg_n = (msg_t *)lf_recieve(global_mem_buf, global_thread_id);

    msg_n->size = sizeof(msg_t) + payload_size;
    msg_n->src = msg->src;
    msg_n->omit_from_log = msg->omit_from_log;
    msg_n->type = msg->type;
    msg_n->dest = msg->dest;
    msg_n->delivery = msg->delivery;
    msg_n->session_count = msg->session_count + 1; /*attempt to increase session count*/
    msg_n->id = msg->id;
    return msg_n;
}
/**
 * @brief allocate an initial message object to a give target, defined by instance identifier.
 * subsequently populated with data prior to send.
 * allocated onto global memory buffer managed in untruster memory. 
 * @warning integrity and confidentiality should be handled prior to populating buffer as contents are visible to host.
 * @param source self, should probably never be anything else.
 * @param dest destination instance, local or remote.
 * @param payload_size size of payload of data.
 * @param async msg_convention_t specifiying if a flow should be generated, for which future responses to this message may be directed. @see msg_convention_t 
 * @return msg_t* newly allocated message object
 */
msg_t *AsyncMessageManager::allocateMessage(aid_t source, aid_t dest, size_t payload_size, msg_convention_t async)
{

    DIGGI_ASSERT(output);
    DIGGI_ASSERT(source.raw != 0);
    DIGGI_ASSERT(dest.raw != 0);
    DIGGI_ASSERT((payload_size + sizeof(msg_t)) < MAX_DIGGI_MEM_SIZE);

    msg_t *msg = (msg_t *)lf_recieve(global_mem_buf, global_thread_id);
    msg->omit_from_log = 0;
    msg->size = sizeof(msg_t) + payload_size;
    msg->src = source;
    msg->dest = dest;
    msg->session_count = 0;
    msg->id = 0;
    return msg;
}

/**
 * @brief Send previously allocated message and await response through provided callback.
 * one-of message, which directs message id to particular callback.
 * provided ptr used for convenienece, add calle managed context to callback.
 * upon response reciept, callback recieves msg_async_response_t object with (msg, context) tuple
 * Thead identity is used to deliver to thread with same id on recieving instance, specified in aid_t
 * Multi step flows all use same thread id for each side of the communication
 * guaranteeing that response callbacks allways surface on the same thread.
 * Atempts to override thread addressing may lead to undefined behaviour.
 * Delivery to other thread may be safely done by diggiapi->GetThreadPool()->ScheduleOn(thread_id, callback,,,)
 * @warning Callback must persist response message explicitly because the message buffer is reclaimed after callback completes
 * @param msg message to send
 * @param cb callback for expected response
 * @param ptr calle managed convenience context pointer
 */
void AsyncMessageManager::sendMessageAsync(msg_t *msg, async_cb_t cb, void *ptr)
{
    DIGGI_ASSERT(cb);

    DIGGI_TRACE(diggiapi->GetLogObject(),
                LogLevel::LDEBUG,
                "Sending message expecting response from: %" PRIu64 ", to: %" PRIu64 ", id: %lu size: %lu, type = %d \n",
                msg->src.raw,
                msg->dest.raw,
                msg->id,
                msg->size,
                msg->type);
    msg->src.fields.thread = (uint8_t)diggiapi->GetThreadPool()->currentThreadId();
    if (msg->id == 0)
    {
        msg->id = (msg->omit_from_log) ? getVirtualMessageId(diggiapi->GetId().raw) : getMessageId(diggiapi->GetId().raw);
    }

    async_handler_map[msg->id].cb = cb;
    async_handler_map[msg->id].arg = ptr;

    lf_send(getTargetBuffer(msg->dest), msg, global_thread_id);
}
/**
 * @brief deletes stored multistep flow state.
 * Once message flows are completed each party may purge internal flow state.
 * recipt of new messages with this identifier, after purge, will cause an assertion failure.
 * @param msg last recieved message from flow
 */
void AsyncMessageManager::endAsync(msg_t *msg)
{
    DIGGI_ASSERT(msg->id > 0);
    DIGGI_TRACE(diggiapi->GetLogObject(),
                LogLevel::LDEBUG,
                "Ending session between: %" PRIu64 ", and: %" PRIu64 ", id: %lu size: %lu \n",
                msg->src.raw,
                msg->dest.raw,
                msg->id,
                msg->size);
    /*
        Zero structure to highlight reuse after erase, as map is not freed.
    */
    async_handler_map[msg->id] = {0};
    async_handler_map.erase(msg->id);
}

/**
 * @brief send message either initially or as a response without expecting callback.
 * For responses to request messages, flow internal state is purged automatically, 
 * since no further interaction will occur as part of flow
 * 
 * @param msg message to be sent
 */
void AsyncMessageManager::sendMessage(msg_t *msg)
{
    DIGGI_TRACE(diggiapi->GetLogObject(),
                LogLevel::LDEBUG,
                "Sending message from: %" PRIu64 ", to: %" PRIu64 ", id: %lu size: %lu , type = %d \n",
                msg->src.raw,
                msg->dest.raw,
                msg->id,
                msg->size,
                msg->type);

    if (msg->id != 0)
    {
        endAsync(msg);
    }

    msg->src.fields.thread = (uint8_t)diggiapi->GetThreadPool()->currentThreadId();

    /*
		We expect to find a header encapsulating the message object 
		used for ringbuffer allocation and transmission.
		We therefore inspect the parrent header to find it.	
	*/
    lf_send(getTargetBuffer(msg->dest), msg, global_thread_id);
}
/**
 * @brief polling loop callback for inbound messages.
 * checks inbound message queue for messages and reschedules self onto diggiapi->GetThreadPool() for asynchronous looping.
 * Implements thread reclamation algorithm, with linear backoff. 
 * AMM registers if current instance thread recieves no incomming messages during a given poll.
 * After a given threshold, the thread is reclaimed by the untrusted runtime, which allow other threads to execute in the interim.
 * The thread is reclaimed for linearly increasing intervals. 
 * Once a packet is retrieved, the algorithm resets and gives exclusive threading controll to the instance.
 * Each thread holds its own AMM and may poll the input queue concurrently.
 * Messages recieved for another thread are delivered to the correct thread AMM by invoking a thread switch to the target via the theadpool api. 
 * @param ctx 
 * @param status 
 */
void AsyncMessageManager::async_message_pump(void *ctx, int status)
{

    DIGGI_ASSERT(ctx);
    auto _this = (AsyncMessageManager *)ctx;
    if (_this->stop)
    {
        return;
    }
    _this->diggiapi->GetThreadPool()->Schedule(AsyncMessageManager::async_message_pump, ctx, __PRETTY_FUNCTION__);
    DIGGI_ASSERT(_this->input);
    msg_async_response_t resp_ctx;
    resp_ctx.context = ctx;
    if (_this->stop)
    {
        /*
			Destructor called, must stop message pump
			Will segfault if destructor is done before log object below is called. 
			Not observed in practice though

			We will risk it for debuggability.
		*/
        DIGGI_TRACE(_this->diggiapi->GetLogObject(), LDEBUG, "async_message_pump is stopping\n");

        return;
    }
    auto item = (msg_t *)lf_try_recieve(_this->input, _this->global_thread_id);

    if (item == nullptr)
    {
        if (_this->nomessage_event_cnt >= DIGGI_IDLE_MESSAGE_THRESHOLD)
        {
            _this->nomessage_event_cnt = 0;
            if (_this->linearbackoff < PEAK_LINEAR_BACKOFF)
            {
                _this->linearbackoff = _this->linearbackoff << 2;
                DIGGI_TRACE(_this->diggiapi->GetLogObject(), LDEBUG, "waiting for message, sleep increased, linearbackoff = %lu\n", _this->linearbackoff);
            }

#ifdef DIGGI_ENCLAVE
            ocall_sleep(_this->linearbackoff * DIGGI_BASE_IDLE_SLEEP_USEC);
#else
            usleep(_this->linearbackoff * DIGGI_BASE_IDLE_SLEEP_USEC);
#endif
        }
        else
        {
            _this->nomessage_event_cnt++;
        }
    }
    else
    {
        _this->nomessage_event_cnt = 0;
        _this->linearbackoff = 1;

        DIGGI_ASSERT(item->size > 0);
        DIGGI_TRACE(_this->diggiapi->GetLogObject(), LDEBUG, "Message recieved, linear backoff reset to 1\n");
        auto msg = (msg_t *)item;
        resp_ctx.msg = msg;
        DIGGI_TRACE(_this->diggiapi->GetLogObject(), LDEBUG,
                    "async_message_pump recieved message from: %lu, to: %lu, id:%lu, size: %lu, type = %d\n",
                    msg->src.raw,
                    msg->dest.raw,
                    msg->id,
                    msg->size,
                    msg->type);

        /*
			If source thread is same as destination thread 
			No resheduling  is required
		*/
        if ((size_t)msg->dest.fields.thread == (size_t)_this->diggiapi->GetThreadPool()->currentThreadId())
        {
            auto defer_ringbuffer_delete = async_source_cb(COPY(msg_async_response_t, &resp_ctx, sizeof(msg_async_response_t)));
            if (!defer_ringbuffer_delete)
            {
                // memset(item, 0, item->size);
                lf_send(_this->global_mem_buf, item, _this->global_thread_id);
            }
        }
        /*
			Resheduling to expected thread is required
		*/
        else
        {
            /*
				The expected recieving thread must exist.
				TODO: Implement fair and consistent scheduling of messages to threads.
					  Given a message mapped to a thread, all messages from that remote thread should be mapped to this particular thread.
					  Messages should be load balanced across threads such that hot message flows do not excessively ocupy a single core.
			*/
            if (!((size_t)(msg->dest.fields.thread) < _this->diggiapi->GetThreadPool()->physicalThreadCount()))
            {
                DIGGI_TRACE(_this->diggiapi->GetLogObject(), LRELEASE,
                            "Physical thread change,src-thread:%d dest-thread %u for message with session count=%lu from: %lu, to: %lu, id:%lu, size: %lu, type = %d\n",
                            _this->diggiapi->GetThreadPool()->currentThreadId(),
                            msg->dest.fields.thread,
                            msg->session_count,
                            msg->src.raw,
                            msg->dest.raw,
                            msg->id,
                            msg->size,
                            msg->type);
                DIGGI_TRACE(_this->diggiapi->GetLogObject(), LRELEASE, "Physical threadcount %lu\n", _this->diggiapi->GetThreadPool()->physicalThreadCount());
            }
            DIGGI_ASSERT(_this->tsafemm);
            DIGGI_ASSERT((size_t)(msg->dest.fields.thread) < _this->diggiapi->GetThreadPool()->physicalThreadCount());
            _this->diggiapi->GetThreadPool()->ScheduleOn(msg->dest.fields.thread,
                                                         AsyncMessageManager::async_source_cb_thread_change,
                                                         COPY(msg_async_response_t, &resp_ctx, sizeof(msg_async_response_t)), __PRETTY_FUNCTION__);
        }
    }
}
/**
 * @brief callback invoked on correct AMM thread in response to a message recieved by another thread/AMM
 * Calls regular message handling procedure after modifying context objects to match current AMM.
 * @param ptr copy of the msg_async_response_t tuple holding the response context and message.
 * @param status 
 */
void AsyncMessageManager::async_source_cb_thread_change(void *ptr, int status)
{
    DIGGI_ASSERT(ptr);
    auto resp = (msg_async_response_t *)ptr;
    auto _this_old_thread = (AsyncMessageManager *)resp->context;
    DIGGI_ASSERT(_this_old_thread);
    DIGGI_ASSERT(((size_t)resp->msg->src.fields.thread) == (size_t)_this_old_thread->diggiapi->GetThreadPool()->currentThreadId());
    DIGGI_ASSERT(((size_t)resp->msg->dest.fields.thread) == (size_t)_this_old_thread->diggiapi->GetThreadPool()->currentThreadId());

    auto msg = resp->msg;
    DIGGI_ASSERT(msg);
    /*
		Set context to correct per thread async message manager object, 
		given by threadsafemanager, for handling callbacks.
	*/
    DIGGI_ASSERT(_this_old_thread->tsafemm);
    resp->context = _this_old_thread->tsafemm->getAsyncMessageManager();
    DIGGI_TRACE(_this_old_thread->diggiapi->GetLogObject(), LDEBUG,
                "Handling message w session_count=%lu on correct thread from: %lu, to: %lu, id:%lu, size: %lu, type = %d\n",
                resp->msg->session_count,
                resp->msg->src.raw,
                resp->msg->dest.raw,
                resp->msg->id,
                resp->msg->size,
                resp->msg->type);

    auto defer_ringbuffer_delete = async_source_cb((msg_async_response_t *)ptr);

    /*
		The new thread accepts responsibillity for old threads ringbuffer message 
		and propperly disposes of it once it is consumed.
	*/
    if (!defer_ringbuffer_delete)
    {
        // memset(msg, 0, msg->size);
        lf_send(_this_old_thread->global_mem_buf, msg, _this_old_thread->tsafemm->getAsyncMessageManager()->global_thread_id);
    }
}

/**
 * @brief maps messages to callbacks, either typed or one-of via flows.
 * Messages are first checked for identifier and if so delegated to one-of callbacks.
 * Messages may have originated as typed, and still hold the typed field, but if they possess an identifer, 
 * we expect them to have a one-off entry in the async_handler_map.
 * One-off handlers witch are not found, cause an assertion, because messages with one-of ids must have a preceding invocation of a typed handler.
 * Typed callback handlers not yet registered, may be caused by instance intitlization being slow.
 * We therefore retry typed handlers by rescheduling the callback to retry invocation pending a initialization event in the interim.
 * @param resp context, message tuple 
 * @return true  if a deffered typed callback handler, where we must retry, we avoid reclaiming message objects until callback delivery occurs.
 * @return false 
 */
bool AsyncMessageManager::async_source_cb(msg_async_response_t *resp)
{
    DIGGI_ASSERT(resp);
    auto _this = (AsyncMessageManager *)resp->context;
    DIGGI_ASSERT(_this);
    auto msg = resp->msg;
    DIGGI_ASSERT(((size_t)resp->msg->src.fields.thread) == (size_t)_this->diggiapi->GetThreadPool()->currentThreadId());
    DIGGI_ASSERT(((size_t)resp->msg->dest.fields.thread) == (size_t)_this->diggiapi->GetThreadPool()->currentThreadId());
    DIGGI_TRACE(_this->diggiapi->GetLogObject(),
                LogLevel::LDEBUG,
                "Incomming from: %" PRIu64 ", to: %" PRIu64 ", id: %lu, type: %d , size: %lu \n",
                msg->src.raw,
                msg->dest.raw,
                msg->id,
                msg->type,
                msg->size);

    if (msg->id != 0)
    {
        auto handler = _this->async_handler_map[msg->id];
        if (handler.cb != nullptr)
        {
            resp->context = handler.arg;
            DIGGI_TRACE(_this->diggiapi->GetLogObject(),
                        LogLevel::LDEBUG,
                        "Delivering continuation of message flow from: %" PRIu64 ", to: %" PRIu64 ", id: %lu, type: %d, size: %lu \n",
                        msg->src.raw,
                        msg->dest.raw,
                        msg->id,
                        msg->type,
                        msg->size);
            handler.cb(resp, 1);
            free(resp);
            return false;
        }
    }
    /*
		In the case where the type has not yet been registered by application func
	*/
    auto type_handler = _this->type_handler_map[msg->type];
    if (type_handler.cb == nullptr)
    {
        DIGGI_TRACE(_this->diggiapi->GetLogObject(),
                    LogLevel::LDEBUG,
                    "type not registered for message from: %" PRIu64 ", to: %" PRIu64 ", id: %lu, type: %d , size: %lu \n",
                    msg->src.raw,
                    msg->dest.raw,
                    msg->id,
                    msg->type,
                    msg->size);

        _this->diggiapi->GetThreadPool()->Schedule(defered_async_source_cb, resp, __PRETTY_FUNCTION__);
        return true;
    }
    else
    {
        resp->context = type_handler.arg;
        DIGGI_TRACE(_this->diggiapi->GetLogObject(),
                    LogLevel::LDEBUG,
                    "Delivering type directly from: %" PRIu64 ", to: %" PRIu64 ", id: %lu, type: %d , size: %lu , session_count:%lu\n",
                    msg->src.raw,
                    msg->dest.raw,
                    msg->id,
                    msg->type,
                    msg->size,
                    msg->session_count);
        DIGGI_ASSERT(type_handler.cb);
        type_handler.cb(resp, 1);
    }
    free(resp);
    return false;
}
/**
 * @brief Deffered delivery of typed callback message.
 * In the event of successs, message objects are reclaimed.
 * May retry by scheduling itself indefinently.
 * @warning bugs leading to indefinete rescheduling of this function may be hard to catch, recomend some messaging or a retry limit.
 * @param ptr msg_async_response_t containing message and context(this)
 * @param status 
 */
void AsyncMessageManager::defered_async_source_cb(void *ptr, int status)
{

    DIGGI_ASSERT(ptr);
    auto resp = (msg_async_response_t *)ptr;
    auto msg = resp->msg;
    auto _this = (AsyncMessageManager *)resp->context;
    DIGGI_ASSERT(((size_t)resp->msg->src.fields.thread) == (size_t)_this->diggiapi->GetThreadPool()->currentThreadId());
    DIGGI_ASSERT(((size_t)resp->msg->dest.fields.thread) == (size_t)_this->diggiapi->GetThreadPool()->currentThreadId());
    DIGGI_TRACE(_this->diggiapi->GetLogObject(),
                LogLevel::LDEBUG,
                "Defered from: %" PRIu64 ", to: %" PRIu64 ", id: %lu, type: %d , size: %lu \n",
                msg->src.raw,
                msg->dest.raw,
                msg->id,
                msg->type,
                msg->size);

    auto type_handler = _this->type_handler_map[resp->msg->type];

    if (type_handler.cb != nullptr)
    {
        DIGGI_TRACE(_this->diggiapi->GetLogObject(),
                    LogLevel::LDEBUG, "Successfull defered from: %" PRIu64 ", to: %" PRIu64 ", id: %lu, type: %d , size: %lu \n",
                    msg->src.raw,
                    msg->dest.raw,
                    msg->id,
                    msg->type,
                    msg->size);

        resp->context = type_handler.arg;
        type_handler.cb(resp, 1);
        // memset(msg, 0, msg->size);
        lf_send(_this->global_mem_buf, resp->msg, _this->global_thread_id);
        free(resp);
    }
    else
    {
        /*
			If type has yet not been scheduled, we retry
		*/
        DIGGI_TRACE(_this->diggiapi->GetLogObject(),
                    LogLevel::LDEBUG, "Retrying: type not registered for message from: %" PRIu64 ", to: %" PRIu64 ", id: %lu, type: %d , size: %lu \n",
                    msg->src.raw,
                    msg->dest.raw,
                    msg->id,
                    msg->type,
                    msg->size);

        _this->diggiapi->GetThreadPool()->Schedule(defered_async_source_cb, resp, __PRETTY_FUNCTION__);
    }
}
