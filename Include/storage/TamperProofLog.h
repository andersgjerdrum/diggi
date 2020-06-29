#ifndef TAMPERPROOF_LOG_H
#define TAMPERPROOF_LOG_H
#include "runtime/DiggiAPI.h"
#include "AsyncContext.h"
#include "messaging/Pack.h"
typedef enum LogMode
{
    READ_LOG,
    WRITE_LOG,
} LogMode;

class ITamperProofLog
{
public:
    ITamperProofLog() {}
    virtual ~ITamperProofLog() {}
    virtual void appendLogEntry(msg_t *msg) = 0;
    virtual void replayLogEntry(std::string log_identifier, async_cb_t cb, async_cb_t completion_callback, void *ptr, void *complete_ptr) = 0;
    virtual void Stop() = 0;
    virtual void initLog(LogMode mode, std::string log_identifier, async_cb_t cb, void *ptr) = 0;
};

class TamperProofLog : public ITamperProofLog
{
    IDiggiAPI *api;
    int file_descriptor;
    size_t next_id;

public:
    TamperProofLog(IDiggiAPI *dapi);
    void appendLogEntry(msg_t *msg);
    void replayLogEntry(std::string log_identifier, async_cb_t cb, async_cb_t completion_callback, void *ptr, void *complete_ptr);
    void Stop();
    void initLog(LogMode mode, std::string log_identifier, async_cb_t cb, void *ptr);
    static void replayLoop_read_header_cb(void *ptr, int status);
    static void replayLoop_read_body_cb(void *ptr, int status);
    static void replayLoop_deliver_cb(void *ptr, int status);
    static void replayLoop_Deliver_Internal_cb(void *ptr, int status);
    static void replayLoop_open_cb(void *ptr, int status);
    static void retryInit(void *ptr, int status);
    static void retryAppendLogEntry(void *ptr, int status);
    static void createLog_cb(void *ptr, int status);
    static void writeLog_cb(void *ptr, int status);
};
#endif