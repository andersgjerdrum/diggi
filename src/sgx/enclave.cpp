/**
 * @file enclave.cpp
 * @author Anders Gjerdrum (anders.t.gjerdrum@uit.no)
 * @brief Entrypoint for encalve execution, responsible for bootstrapping the diggi trusted runtime.
 * @version 0.1
 * @date 2020-01-29
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#include "sgx/enclave.h"
#ifdef DIGGI_ENCLAVE

///func api reference
static DiggiAPI *acontext = nullptr;

///inbound raw message queue reference
static lf_buffer_t *inbound_queue = nullptr;

///outbound raw message queue reference
static lf_buffer_t *outbound_queue = nullptr;

///reference to threadpool, threads busy wait on datastructure being initialized, before entering, and must not be cached, hence volatile
static ThreadPool *volatile threadpool = nullptr;
static volatile int threadcount_initialized = 0;
/**
 * @brief Function which all threads enter.
 * All entering threads wait until thread is intiialized by init thread(untrusted runtime thread)
 * afterwards increment a counter of intialized threads, which init waits for to ensure runtime intialization is complete, before init thread exits to untruste runtime
 */
void enclave_thread_entry_function()
{
    while (threadpool == nullptr)
        ;
    __sync_fetch_and_add(&threadcount_initialized, 1);
    threadpool->InitializeThread();
}

void client_stop_trampoline(void *ptr, int status);

/**
 * To simplify marshaling map is converted to array before being copied into enclave memory.
 * This function recreates map between aid 64 bit instance identifier, and the human readable name.
 * Similar to how DNS works with IP addresses.
 * @param mapdata array of entries
 * @param count count of entries.
 * @return std::map<string, aid_t> map of human readable names to instance indentifier (aid_t)
 */
std::map<string, aid_t> convert_2_map_representation(name_service_update_t *mapdata, size_t count)
{

    DIGGI_ASSERT(count);
    DIGGI_ASSERT(mapdata);
    std::map<string, aid_t> nameservice_map;
    for (unsigned i = 0; i < count; i++)
    {
        nameservice_map[std::string(mapdata[i].name, mapdata[i].name_size)] = mapdata[i].physical_address;
    }
    return nameservice_map;
}

/**
 * @brief wrapper funciton for starting instance on runtime thread instead of init thread provided by untrusted runtime.
 * Init and start invoked on same thread.
 * Start may only start once init is done.
 * Input is a reference to the Diggi api holding all interfaces availible to instances.
 * @see DiggiAPI::DiggiAPI
 * @param ptr 
 * @param status 
 */
void start_init(void *ptr, int status)
{
    DIGGI_ASSERT(ptr);
    auto acontext = (DiggiAPI *)ptr;
    func_init(acontext, 1);
    func_start(acontext, 1);
}

/**
 * @brief initial entry point for enclave
 * Executes on the host untrusted runtimes own thread (the init thread)
 * @param self the unique identifier of this instances
 * @param id sgx implementation identifier of this enclave.
 * @param base_thread_id Host relative base thread id. Threads are attributed to instances in ranges consisting of base + count.
 * @param global_memory_buffer refference to untrusted memory object buffer shared by all instances cohosted on this untrusted runtime, used for allocating messages bound to another instance, and releasing inbound messages after use.
 * @param input_q inbound message queue from untrusted runtime
 * @param output_q outbound message quue to untrusted runtime
 * @param mapdata map of message queue to other instances on same host, used to reduce untrusted runtime message processing preassure, messages delivered directly onto input queue.
 * @param count count of message queues.
 * @param func_name human readable name of current instance
 * @param expected_threads threads allocate to this instances, tested before init completes.
 */
void client(aid_t self,
            sgx_enclave_id_t id,
            size_t base_thread_id,
            lf_buffer_t *global_memory_buffer,
            lf_buffer_t *input_q,
            lf_buffer_t *output_q,
            name_service_update_t *mapdata,
            size_t count,
            const char *func_name,
            size_t expected_threads)
{
    std::vector<name_service_update_t> outbound_queues(mapdata, mapdata + count);

    auto map_nameservice = convert_2_map_representation(mapdata, count);
    DIGGI_ASSERT(input_q);
    DIGGI_ASSERT(output_q);

    /*wait for threads to be initialized*/
    threadpool = new ThreadPool(expected_threads, ENCLAVE_MODE, std::string(func_name));
    while (threadcount_initialized < expected_threads)
        ;
    zcstring convert(static_attested_diggi_configuration);
    json_node conf(convert);

    inbound_queue = input_q;
    outbound_queue = output_q;

    auto log_r = new StdLogger(threadpool);
    auto loglvl = LogLevel::LRELEASE;
    if (conf.contains("LogLevel"))
    {
        loglvl = (conf["LogLevel"].value == "LDEBUG") ? LogLevel::LDEBUG : LogLevel::LRELEASE;
    }
    log_r->SetLogLevel(loglvl);
    log_r->SetFuncId(self, func_name);

    DIGGI_ASSERT(acontext == nullptr);

    acontext = new DiggiAPI(
        threadpool,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        log_r,
        self,
        nullptr);

    acontext->SetFuncConfig(conf);
    SET_DIGGI_GLOBAL_CONTEXT(acontext);

    acontext->SetEnclaveId(id);

    log_r->Log("This enclaves address=%" PRIu64 "\n", self.raw);

    bool skip_attestation = false;
    if (conf.contains("skip-attestation"))
    {
        skip_attestation = (conf["skip-attestation"].value == "1") ? true : false;
    }

    bool trusted_root = false;
    if (conf.contains("trusted-root"))
    {
        trusted_root = (conf["trusted-root"].value == "1") ? true : false;
    }

    bool dynamic_measurement = false;
    if (conf.contains("dynamic-measurement"))
    {
        dynamic_measurement = (conf["dynamic-measurement"].value == "1") ? true : false;
    }

    if (skip_attestation)
    {
        log_r->Log(LRELEASE, "Skipping attestation\n");
        DIGGI_ASSERT(!trusted_root);
    }

    bool replay_func = false;
    if (conf.contains("replay-func"))
    {
        replay_func = (conf["replay-func"].value == "1") ? true : false;
    }
    bool record_func = false;
    if (conf.contains("record-func"))
    {
        record_func = (conf["record-func"].value == "1") ? true : false;
    }
    sgx_report_t report;
    sgx_status_t ret = sgx_create_report(NULL, NULL, &report);
    DIGGI_ASSERT(ret == SGX_SUCCESS);
    log_r->Log(LRELEASE, "Enclave Measurement:\n");
    Utils::print_byte_array(report.body.mr_enclave.m, SGX_HASH_SIZE, log_r, LRELEASE);
    auto dynamicmeasurement = (dynamic_measurement)
                                  ? (new DynamicEnclaveMeasurement(report.body.mr_enclave.m, SGX_HASH_SIZE, acontext))
                                  : nullptr;
    auto shm_mngr = new StorageManager(acontext, new SGXSeal(CREATOR, !replay_func));
    acontext->SetStorageManager(shm_mngr);
    /*should be moved to run on sheduled thread*/
    auto tmm = ThreadSafeMessageManager::Create<SecureMessageManager, AsyncMessageManager>(
        acontext,
        inbound_queue,
        outbound_queue,
        (!skip_attestation)
            ? static_cast<IIASAPI *>(new AttestationAPI())
            : static_cast<IIASAPI *>(new NoAttestationAPI()),
        map_nameservice,
        outbound_queues,
        base_thread_id,
        global_memory_buffer,
        trusted_root,
        dynamicmeasurement,
        record_func,
        new SGXCryptoImpl());

    acontext->SetMessageManager(tmm);
    acontext->SetSignalHandler(new DiggiSignalHandler(tmm));

    if (replay_func)
    {
        DIGGI_ASSERT(!record_func);
        ///TODO: memleak
        auto storage_d_api = new DiggiAPI(acontext);
        storage_d_api->SetStorageManager(new StorageManager(storage_d_api, new SGXSeal(CREATOR, !replay_func)));

        tmm = ThreadSafeMessageManager::CreateReplay(
            storage_d_api,
            map_nameservice,
            self,
            client_stop_trampoline,
            storage_d_api);
        acontext->SetMessageManager(tmm);
        acontext->SetSignalHandler(new DiggiSignalHandler(tmm));
    }
    acontext->GetThreadPool()->ScheduleOn(0, start_init, acontext, __PRETTY_FUNCTION__);
}

/**
 * @brief out of bound stop signal from host untrusted runtime.
 * Allow gracefull shutdown of instance.
 * Destroys instance after function returns.
 * Relinquishes control of threads back to host. 
 * Since enclave, memory reclamation handled by System software regardsless, so free operations are not neccesary.
 */
void client_stop_trampoline(void *ptr, int status)
{
    DIGGI_ASSERT(ptr);
    auto api = (DiggiAPI *)ptr;
    api->GetSignalHandler()->Stopfunc();
}
void client_stop()
{
    DIGGI_ASSERT(acontext);
    acontext->GetLogObject()->Log("Shutting Down Enclave\n");

    /*stop event scheduled out of band on separate thread, to allow service in case of blocking*/
    func_stop(acontext, 1);

    /*
	All pending activites on threadpool are allowed to finish execution
	*/

    acontext->GetThreadPool()->Stop();
    auto mngr = acontext->GetMessageManager();
    /*
        No cleanup or free operations, as other threads migth still touch objects on way out and cause a SIGILL.
    */
}

#endif