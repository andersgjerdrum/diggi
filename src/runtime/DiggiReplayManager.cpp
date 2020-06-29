#include "runtime/DiggiReplayManager.h"
#include "messaging/Util.h"
DiggiReplayManager::DiggiReplayManager(
    IThreadPool *threadpool,
    std::map<std::string, aid_t> nameservice_updates,
    aid_t self_id,
    ITamperProofLog *inputl,
    std::string input_log_identifier,
    ITamperProofLog *outputl,
    std::string output_log_identifier,
    ILog *log,
    unsigned int expected_thread)
    : threadpool(threadpool),
      self(self_id),
      next_in_line(0),
      monotonic_msg_id(2),
      input_log(inputl),
      output_log(outputl),
      logger(log),
      this_thread(expected_thread),
      input_id(input_log_identifier),
      output_id(output_log_identifier)
{
    name_servicemap = nameservice_updates;
}

void DiggiReplayManager::Start(async_cb_t cb, void *ptr)
{
    DIGGI_ASSERT(output_log);
    DIGGI_ASSERT(input_log);
    output_log->initLog(
        WRITE_LOG,
        output_id,
        nullptr,
        nullptr);
    input_log->replayLogEntry(
        input_id,
        DiggiReplayManager::replayEnqueue,
        cb,
        this,
        ptr);
}

DiggiReplayManager::~DiggiReplayManager()
{
    output_log->Stop();
    input_log->Stop();
}
void DiggiReplayManager::replayEnqueue(void *ptr, int status)
{
    auto resp = (msg_async_response_t *)ptr;
    DIGGI_ASSERT(resp);
    DIGGI_ASSERT(resp->context);
    DIGGI_ASSERT(resp->msg);
    auto _this = (DiggiReplayManager *)resp->context;
    auto msg = resp->msg;

    ///Ingnore initial session requests and key exchange contexts
    ///A successfull agreement will always be followed by another send
    ///if by chance one should arrive, we do not support it.
    DIGGI_TRACE(_this->logger,
                LogLevel::LDEBUG,
                "Inbound from: %" PRIu64 ", to: %" PRIu64 ", id: %lu, type: %d , size: %lu session_id = %lu, next_in_line = %lu\n",
                msg->src.raw,
                msg->dest.raw,
                msg->id,
                msg->type,
                msg->size,
                msg->session_count,
                _this->next_in_line);
    if (msg->type == SESSION_REQUEST)
    {
        DIGGI_ASSERT(false);
    }
    if (_this->next_in_line != msg->session_count)
    {
        auto resp1 = new msg_async_response_t();
        resp1->context = _this;
        resp1->msg = COPY(msg_t, msg, msg->size);
        _this->threadpool->Schedule(DiggiReplayManager::typehandlerDeffered, resp1, __PRETTY_FUNCTION__);
        return;
    }

    if (msg->id != 0)
    {
        auto handler = _this->async_handler_map[msg->id];
        if (handler.cb != nullptr)
        {
            _this->next_in_line++;
            // Utils::print_byte_array(msg->data,msg->size - sizeof(msg_t));
            DIGGI_TRACE(_this->logger,
                        LogLevel::LDEBUG,
                        "Incomming one-of from: %" PRIu64 ", to: %" PRIu64 ", id: %lu, type: %d , size: %lu session_id = %lu, next_in_line = %lu\n",
                        msg->src.raw,
                        msg->dest.raw,
                        msg->id,
                        msg->type,
                        msg->size,
                        msg->session_count,
                        _this->next_in_line);
            resp->context = handler.arg;
            handler.cb(resp, 1);
            return;
        }
    }
    /*
		In the case where the type has not yet been registered by application func
	*/
    auto type_handler = _this->type_handler_map[msg->type];
    if (type_handler.cb == nullptr)
    {
        auto resp1 = new msg_async_response_t();
        resp1->context = _this;
        resp1->msg = COPY(msg_t, resp->msg, resp->msg->size);
        _this->threadpool->Schedule(DiggiReplayManager::typehandlerDeffered, resp1, __PRETTY_FUNCTION__);
        return;
    }
    else
    {
        _this->next_in_line++;
        DIGGI_TRACE(_this->logger,
                    LogLevel::LDEBUG,
                    "Incomming  typed from: %" PRIu64 ", to: %" PRIu64 ", id: %lu, type: %d , size: %lu session_id = %lu, next_in_line = %lu\n",
                    msg->src.raw,
                    msg->dest.raw,
                    msg->id,
                    msg->type,
                    msg->size,
                    msg->session_count,
                    _this->next_in_line);
        resp->context = type_handler.arg;
        DIGGI_ASSERT(type_handler.cb);
        type_handler.cb(resp, 1);
    }
    return;
}
void DiggiReplayManager::typehandlerDeffered(void *ptr, int status)
{
    auto resp = (msg_async_response_t *)ptr;
    DIGGI_ASSERT(resp);
    DIGGI_ASSERT(resp->context);
    DIGGI_ASSERT(resp->msg);
    auto msg = resp->msg;
    auto _this = (DiggiReplayManager *)resp->context;
    DIGGI_TRACE(_this->logger,
                LogLevel::LDEBUG,
                "Inbound  defered from: %" PRIu64 ", to: %" PRIu64 ", id: %lu, type: %d , size: %lu session_id = %lu, next_in_line = %lu\n",
                msg->src.raw,
                msg->dest.raw,
                msg->id,
                msg->type,
                msg->size,
                msg->session_count,
                _this->next_in_line);
    if (_this->next_in_line != msg->session_count)
    {
        _this->threadpool->Schedule(DiggiReplayManager::typehandlerDeffered, resp, __PRETTY_FUNCTION__);
        return;
    }
    if (msg->id != 0)
    {
        auto handler = _this->async_handler_map[msg->id];

        if (handler.cb != nullptr)
        {
            _this->next_in_line++;

            DIGGI_TRACE(_this->logger,
                        LogLevel::LDEBUG,
                        "Incomming from: %" PRIu64 ", to: %" PRIu64 ", id: %lu, type: %d , size: %lu, session_id = %lu, next_in_line = %lu\n",
                        msg->src.raw,
                        msg->dest.raw,
                        msg->id,
                        msg->type,
                        msg->size,
                        msg->session_count,
                        _this->next_in_line);
            resp->context = handler.arg;
            handler.cb(resp, 1);
            free(resp->msg);
            resp->msg = nullptr;
            delete resp;
            resp = nullptr;
            return;
        }
    }

    auto type_handler = _this->type_handler_map[msg->type];
    if (type_handler.cb == nullptr)
    {
        _this->threadpool->Schedule(DiggiReplayManager::typehandlerDeffered, resp, __PRETTY_FUNCTION__);
        return;
    }
    else
    {
        _this->next_in_line++;
        DIGGI_TRACE(_this->logger,
                    LogLevel::LDEBUG,
                    "Incomming from: %" PRIu64 ", to: %" PRIu64 ", id: %lu, type: %d , size: %lu, session_id = %lu, next_in_line = %lu\n",
                    msg->src.raw,
                    msg->dest.raw,
                    msg->id,
                    msg->type,
                    msg->size,
                    msg->session_count,
                    _this->next_in_line);
        resp->context = type_handler.arg;
        DIGGI_ASSERT(type_handler.cb);
        type_handler.cb(resp, 1);
        free(resp->msg);
        resp->msg = nullptr;
        delete resp;
        resp = nullptr;
    }
    return;
}

void DiggiReplayManager::endAsync(msg_t *msg)
{
    DIGGI_ASSERT(msg);
    DIGGI_TRACE(logger,
                LogLevel::LDEBUG,
                "Replay Ending session between: %" PRIu64 ", and: %" PRIu64 ", id: %lu size: %lu \n",
                msg->src.raw,
                msg->dest.raw,
                msg->id,
                msg->size);
    DIGGI_ASSERT(msg->id > 0);
    /*
        Zero structure to highlight reuse after erase, as map is not freed.
    */
    async_handler_map[msg->id] = {0};
    async_handler_map.erase(msg->id);
    msg->id = 0;
}
void DiggiReplayManager::Send(msg_t *msg, async_cb_t cb, void *cb_context)
{
    DIGGI_ASSERT(msg);
    if (msg->type == SESSION_REQUEST)
    {
        ///not implemented yet.
        DIGGI_ASSERT(false);
    }

    msg->src.fields.thread = (uint8_t)threadpool->currentThreadId();
    output_log->appendLogEntry(msg);
    if (cb == nullptr)
    {
        DIGGI_ASSERT(cb_context == nullptr);
        if (msg->id)
        {
            endAsync(msg);
        }
    }
    else
    {
        if (msg->id == 0)
        {
            msg->id = getMessageId(self.raw);
        }
        async_handler_map[msg->id].cb = cb;
        async_handler_map[msg->id].arg = cb_context;
    }
    DIGGI_TRACE(logger,
                LogLevel::LDEBUG,
                "Replay Sending message expecting response from: %" PRIu64 ", to: %" PRIu64 ", id: %lu size: %lu, type = %d, session_id = %lu, next_in_line = %lu\n",
                msg->src.raw,
                msg->dest.raw,
                msg->id,
                msg->size,
                msg->type,
                msg->session_count,
                next_in_line);
    // next_in_line[msg->dest.raw]++;
    free(msg);
}
msg_t *DiggiReplayManager::allocateMessage(std::string destination, size_t payload_size, msg_convention_t async, msg_delivery_t delivery)
{
    DIGGI_ASSERT(name_servicemap.find(destination) != name_servicemap.end());
    return allocateMessage(name_servicemap[destination], payload_size, async, delivery);
}
msg_t *DiggiReplayManager::allocateMessage(aid_t destination, size_t payload_size, msg_convention_t async, msg_delivery_t delivery)
{
    DIGGI_ASSERT(this_thread == threadpool->currentThreadId());
    auto msg = (msg_t *)malloc(sizeof(msg_t) + payload_size);
    msg->size = sizeof(msg_t) + payload_size;
    msg->session_count = 0;
    msg->omit_from_log = 0;
    msg->delivery = delivery;
    msg->src = self;
    msg->dest = destination;
    msg->id = 0;
    return msg;
}
msg_t *DiggiReplayManager::allocateMessage(msg_t *msg, size_t payload_size)
{
    DIGGI_ASSERT(this_thread == threadpool->currentThreadId());
    DIGGI_ASSERT(msg->src.raw != 0);
    DIGGI_ASSERT(msg->dest.raw != 0);
    DIGGI_ASSERT((payload_size + sizeof(msg_t)) < MAX_DIGGI_MEM_SIZE);

    auto msg_n = (msg_t *)malloc(sizeof(msg_t) + payload_size);

    msg_n->delivery = msg->delivery;
    msg_n->size = sizeof(msg_t) + payload_size;
    msg_n->type = msg->type;
    msg_n->id = msg->id;
    msg_n->omit_from_log = msg->omit_from_log;
    msg_n->session_count = msg->session_count + 1; /*attempt to increase session count*/
    msg_n->src = msg->src;
    msg_n->dest = msg->dest;
    return msg_n;
}

void DiggiReplayManager::registerTypeCallback(async_cb_t cb, msg_type_t ty, void *arg)
{
    DIGGI_ASSERT(cb);
    type_handler_map[ty].cb = cb;
    type_handler_map[ty].status = 1;
    type_handler_map[ty].arg = arg;
    DIGGI_TRACE(logger, LogLevel::LDEBUG, "Replay register callback for type=%d\n", ty);
}

std::map<std::string, aid_t> DiggiReplayManager::getfuncNames()
{
    DIGGI_ASSERT(this_thread == threadpool->currentThreadId());
    return name_servicemap;
}
unsigned long DiggiReplayManager::getMessageId(unsigned long func_identifier)
{
    monotonic_msg_id++;
    auto retval = (func_identifier << 32) | (monotonic_msg_id & 0x00000000ffffffff);
    DIGGI_TRACE(logger, LDEBUG, "new Replayable id=%lu\n", retval);

    return retval;
}