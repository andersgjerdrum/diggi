#include "storage/TamperProofLog.h"

TamperProofLog::TamperProofLog(IDiggiAPI *dapi) : api(dapi),
                                                  file_descriptor(0),
                                                  next_id(0)
{
    DIGGI_ASSERT(api);
    DIGGI_ASSERT(api->GetStorageManager());
}
typedef struct AsyncContext<TamperProofLog *, msg_t *, async_cb_t, void *, async_cb_t, void *> log_read_entry_ctx;
void TamperProofLog::replayLogEntry(std::string log_identifier, async_cb_t cb, async_cb_t completion_callback, void *ptr, void *complete_ptr)
{
    ///Must copy as it is asynchronous.
    DIGGI_TRACE(api->GetLogObject(), LogLevel::LDEBUG, "Starting log replay of input\n");

    auto ctx = new log_read_entry_ctx(this, nullptr, cb, ptr, completion_callback, complete_ptr);
    DIGGI_ASSERT(file_descriptor == 0);

    std::string filename = api->GetLogObject()->GetfuncName() + log_identifier + ".tamperproof.log";
    api->GetStorageManager()->async_open(
        filename.c_str(),
        O_RDWR,
        S_IRWXU,
        TamperProofLog::replayLoop_open_cb,
        ctx,
        true,
        true);
}

void TamperProofLog::replayLoop_read_body_cb(void *ptr, int status)
{

    auto resp = (msg_async_response_t *)ptr;
    uint8_t *mvptr = resp->msg->data;
    auto ctx = (log_read_entry_ctx *)resp->context;

    size_t count = Pack::unpack<size_t>(&mvptr);
    Pack::unpack<off_t>(&mvptr);

    if (count == 0)
    {
        DIGGI_TRACE(ctx->item1->api->GetLogObject(), LogLevel::LDEBUG, "Replay log done!\n");

        ctx->item5(ctx->item6, 1);
        delete ctx;
        ctx = nullptr;
        return;
        ///replay done
    }
    DIGGI_ASSERT(count == sizeof(msg_t));
    msg_t *msgheader = (msg_t *)mvptr;
    auto _this = ctx->item1;

    ///ensuring delivery loop occurs before the next read
    DIGGI_ASSERT(ctx->item2 == nullptr);
    DIGGI_ASSERT(msgheader->size > 0);
    ctx->item2 = ALLOC_P(msg_t, msgheader->size - sizeof(msg_t));
    memcpy(ctx->item2, msgheader, sizeof(msg_t));
    if (msgheader->size == sizeof(msg_t))
    {
        auto rsp = new msg_async_response_t();
        rsp->context = ctx;
        rsp->msg = ctx->item2;
        replayLoop_deliver_cb(rsp, 1);
        DIGGI_ASSERT(ctx->item2 == nullptr);
        delete rsp;
    }
    else
    {
        _this->api->GetStorageManager()->async_read(
            _this->file_descriptor,
            nullptr,
            msgheader->size - sizeof(msg_t),
            TamperProofLog::replayLoop_deliver_cb,
            ctx,
            true,
            true);
    }
}
void TamperProofLog::replayLoop_deliver_cb(void *ptr, int status)
{

    auto resp = (msg_async_response_t *)ptr;
    auto ctx = (log_read_entry_ctx *)resp->context;
    DIGGI_TRACE(ctx->item1->api->GetLogObject(), LogLevel::LDEBUG, "Replay log deliver\n");

    uint8_t *mvptr = resp->msg->data;
    if (resp->msg->size > sizeof(msg_t))
    {
        size_t count = Pack::unpack<size_t>(&mvptr);
        Pack::unpack<off_t>(&mvptr);
        DIGGI_ASSERT(ctx->item2);
        DIGGI_ASSERT(ctx->item2->size <= resp->msg->size - (sizeof(off_t) + sizeof(size_t)));
        Pack::unpackBuffer(&mvptr, ctx->item2->data, ctx->item2->size - sizeof(msg_t));
        DIGGI_ASSERT(count == ctx->item2->size - sizeof(msg_t));
    }

    auto ctx_n = new log_read_entry_ctx(
        ctx->item1,
        nullptr,
        ctx->item3,
        ctx->item4,
        ctx->item5,
        ctx->item6);
    ///Allow simultaneous prefetching of next log entry.
    ctx->item1->api->GetThreadPool()->Schedule(TamperProofLog::replayLoop_Deliver_Internal_cb, ctx, __PRETTY_FUNCTION__);

    ctx->item1->api->GetStorageManager()
        ->async_read(
            ctx->item1->file_descriptor,
            nullptr,
            sizeof(msg_t),
            TamperProofLog::replayLoop_read_body_cb,
            ctx_n,
            true,
            true);
}

void TamperProofLog::replayLoop_Deliver_Internal_cb(void *ptr, int status)
{
    DIGGI_ASSERT(ptr);
    auto ctx = (log_read_entry_ctx *)ptr;
    auto rsp = new msg_async_response_t();
    rsp->msg = ctx->item2;
    rsp->context = ctx->item4;

    ctx->item3(rsp, 1);

    free(ctx->item2);
    ctx->item2 = nullptr;
    delete ctx;
    delete rsp;
}

void TamperProofLog::replayLoop_open_cb(void *ptr, int status)
{

    DIGGI_ASSERT(ptr);
    auto resp = (msg_async_response_t *)ptr;
    uint8_t *mvptr = resp->msg->data;
    auto ctx = (log_read_entry_ctx *)resp->context;
    auto _this = ctx->item1;
    _this->file_descriptor = Pack::unpack<int>(&mvptr);
    DIGGI_ASSERT(_this->file_descriptor > 0);
    DIGGI_TRACE(_this->api->GetLogObject(), LogLevel::LDEBUG, "Replay log init complete fd=%d\n", _this->file_descriptor);

    _this->api->GetStorageManager()->async_read(
        _this->file_descriptor,
        nullptr,
        sizeof(msg_t),
        TamperProofLog::replayLoop_read_body_cb,
        ctx,
        true,
        true);
}

typedef struct AsyncContext<TamperProofLog *, async_cb_t, void *, std::string, LogMode> log_init_ctx;

void TamperProofLog::initLog(LogMode mode, std::string log_identifier, async_cb_t cb, void *ptr)
{
    DIGGI_ASSERT(api);
    auto ctx = new log_init_ctx(this, cb, ptr, log_identifier, mode);

    ///do not try logging before storage infra is set up
    if (!ctx->item1->api->GetStorageManager() || !ctx->item1->api->GetMessageManager())
    {
        api->GetThreadPool()->Schedule(TamperProofLog::retryInit, ctx, __PRETTY_FUNCTION__);
        return;
    }
    retryInit(ctx, 1);
}
void TamperProofLog::retryInit(void *ptr, int status)
{

    DIGGI_ASSERT(ptr);
    auto ctx = (log_init_ctx *)ptr;
    DIGGI_TRACE(ctx->item1->api->GetLogObject(), LogLevel::LDEBUG, "Retry log init\n");
    ///do not try logging before storage infra is set up
    if (!ctx->item1->api->GetStorageManager() || !ctx->item1->api->GetMessageManager())
    {
        ctx->item1->api->GetThreadPool()->Schedule(TamperProofLog::retryInit, ctx, __PRETTY_FUNCTION__);
        return;
    }

    int oflags = (ctx->item5 == READ_LOG) ? (O_RDWR) : (O_RDWR | O_TRUNC | O_CREAT);
    std::string filename = ctx->item1->api->GetLogObject()->GetfuncName() + ctx->item4 + ".tamperproof.log";
    ctx->item1->api->GetStorageManager()->async_open(
        filename.c_str(),
        oflags,
        S_IRWXU,
        TamperProofLog::createLog_cb,
        ctx,
        true,
        true);
}

void TamperProofLog::createLog_cb(void *ptr, int status)
{
    DIGGI_ASSERT(ptr);
    auto resp = (msg_async_response_t *)ptr;
    uint8_t *mvptr = resp->msg->data;
    auto ctx = (log_init_ctx *)resp->context;

    ctx->item1->file_descriptor = Pack::unpack<int>(&mvptr);
    DIGGI_TRACE(ctx->item1->api->GetLogObject(), LogLevel::LDEBUG, "Log Init Complete fd=%d\n", ctx->item1->file_descriptor);

    DIGGI_ASSERT(ctx->item1->file_descriptor > 0);
    if (ctx->item2)
    {
        ctx->item2(ctx->item3, 1);
    }
    delete ctx;
    ctx = nullptr;
}

typedef struct AsyncContext<TamperProofLog *, msg_t *> log_append_entry_ctx;
void TamperProofLog::appendLogEntry(msg_t *msg)
{
    ///Must copy as it is asynchronous.
    auto ctx = new log_append_entry_ctx(this, COPY(msg_t, msg, msg->size));
    ctx->item2->session_count = next_id++;
    if (file_descriptor == 0)
    {
        api->GetThreadPool()->Schedule(TamperProofLog::retryAppendLogEntry, ctx, __PRETTY_FUNCTION__);
        return;
    }
    retryAppendLogEntry(ctx, 1);
}
void TamperProofLog::retryAppendLogEntry(void *ptr, int status)
{
    DIGGI_ASSERT(ptr);
    auto ctx = (log_append_entry_ctx *)ptr;
    if (ctx->item1->file_descriptor == 0)
    {
        ctx->item1->api->GetThreadPool()->Schedule(TamperProofLog::retryAppendLogEntry, ctx, __PRETTY_FUNCTION__);
        return;
    }
    DIGGI_TRACE(ctx->item1->api->GetLogObject(),
                LogLevel::LDEBUG,
                "Appending message to log from: %" PRIu64 ", to: %" PRIu64 ", id: %lu, type: %d , size: %lu \n",
                ctx->item2->src.raw,
                ctx->item2->dest.raw,
                ctx->item2->id,
                ctx->item2->type,
                ctx->item2->size);

    ctx->item1->api->GetStorageManager()->async_write(
        ctx->item1->file_descriptor,
        (const void *)ctx->item2,
        ctx->item2->size,
        TamperProofLog::writeLog_cb,
        ctx,
        true,
        true);
}
void TamperProofLog::writeLog_cb(void *ptr, int status)
{
    DIGGI_ASSERT(ptr);
    auto resp = (msg_async_response_t *)ptr;
    auto ctx = (log_append_entry_ctx *)resp->context;
    free(ctx->item2);
    ctx->item2 = nullptr;
    delete ctx;
    ctx = nullptr;
}

void TamperProofLog::Stop()
{
    api->GetStorageManager()->async_close(file_descriptor, true);
    file_descriptor = 0;
}
