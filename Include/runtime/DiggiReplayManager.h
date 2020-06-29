#ifndef DIGGI_REPLAY_M
#define DIGGI_REPLAY_M

#include "datatypes.h"
#include "messaging/AsyncMessageManager.h"
#include "sgx/DynamicEnclaveMeasurement.h"
#include "messaging/IIASAPI.h"
#include "storage/TamperProofLog.h"
class DiggiReplayManager : public IMessageManager
{
    /// diggi threadpool api reference
    IThreadPool *threadpool;
    aid_t self;

    unsigned long getMessageId(unsigned long func_identifier);
    std::map<msg_type_t, async_work_t> type_handler_map;
    std::map<uint64_t, async_work_t> async_handler_map;
    uint64_t next_in_line;
    unsigned monotonic_msg_id;
    ITamperProofLog *input_log;
    ITamperProofLog *output_log;
    /// diggi logger api reference
    ILog *logger;
    /// own thread id, each SMM and AMM is associated with a unique id. AMM guarantees delivery to same thread.
    int this_thread;
    std::string input_id;
    std::string output_id;

public:
    /// mapping of Human Readable Name(HRD) to unique diggi instance identifier. Contains recipients allowed for this enclave, encoded in configuration as part of binary.
    std::map<std::string, aid_t> name_servicemap;
    DiggiReplayManager(
        IThreadPool *threadpool,
        std::map<std::string, aid_t> nameservice_updates,
        aid_t self_id,
        ITamperProofLog *inputl,
        std::string input_log_identifier,
        ITamperProofLog *outputl,
        std::string output_log_identifier,
        ILog *log,
        unsigned int expected_thread);
    ~DiggiReplayManager();
    static void replayEnqueue(void *ptr, int status);
    static void typehandlerDeffered(void *ptr, int status);
    static void replayComplete(void *ptr, int status);
    static void initComplete(void *ptr, int status);
    void endAsync(msg_t *msg);
    void Send(msg_t *msg, async_cb_t cb, void *cb_context);
    msg_t *allocateMessage(std::string destination, size_t payload_size, msg_convention_t async, msg_delivery_t delivery);
    msg_t *allocateMessage(aid_t destination, size_t payload_size, msg_convention_t async, msg_delivery_t delivery);
    msg_t *allocateMessage(msg_t *msg, size_t payload_size);
    void registerTypeCallback(async_cb_t cb, msg_type_t type, void *ctx);
    std::map<std::string, aid_t> getfuncNames();
    void Start(async_cb_t cb, void *ptr);
};
#endif