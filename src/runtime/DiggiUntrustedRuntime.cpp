/**
 * @file funcruntime.cpp
 * @author Anders Gjerdrum (anders.t.gjerdrum@uit.no)
 * @brief Implementation of untrusted diggi runtime, handles deployment and message scheduling.
 * @version 0.1
 * @date 2020-01-30
 * 
 * @copyright Copyright (c) 2020 
 * 
 */
#include "runtime/DiggiUntrustedRuntime.h"

#ifdef UNTRUSTED_APP

/**
 * @brief class used by inter node networking to store connection info.
 * 
 */
class networkinfo
{
public:
    /// ip of remote diggi host instance (initialy specified in configuration.json)
    zcstring ip;
    /// port of remote diggi host instance (initialy specified in configuration.json)
    zcstring port;
    /// reference to asynchonous connection primitive used for networking.
    Connection *con;
    /// underlying protocol for connection, requires reliable delivery.
    IConnectionPrimitives *proto;
    networkinfo() : con(nullptr), proto(nullptr)
    {
    }
    networkinfo(zcstring prt, zcstring ipadr) : port(prt), ip(ipadr), con(nullptr), proto(nullptr)
    {
    }
};

///map of networkinfo instances, one for each remote diggi node. aid proc field used as index into map
static std::map<uint8_t, networkinfo *> *network_map = nullptr;
///context object used to store outbound message buffer, accounting to ensure local buffer is released after send.
typedef AsyncContext<lf_buffer_t *, msg_t *> proc_outbound_ctx_t;

/// reference to global memory buffer pool, used by all diggi instances hosted by this runtime to allocate message objects prior to send.
static lf_buffer_t *global_memory_buffer = nullptr;

/// Untrusted runtime accounting map for keping information about each diggi instance. indexec by aid.
static std::map<uint64_t, func_management_context_t> func_map;

///reference to the untrusted runtimes availible diggi api.
static DiggiAPI *proc_ctx = nullptr;
/// bolean set during process intialization determining if diggi instance should exit when all instances have exited voulentarily. used in acceptance tests to ensure gracefull shutdown.
static bool runtime_global_exit_when_done = false;

/**
 *  Initialize the enclave:
 *  Step 1: try to retrieve the launch token saved by last transaction
 *  Step 2: call sgx_create_enclave to initialize an enclave instance
 *  Step 3: save the launch token if it is updated
 */
void initialize_enclave(sgx_enclave_id_t *id, const char *enclave_filename)
{
    DIGGI_ASSERT(proc_ctx);
    sgx_launch_token_t token = {0};
    int updated = 0;

    auto sts = sgx_create_enclave(enclave_filename, 1, &token, &updated, id, NULL);
    DIGGI_TRACE(proc_ctx->GetLogObject(), LRELEASE, "Initializing enclave:%s with id: %lu\n", enclave_filename, *id);

    if (sts == 0x4001)
    {
        DIGGI_TRACE(proc_ctx->GetLogObject(), LRELEASE, "ERROR: AE service did not respond or the requested service is not supported\n.");
        DIGGI_ASSERT(sts == SGX_SUCCESS);
    }
    else if (sts == 0x2006)
    {
        DIGGI_TRACE(proc_ctx->GetLogObject(), LRELEASE, "ERROR: Cannot contact sgx device, either hardware does not support it or a BIOS firmware update is required\n");
        DIGGI_ASSERT(sts == SGX_SUCCESS);
    }
    else if (sts == 0x0001)
    {
        DIGGI_TRACE(proc_ctx->GetLogObject(), LRELEASE, "ERROR: System does not have SGX capabilities, recompile in simulation mode\n");
        DIGGI_ASSERT(sts == SGX_SUCCESS);
    }
    else if (sts != SGX_SUCCESS)
    {
        DIGGI_TRACE(proc_ctx->GetLogObject(), LRELEASE, "ERROR: Enclave initialize failed with error = 0x%04x\n", sts);
        DIGGI_ASSERT(sts == SGX_SUCCESS);
    }
}

///bolean value used to shut down message loop in the event of gracefull exit.
static volatile bool stop_message_loop = false;

/**
 * @brief message scheduler loop for internal messages.
 * Supports message scheduling to other diggi instances on same host but is only invoked when messages are sent to a remote diggi host.
 * Local messages are directly delivered to target instance, by injecting addressable inbound buffers into target instances
 * 
 * Each invocation of the below callback checs the outbound queues for messages and in the event of a remote message,
 * forwards handling to networking callback.
 * 
 * Before done, the function schedules a new invokation of itself onto the threadpool.
 * @param msg 
 * @param status 
 */
void message_sheduler_loop(void *msg, int status)
{
    __sync_synchronize();
    if (stop_message_loop)
    {
        DIGGI_TRACE(proc_ctx->GetLogObject(), LDEBUG, "diggi_base_executable message scheduler loop exiting...\n");
        return;
    }
    DIGGI_ASSERT(proc_ctx);
    for (
        std::map<uint64_t, func_management_context_t>::const_iterator it = func_map.begin();
        it != func_map.end();
        ++it)
    {

        DIGGI_ASSERT(it->second.output_queue);
        auto item = (msg_t *)lf_try_recieve(it->second.output_queue, 0);

        if (item != nullptr)
        {
            //telemetry_capture("Message in func func scheduler");

            auto msg = (msg_t *)item;
            DIGGI_ASSERT(msg);
            if (msg->type == DIGGI_SIGNAL_TYPE_EXIT)
            {
                telemetry_write();
                DIGGI_TRACE(proc_ctx->GetLogObject(), LRELEASE, "%s is sending Signal: Stop to the diggi runtime\n", it->second.name.c_str());
                free(item);
                func_management_context_t itm = it->second;
                func_map.erase(it->first);
                if (runtime_global_exit_when_done)
                {
                    shutdown_funcs();
                }
                else
                {
                    shutdown_func(itm);
                }
                /*
                    must break and reenumerate map
                */
                break;
            }
            else if (msg->dest.fields.proc != proc_ctx->GetId().fields.proc)
            {
                /*  
					Sending message to another diggi_base_executable
				*/
                auto arg = it->second.output_queue;
                auto ctx = new proc_outbound_ctx_t(arg, item);
                tcp_outgoing(ctx, 1);
            }

            else
            {
                /*
					Remove thread specific info to ensure correct routing via func_map
				*/
                auto dest = msg->dest;
                dest.fields.thread = 0;

                auto destination_queue = func_map[dest.raw].input_queue;
                DIGGI_ASSERT(destination_queue);

                DIGGI_TRACE(proc_ctx->GetLogObject(),
                            LogLevel::LDEBUG,
                            "Handling  message from: %" PRIu64 ", to: %" PRIu64 ", id: %lu size: %lu , type = %d \n",
                            msg->src.raw,
                            msg->dest.raw,
                            msg->id,
                            msg->size,
                            msg->type);

                lf_send(destination_queue, msg, 0);

                //telemetry_capture("Message forwarded to destination queue");

                DIGGI_TRACE(proc_ctx->GetLogObject(), LDEBUG,
                            "Successful message forwarding in message scheduler\n");
            }
        }
    }

    proc_ctx->GetThreadPool()->Schedule(message_sheduler_loop, proc_ctx, __PRETTY_FUNCTION__);
}

/**
 * @brief signal to message sheduler to stop execution
 * 
 */
void message_sheduler_stop()
{
    stop_message_loop = true;
    __sync_synchronize();
}
/**
 * @brief init funciton for bootstrapping message sheduler loop.
 * start message sheduler by requesting threadpool to shedule an invocation.
 */
void message_scheduler_start()
{
    DIGGI_ASSERT(proc_ctx);
    stop_message_loop = false;
    __sync_synchronize();

    DIGGI_TRACE(proc_ctx->GetLogObject(), LDEBUG, "Starting Message Scheduler\n");
    proc_ctx->GetThreadPool()->Schedule(message_sheduler_loop, proc_ctx, __PRETTY_FUNCTION__);
}
/**
 * @brief initalizes enclave instances according to specifications in configuration.json. 
 * each thread enters enclave through special ecall.
 * Generates unique aids for each instance.
 * Deffer intialization of diggi api, until enclave entry.
 * @param enclave_funclist list of sub-objects of JSON describing each enclave instance to initialize
 * @param map unused parameter
 * @param max_threads maximum expected threads which may concurrently consume outbound instance buffer and produce onto inbound buffer. the theoretical max should allway be count of local instances, multiplied by per instance threads and the untrusted runtime thread.
 * @return std::vector<name_service_update_t> return map of aid to human readable name
 */
std::vector<name_service_update_t> initialize_enclave_funcs(
    std::vector<json_node> enclave_funclist,
    json_node map,
    size_t max_threads)
{
    if (enclave_funclist.size() == 0)
    {
        return std::vector<name_service_update_t>();
    }
    auto map_arr = std::vector<name_service_update_t>();

    for (unsigned i = 0; i < enclave_funclist.size(); i++)
    {
        auto enclave_name = enclave_funclist[i].key.tostring();

        auto filename = (enclave_funclist[i].key.find("@") != UINT_MAX)
                            ? enclave_funclist[i].key.tostring() + ".signed.so"
                            : enclave_funclist[i].key.tostring() + ".signed.so";

        sgx_enclave_id_t id = 0;
        initialize_enclave(&id, filename.c_str());

        aid_t cli;
        cli.raw = 0;
        cli.fields.enclave = i;
        cli.fields.proc = proc_ctx->GetId().fields.proc;
        cli.fields.type = ENCLAVE;
        cli.fields.att_group = (uint8_t)atoi(enclave_funclist[i]["attestation-group"].value.tostring().c_str());

        func_map[cli.raw].id = cli;
        func_map[cli.raw].enclave = true;
        func_map[cli.raw].enc_id = id;
        func_map[cli.raw].name = enclave_funclist[i].key.tostring();
        /*TODO: support multiple threads*/
        auto threads = atoi(enclave_funclist[i]["threads"].value.tostring().c_str());
        for (unsigned th = 0; th < threads; th++)
        {
            func_map[cli.raw].enclave_thread.push_back(
                new_thread_with_affinity_enc(enclave_thread_entry, new uint64_t(id)));
        }
        func_map[cli.raw].input_queue = lf_new(RING_BUFFER_SIZE, (max_threads * threads) + 1, (max_threads * threads) + 1);
        func_map[cli.raw].output_queue = lf_new(RING_BUFFER_SIZE, (max_threads * threads) + 1, (max_threads * threads) + 1);
        func_map[cli.raw].configuration = enclave_funclist[i];
        name_service_update_t upd;
        memcpy(upd.name, enclave_name.c_str(), enclave_name.size());
        upd.name_size = enclave_name.size();
        upd.physical_address = cli;
        upd.destination_queue = func_map[cli.raw].input_queue;

        map_arr.push_back(upd);
    }
    return map_arr;
}

/**
 * @brief initialize non-enclave diggi instances. 
 * Used for system instances such as network server and storage server which interface with the host operating system.
 * @see NetworkServer::NetworkServer
 * @see StorageServer::StorageServer
 * Generate unique aid for each instance.
 * Creates threads directly interfacing with operating system.
 * Initializes diggi api and runtime for non-enclave instances, as these execute in regular process memory.
 * @param funclist list of sub-objects extracted from configuration.json for non-enclave diggi instances.
 * @param map unused parameter
 * @param max_threads maximum expected threads which may concurrently consume outbound instance buffer and produce onto inbound buffer. the theoretical max should allway be count of local instances, multiplied by per instance threads and the untrusted runtime thread.
 * @return std::vector<name_service_update_t> return map of aid to human readable name
 */
std::vector<name_service_update_t> initialize_library_funcs(
    std::vector<json_node> funclist,
    json_node map,
    size_t max_threads)
{

    auto map_arr = std::vector<name_service_update_t>();

    for (unsigned i = 0; i < funclist.size(); i++)
    {
        /*
		To create two distinct func libs, we must copy the binaries
		*/

        auto dst_filename = funclist[i].key.tostring() + ".so";

        DIGGI_TRACE(proc_ctx->GetLogObject(), LRELEASE, "Initializing lib func: %s\n", dst_filename.c_str());
        auto location = "./" + dst_filename;
        auto handle = dlopen(location.c_str(), RTLD_LOCAL | RTLD_LAZY);
        if (!handle)
        {
            DIGGI_TRACE(proc_ctx->GetLogObject(), LRELEASE, "ERROR: %s\n", dlerror());
            DIGGI_ASSERT(false);
        }
        DIGGI_ASSERT(handle);
        auto cb_s = (async_cb_t)dlsym(handle, "func_start");
        auto cb_i = (async_cb_t)dlsym(handle, "func_init_pre");
        auto cb_d = (async_cb_t)dlsym(handle, "func_stop");
        DIGGI_ASSERT(cb_s);
        DIGGI_ASSERT(cb_i);
        DIGGI_ASSERT(cb_d);

        aid_t d;
        d.raw = 0;
        d.fields.lib = i;
        d.fields.proc = proc_ctx->GetId().fields.proc;
        d.fields.type = LIB;
        d.fields.att_group = (uint8_t)atoi(funclist[i]["attestation-group"].value.tostring().c_str());

        func_map[d.raw].id = d;
        func_map[d.raw].enclave = false;
        func_map[d.raw].start_func = cb_s;
        func_map[d.raw].init_func = cb_i;
        func_map[d.raw].stop_func = cb_d;

        func_map[d.raw].name = funclist[i].key.tostring();
        auto conf = funclist[i];

        name_service_update_t upd;
        memcpy(upd.name, funclist[i].key.tostring().c_str(), funclist[i].key.tostring().size());
        upd.name_size = funclist[i].key.tostring().size();
        upd.physical_address = d;

        auto threads = atoi(conf["threads"].value.tostring().c_str());
        DIGGI_ASSERT(threads > 0);
        auto pool_singleton = new ThreadPool(threads, REGULAR_MODE, funclist[i].key.tostring());
        /*
            TODO: funcs now expect that all other funcs have the same ammount of threads.
                may not be the case in the future. 
        */
        func_map[d.raw].input_queue = lf_new(RING_BUFFER_SIZE, (max_threads * threads) + 1, (max_threads * threads) + 1);
        func_map[d.raw].output_queue = lf_new(RING_BUFFER_SIZE, (max_threads * threads) + 1, (max_threads * threads) + 1);
        upd.destination_queue = func_map[d.raw].input_queue;
        map_arr.push_back(upd);
        auto a_logger = new StdLogger(pool_singleton);
        auto loglvl = LogLevel::LRELEASE;
        if (conf.contains("LogLevel"))
        {
            loglvl = (conf["LogLevel"].value == "LDEBUG") ? LogLevel::LDEBUG : LogLevel::LRELEASE;
        }
        a_logger->SetLogLevel(loglvl);
        a_logger->SetFuncId(d, funclist[i].key.tostring());

        /*
			Context object
		*/
        DIGGI_TRACE(a_logger, LDEBUG, "Creating context object for untrusted func with configuration: %s\n", conf.debug_get_string().c_str());
        func_map[d.raw].acontext = new DiggiAPI(
            pool_singleton,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            a_logger,
            d,
            handle);
        func_map[d.raw].acontext->SetFuncConfig(conf);
        /*
			Noseal object lost according to valgrind
		*/
    }

    return map_arr;
}

/**
 * @brief convenience function to unmarshal map for (aid,human readable instance name)
 * 
 * @param mapdata array of map entries
 * @param count number of entries
 * @return std::map<string, aid_t> STL map of <diggi instance name, aid>
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
void shutdown_func_trampoline(void *ptr, int status)
{
    DIGGI_ASSERT(ptr);
    auto api = (DiggiAPI *)ptr;
    api->GetSignalHandler()->Stopfunc();
}
/**
 * @brief Starting all instances, enclave and non-enclave, previously initialized.
 * For enclave instances, this funciton enters each encalve and intializes the trusted runtime in sequence.
 * Executes on "init thread", not associated with any instance, thread dedicated to untrusted runtime.
 * For non-enclave instances message polling services are started and init,start funcitons are invoked directly
 * @param map map of human readable name to aid mapping.
 */
void start_funcs(std::vector<name_service_update_t> map)
{

    /*
	    Only start funcs once all support structures are in place
	    and once all addresses are mapped
	*/

    /*
        TODO: same as above, funcs now expect that all other funcs have the same ammount of threads.
            may not be the case in the future. 
    */

    auto threads = (func_map.begin()->second.enclave_thread.size() > 0)
                       ? (func_map.begin()->second.enclave_thread.size())
                       : atoi(func_map.begin()->second.acontext->GetFuncConfig()["threads"].value.tostring().c_str());

    global_memory_buffer = provision_memory_buffer((func_map.size() * threads) + 1, MAX_DIGGI_MEM_ITEMS, MAX_DIGGI_MEM_SIZE);

    DIGGI_TRACE(proc_ctx->GetLogObject(), LRELEASE, "done making memory, for %d threads\n", threads);
    telemetry_init();
    telemetry_start();
    size_t base_thread_id = 1;
    for (
        std::map<uint64_t, func_management_context_t>::const_iterator it = func_map.begin();
        it != func_map.end();
        it++)
    {
        auto func = it->second;
        if (!func.enclave)
        {
            bool skip_attestation = false;
            if (func.acontext->GetFuncConfig().contains("skip-attestation"))
            {
                skip_attestation = (func.acontext->GetFuncConfig()["skip-attestation"].value == "1") ? true : false;
            }
            bool trusted_root = false;
            if (func.acontext->GetFuncConfig().contains("trusted-root"))
            {
                trusted_root = (func.acontext->GetFuncConfig()["trusted-root"].value == "1") ? true : false;
            }
            bool replay_func = false;
            if (func.acontext->GetFuncConfig().contains("replay-func"))
            {
                replay_func = (func.acontext->GetFuncConfig()["replay-func"].value == "1") ? true : false;
            }
            bool record_func = false;
            if (func.acontext->GetFuncConfig().contains("record-func"))
            {
                record_func = (func.acontext->GetFuncConfig()["record-func"].value == "1") ? true : false;
            }

            bool dynamic_measurement = false;
            if (func.acontext->GetFuncConfig().contains("dynamic-measurement"))
            {
                dynamic_measurement = (func.acontext->GetFuncConfig()["dynamic-measurement"].value == "1") ? true : false;
            }

            if (skip_attestation)
            {
                DIGGI_ASSERT(!trusted_root);
            }

            IIASAPI *iasapi = (!skip_attestation)
                                  ? static_cast<IIASAPI *>(new AttestationAPI())
                                  : static_cast<IIASAPI *>(new NoAttestationAPI());
            auto dynamicmeasurement = (dynamic_measurement) ? (new DynamicEnclaveMeasurement(func.acontext)) : nullptr;
            func.acontext->SetStorageManager(new StorageManager(func.acontext, new NoSeal(!replay_func)));

            auto tmm = ThreadSafeMessageManager::Create<SecureMessageManager, AsyncMessageManager>(
                func.acontext,
                func.input_queue,
                func.output_queue,
                iasapi,
                convert_2_map_representation(map.data(), map.size()),
                map,
                base_thread_id,
                global_memory_buffer,
                trusted_root,
                dynamicmeasurement,
                record_func,
                new SGXCryptoImpl());

            DIGGI_TRACE(proc_ctx->GetLogObject(), LDEBUG, "Initializing func with id:  %" PRIu64 "\n", func.id.raw);

            func.acontext->SetMessageManager(tmm);
            ///TODO: memleak
            func.acontext->SetSignalHandler(new DiggiSignalHandler(tmm));
            //If replay, we avoid replay attack prevention through crc hashes
            if (replay_func)
            {
                DIGGI_ASSERT(!record_func);
                ///TODO: memleak
                auto storage_d_api = new DiggiAPI(func.acontext);
                storage_d_api->SetStorageManager(new StorageManager(storage_d_api, new NoSeal(false)));

                auto replm = ThreadSafeMessageManager::CreateReplay(
                    storage_d_api,
                    convert_2_map_representation(map.data(), map.size()),
                    func.id,
                    shutdown_func_trampoline,
                    storage_d_api);
                func.acontext->SetMessageManager(replm);
                func.acontext->SetSignalHandler(new DiggiSignalHandler(replm));
            }

            func.acontext->GetThreadPool()->ScheduleOn(
                0,
                func.init_func,
                func.acontext,
                __PRETTY_FUNCTION__);
            func.acontext->GetThreadPool()->ScheduleOn(
                0,
                func.start_func,
                func.acontext,
                __PRETTY_FUNCTION__);
            base_thread_id += (size_t)atoi(func.acontext->GetFuncConfig()["threads"].value.tostring().c_str());
        }
        else
        {
            auto stat = ecall_enclave_entry_client(
                func.enc_id,
                func.id,
                func.enc_id,
                base_thread_id,
                global_memory_buffer,
                func.input_queue,
                func.output_queue,
                map.data(),
                map.size(),
                func.name.c_str(),
                func.enclave_thread.size());
            DIGGI_ASSERT(stat == SGX_SUCCESS);
            base_thread_id += func.enclave_thread.size();
        }
    }
}

/**
 * @brief invoked in the event of a configuration update submitted to the untrusted runtime.
 * Invoked during diggi host startup to setup execution.
 * also supports reconfiguration via controll plane functions (http request)
 * @param str mutable buffer representation of configuration JSON object.
 */
void configuration_update(zcstring str)
{
    DIGGI_ASSERT(!str.empty());
    json_document conf(str);
    DIGGI_TRACE(proc_ctx->GetLogObject(), LDEBUG, "Configuration update:\n%s\n", str.tostring().c_str());
    auto procconf = conf.head.children[4].children;
    std::vector<name_service_update_t> nameservicemap;

    for (int i = 0; i < procconf.size(); i++)
    {
        /*
			Populate network map
		*/

        auto term = procconf[i]["network"].value;
        auto separatorindex = term.find(":");
        auto port = term.substr(separatorindex + 1);
        auto ip = term.substr(0, separatorindex);
        DIGGI_TRACE(proc_ctx->GetLogObject(), LRELEASE,
                    "Process with id:%d, Added ip=%s,port=%s, proc=%d to network map\n",
                    (int)proc_ctx->GetId().fields.proc,
                    ip.tostring().c_str(),
                    port.tostring().c_str(),
                    i + 1);
        /*
			May allready be populated if func func recieves message before provisioning is done
		*/
        if ((*network_map)[(uint8_t)(i + 1)] == nullptr)
        {
            (*network_map)[(uint8_t)(i + 1)] = new networkinfo(port, ip);
        }

        if (i + 1 == (int)proc_ctx->GetId().fields.proc)
        {
            /* 
				Process + postfix identifier
			*/

            auto loglvl = LogLevel::LRELEASE;
            if (procconf[i].contains("LogLevel"))
            {
                loglvl = (procconf[i]["LogLevel"].value == "LDEBUG") ? LogLevel::LDEBUG : LogLevel::LRELEASE;
            }
            proc_ctx->GetLogObject()->SetLogLevel(loglvl);
            auto funclist = procconf[i]["funcs"].children;
            auto enclave_funclist = procconf[i]["enclave"]["funcs"].children;
            auto map_list = conf.head["map"];
            /*
			TODO: implement propper map
			*/
            auto nameservicemap_enc = initialize_enclave_funcs(enclave_funclist, map_list, enclave_funclist.size() + funclist.size());
            nameservicemap.insert(
                nameservicemap.end(),
                nameservicemap_enc.begin(),
                nameservicemap_enc.end());

            auto nameservicemap_lib = initialize_library_funcs(funclist, map_list, enclave_funclist.size() + funclist.size());
            nameservicemap.insert(
                nameservicemap.end(),
                nameservicemap_lib.begin(),
                nameservicemap_lib.end());
        }
        else
        {
            auto funclist = procconf[i].children[1].children;
            auto enclave_funclist = procconf[i].children[2].children[0].children;

            for (unsigned x = 0; x < funclist.size(); x++)
            {
                aid_t d;
                d.raw = 0;
                d.fields.lib = x;
                d.fields.proc = i + 1;
                d.fields.type = LIB;
                name_service_update_t upd;
                memcpy(upd.name, funclist[x].key.tostring().c_str(), funclist[x].key.tostring().size());
                upd.name_size = funclist[x].key.tostring().size();
                upd.physical_address = d;
                upd.destination_queue = nullptr;
                nameservicemap.push_back(upd);
            }

            for (unsigned x = 0; x < enclave_funclist.size(); x++)
            {
                aid_t cli;
                cli.raw = 0;
                /*
					May require explicit mapping because enclave identifiers are not provisoned monotonically increasing.
				*/
                cli.fields.enclave = x;

                cli.fields.proc = i + 1;
                cli.fields.type = ENCLAVE;

                name_service_update_t upd;
                memcpy(
                    upd.name,
                    enclave_funclist[x].key.tostring().c_str(),
                    enclave_funclist[x].key.tostring().size());
                upd.destination_queue = nullptr;
                upd.name_size = enclave_funclist[x].key.tostring().size();
                upd.physical_address = cli;
                nameservicemap.push_back(upd);
            }
        }
    }
    start_funcs(nameservicemap);
    message_scheduler_start();
}

/**
 * @brief shut down a particular func.
 * signal stop to the instance in question and deallocate resources associated with it. 
 * If enclave instance, operation will block until enclave voulentarily releases threading resource.
 * Also checks if the runtime_global_exit_when_done flag is set, if so and all other instances are shut down, exit(1) host process.
 * @param ctx context object for instance being shut down.
 */
void shutdown_func(func_management_context_t ctx)
{
    if (ctx.id.fields.type == LIB)
    {

        auto func = ctx.acontext;
        DIGGI_ASSERT(func);

        auto hndl = func->GetDlHandle();
        DIGGI_ASSERT(hndl);
        auto cb_s = (async_cb_t)dlsym(hndl, "func_stop");
        cb_s(func, 1);

        func->GetThreadPool()->Stop();
        dlclose(hndl);
        auto lg = (StdLogger *)func->GetLogObject();
        auto gmm = func->GetMessageManager();

        delete gmm;

        auto tp = (ThreadPool *)func->GetThreadPool();
        delete tp;
        delete lg;
        delete func;
        lf_destroy(ctx.input_queue);
        lf_destroy(ctx.output_queue);
    }
    else
    {
        DIGGI_ASSERT(ctx.id.fields.type == ENCLAVE);

        auto stat = ecall_enclave_stop(ctx.enc_id);
        DIGGI_ASSERT(stat == SGX_SUCCESS);
        /*
            stop should cause thread to jump out of scheduler loop
            and we wait for it to ensure that enclave teardown
            does not deadlock
        */
        for (auto th : ctx.enclave_thread)
        {
            th->join();
            delete th;
        }
        sgx_destroy_enclave(ctx.enc_id);
        lf_destroy(ctx.input_queue);
        lf_destroy(ctx.output_queue);

        /*
            We assume that destroying the enclave
            deallocates all associated memory
        */
    }
    if (runtime_global_exit_when_done && (func_map.size() == 0))
    {
        DIGGI_TRACE(proc_ctx->GetLogObject(), LRELEASE, "Last func exited, shutting down runtime\n");
        stop_message_loop = true;
        proc_ctx->GetThreadPool()->Stop();
        lf_destroy(global_memory_buffer);
        global_memory_buffer = nullptr;
        telemetry_write();
        // runtime_stop(nullptr, 1);
    }
}
/**
 * @brief Shut down all funcs managed by this runtime.
 * Calls shutdown_func() for each, if last and runtime_global_exit_when_done is set. will cause host process to exit.
 * Desroys global message object memory buffer after all instances are shutdown. 
 * Important to wait until instances are shut down to avoid message allocation after free.
 */
void shutdown_funcs()
{
    message_sheduler_stop();
    if (runtime_global_exit_when_done)
    {
        proc_ctx->GetThreadPool()->Stop();
    }

    for (
        std::map<uint64_t, func_management_context_t>::const_iterator it = func_map.begin();
        it != func_map.end();
        ++it)
    {
        shutdown_func(it->second);
    }
    reset_affinity();
    func_map.clear();
    telemetry_write();
    if (global_memory_buffer)
    {
        lf_destroy(global_memory_buffer);
    }
    /*Deletes all all ringbuffers*/
}

/**
 * @brief Initital entry point for runtime, called from main.cpp
 * diggi api availible for the untrusted runtime is initialized in main method and passed as argument to this function.
 * Executes on init thread/untrusted runtime dedicated thread.
 * 
 * @param ctx 
 * @param status 
 */
void runtime_init(void *ctx, int status)
{

    DIGGI_ASSERT(proc_ctx == nullptr);
    DIGGI_ASSERT(ctx);
    proc_ctx = (DiggiAPI *)ctx;
    DIGGI_TRACE(proc_ctx->GetLogObject(), LDEBUG, "Entering func func init\n");
}
/**
 * @brief invoked once untrusted runtime api is setup from main.cpp
 * Executes on init thread/untrusted runtime dedicated thread once runtime_init() call is done.

 * Invokes configuration_update() which iniitializes instances according to configuration JSON object.
 * main.cpp reads configuration from disk and passes it into this function.
 * 
 * @param com pointer to buffer containing configuration
 * @param status unused parameter for error handling(future work)
 * @param exit_when_done if parameter --exit_when_done is passed to process start, exit when all instances shut down.
 */
void runtime_start(void *com, int status, bool exit_when_done)
{

    runtime_global_exit_when_done = exit_when_done;
    DIGGI_ASSERT(com);
    DIGGI_ASSERT(proc_ctx);

    DIGGI_TRACE(proc_ctx->GetLogObject(), LRELEASE, "Entering func func start \n");

    auto config = (zcstring *)com;
    network_map = new std::map<uint8_t, networkinfo *>();

    /*need context object*/
    if (config->size() > 0)
    {
        configuration_update(*config);
    }

    delete config;
    return;
}
/**
 * @brief invoked on recive event from network
 * Prepares incomming network message and hands of messsage to correct diggi instance input queue
 * @param ptr pointer to mutable buffer directly from network
 * @param status unused parameter for error handling (future work)
 */
void runtime_recieve(void *ptr, int status)
{

    /*todo: delete zcstring*/
    DIGGI_ASSERT(ptr);
    auto messagebuff = (zcstring *)ptr;
    DIGGI_ASSERT(messagebuff->getmbuf()->head->size >= sizeof(msg_t));
    auto dest = ((msg_t *)messagebuff->getmbuf()->head->data)->dest;

    /*
		Remove thread specific addressing
	*/
    dest.fields.thread = 0;
    /*
		If messages are inbound before funcs are ready to recieve, 
		the message delivery is retried
	*/
    if (func_map[dest.raw].input_queue == nullptr)
    {
        proc_ctx->GetThreadPool()->Schedule(runtime_recieve, ptr, __PRETTY_FUNCTION__);
        return;
    }
    /*Nested message for forwarding*/
    DIGGI_ASSERT(global_memory_buffer != nullptr);
    DIGGI_ASSERT(messagebuff->size() < MAX_DIGGI_MEM_SIZE);

    msg_t *rb_msg = (msg_t *)lf_recieve(global_memory_buffer, 0);

    DIGGI_TRACE(proc_ctx->GetLogObject(), LDEBUG, "Message in func func recieve to %d\n", dest.raw);

    if (rb_msg == nullptr)
    {
        DIGGI_TRACE(proc_ctx->GetLogObject(),
                    LDEBUG,
                    "Recieve buffer contention in runtime_recieve, deffering message delivery\n");
        /*
		Queue is full, so we wait for availible space and defer message delivery
		NB:Will cause Out-Off-Order message delivery!
		(Can be fixed by defering all messages to that input queue)
		*/

        proc_ctx->GetThreadPool()->Schedule(runtime_recieve, ptr, __PRETTY_FUNCTION__);
    }
    else
    {
        //telemetry_capture("recieved network message in func func");

        DIGGI_TRACE(proc_ctx->GetLogObject(),
                    LogLevel::LDEBUG,
                    "Successful message reservation in runtime_recieve \n");
        auto msg_sender = (msg_t *)(rb_msg);

        /*
		TODO: avoid copying data
		*/
        auto retstring = messagebuff->tostring();

        memcpy(msg_sender, retstring.c_str(), retstring.size());

        lf_send(func_map[dest.raw].input_queue, rb_msg, 0);
        //telemetry_capture("network message forwarded tot target func func");
        auto obj = (zcstring *)ptr;
        delete obj;
        DIGGI_TRACE(proc_ctx->GetLogObject(),
                    LogLevel::LDEBUG,
                    "Successful message forwarding in runtime_recieve\n");
    }
}
/**
 * @brief cleanup runtime allocations, prior to process exit(1)
 * Closes all open connections, deallocates network info, and shuts down instances which are still running.
 * 
 * @param ctx unused parameter
 * @param status unused parameter
 */
void runtime_stop(void *ctx, int status)
{
    /*
	Close connections keept alive
	*/
    DIGGI_TRACE(proc_ctx->GetLogObject(),
                LogLevel::LRELEASE,
                "Shuting down connections\n");
    for (auto val : *network_map)
    {

        if (val.second != nullptr)
        {
            if (val.second->con != nullptr)
            {
                val.second->con->close();
                delete val.second->con;
                auto icpr = (SimpleTCP *)val.second->proto;
                delete icpr;
            }
            delete val.second;
        }
    }
    /*
		handled by standard allocator
	*/
    delete network_map;
    shutdown_funcs();
}

/**
 * @brief prepares and begins outgoing message transmission.
 * Invoked by message_sheduler_loop when the aid field indicates the destination is a remote diggi host.
 * Current networking protocol uses TCP for reliable delivery. TCP handshake occurs laizily upon first message transmission.
 * If connection fails, tcp_outgoing() reschedules itself onto the threadpool to retry operation, allowing other requests in the interim.
 * once operation is completed tcp_outgoing_cb() is invoked.
 * @param ptr outbound context object storing message object and outbound queue for cleanup post-transmission
 * @param status unused parameter errorhandling (future work)
 */
void tcp_outgoing(void *ptr, int status)
{
    //telemetry_capture("proc outgoing recieved queue message");
    DIGGI_ASSERT(ptr);

    auto ctx = (proc_outbound_ctx_t *)ptr;
    auto msg = (msg_t *)(ctx->item2);

    DIGGI_ASSERT(msg);

    auto networkinfor = (*network_map)[msg->dest.fields.proc];
    DIGGI_ASSERT(networkinfor != nullptr);

    /*
	Only setup new connection if one does not allready exist to the given destination
	*/
    int failconnect = 0;
    if (networkinfor->con == nullptr)
    {
        networkinfor->proto = new SimpleTCP();
        networkinfor->con = new Connection(proc_ctx->GetThreadPool(),
                                           nullptr,
                                           networkinfor->proto,
                                           proc_ctx->GetLogObject());

        /*Dataport is one-of the controll port*/
        auto dataport = atoi(networkinfor->port.tostring().c_str()) + 1;

        DIGGI_TRACE(proc_ctx->GetLogObject(),
                    "Outgoing tcp connection to proc:%u with ip:%s and port: %d\n",
                    msg->dest.fields.proc,
                    networkinfor->ip.tostring().c_str(),
                    dataport);

        failconnect = networkinfor->con->initialize("", networkinfor->ip.tostring(), Utils::itoa(dataport));

        if (!failconnect)
        {
            /*
				Connection has been established and now this tcp socket can be read for incomming messages
			*/
            proc_ctx->GetThreadPool()->Schedule(tcp_incomming, networkinfor->con, __PRETTY_FUNCTION__);
        }
    }

    if (failconnect)
    {
        /*
			Retry connection
		*/
        delete networkinfor->con;
        networkinfor->con = nullptr;
        proc_ctx->GetThreadPool()->Schedule(tcp_outgoing, ctx, __PRETTY_FUNCTION__);
    }
    else
    {
        DIGGI_TRACE(proc_ctx->GetLogObject(), LDEBUG, "Outgoing tcp message to proc:%u \n", msg->dest.fields.proc);
        networkinfor->con->write(zcstring((char *)msg, msg->size), tcp_outgoing_cb, ctx);
    }
}
/**
 * @brief Invoked once outbound message is completely written to socket internal buffer.
 * Because outbound message allocated on the global_memory_buffer is not copied after the initial allocation within the diggi instance, 
 * rather the pointers are sent through the outbound message queue to the untrusted runtime,
 * we ensure that the message is transmitted before deallocating it.
 * Also deallocate context object proc_outbound_ctx_t
 * @param ptr proc_outbound_ctx_t context object holding messageobject
 * @param status unused parameter error handling (future work)
 */
void tcp_outgoing_cb(void *ptr, int status)
{
    /*
		Only free ringbuffer item when tcp write is done
	*/
    DIGGI_TRACE(proc_ctx->GetLogObject(), LDEBUG, "tcp_outgoing_cb:cleaning up send\n");

    DIGGI_ASSERT(ptr);
    auto ctx = (proc_outbound_ctx_t *)ptr;
    DIGGI_ASSERT(global_memory_buffer != nullptr);

    lf_send(global_memory_buffer, ctx->item2, 0);
    delete ctx;
}

/**
 * @brief invoked in asynchronous callback loop to check for new inbound messages on TCP buffer.
 * loop chain goes: tcp_incomming()->tcp_incomming_read_header_cb()->tcp_incomming_read_body_cb()->tcp_incomming()
 * paralell reads from other sockets may be intermixed between each element in callback chain.
 * Allow non-blocking high utilization of single runtime thread so that more may execute diggi instances.
 * calls a non blocking read operation on Connection object for header-info of inbound message over TCP, which trigger a callback upon successs.
 * @param ptr Connection object used for asynchronous read.
 * @param status 
 */
void tcp_incomming(void *ptr, int status)
{
    DIGGI_ASSERT(ptr);
    auto con = (Connection *)ptr;
    con->read(sizeof(msg_t), tcp_incomming_read_header_cb, nullptr);
}
/**
 * @brief callback on succesfull header read operation.
 * Uses header info to invoke read of body into message object directly relayable without copy operations to destination diggi instance.
 * For partial reads, this funciton is reinvoked until satisfied.
 * @param ptr  read context object storing read state, connection object, and destination buffer.
 * @param status unused parameter error handling (future work)
 */
void tcp_incomming_read_header_cb(void *ptr, int status)
{
    //telemetry_capture("recieved header");
    DIGGI_ASSERT(ptr);
    auto ctx = (connection_read_ctx_t *)ptr;
    DIGGI_ASSERT(ctx->item1);
    auto mbuf = ctx->item2.getmbuf();
    DIGGI_ASSERT(ctx->item2.size() == sizeof(msg_t) && mbuf->head->size == sizeof(msg_t));

    /*
	assume that one of the funcs is the client and one is the server
	*/

    auto proc = ((msg_t *)mbuf->head->data)->src.fields.proc;
    DIGGI_TRACE(proc_ctx->GetLogObject(), LDEBUG, "new incomming tcp message from proc:%u\n", proc);
    DIGGI_ASSERT(proc > 0);

    /*
		If inbound connection happens before configchange is able to provision funcs
	*/
    auto networkinfor = (*network_map)[proc];
    if ((*network_map)[proc] == nullptr)
    {
        (*network_map)[proc] = new networkinfo();
    }
    (*network_map)[proc]->con = ctx->item1;

    auto message_body_size = ((msg_t *)mbuf->head->data)->size - sizeof(msg_t);
    if (message_body_size > 0)
    {
        ctx->item1->read(message_body_size, tcp_incomming_read_body_cb, new zcstring(ctx->item2));
    }
    else
    {
        /*no body recieved*/
        DIGGI_TRACE(proc_ctx->GetLogObject(),
                    LogLevel::LDEBUG,
                    "proc_incomming_tcp_read_header_cb: message without body recieved\n");
        runtime_recieve(new zcstring(ctx->item2), 1);

        /*
			Done with message, continue reading from socket, as new messages may be relayed
		*/
        proc_ctx->GetThreadPool()->Schedule(tcp_incomming, ctx->item1, __PRETTY_FUNCTION__);
    }
}

/**
 * @brief invoked after a successful read of message body from TCP input buffer.
 * Pases completed inbound message to runtime for scheduling onto correct inbound queue.
 * @param ptr  read context object holding connection object, inbound buffer and read state.s
 * @param status status flag, unused, future work
 */
void tcp_incomming_read_body_cb(void *ptr, int status)
{
    //telemetry_capture("recieved body");

    DIGGI_ASSERT(ptr);
    auto ctx = (connection_read_ctx_t *)ptr;
    DIGGI_ASSERT(ctx->item5);
    auto message_buf = (zcstring *)ctx->item5;
    DIGGI_ASSERT(ctx->item1);
    message_buf->append(ctx->item2);
    runtime_recieve(message_buf, 1);

    /*
	Done with message, continue reading from socket, as new messages may be relayed
	*/
    proc_ctx->GetThreadPool()->Schedule(tcp_incomming, ctx->item1, __PRETTY_FUNCTION__);
}

/**
 * @brief Incomming http configuration change 
 * Control plane configuration change invoked by diggi deployment infrastructure.
 * Translate connection object to httprequest object request.
 * @param ptr connection object
 * @param status status flag, unused parameter, future work.
 */
void http_incomming(void *ptr, int status)
{
    DIGGI_ASSERT(ptr);

    auto con = (Connection *)ptr;
    auto re = new http_request(con, proc_ctx->GetLogObject());

    DIGGI_TRACE(proc_ctx->GetLogObject(), LDEBUG, "Incomming http connection\n");

    re->Get(http_incomming_cb, re);
}
/**
 * @brief Callback invoked once httprequest is loaded.
 * invokes shutdown on all diggi instances and issues a configuration updatate based on the recieved configuration JSON.
 * The Configuration is delivered as part of the http body.
 * Once completed, the http-handler responds with an 200 OK.
 * @param ptr http response object.
 * @param status status flag, unused parameter, future work
 */
void http_incomming_cb(void *ptr, int status)
{

    DIGGI_ASSERT(ptr);
    auto re = (http_response *)ptr;

    /*
	TODO: prefixed by whitespace, must be fixed in http impl
	*/
    if (re->headers["diggi-message-type"].compare(" configuration"))
    {
        DIGGI_TRACE(proc_ctx->GetLogObject(), LRELEASE, "Reconfiguration: Shutting down old funcs.....\n");
        shutdown_funcs();

        DIGGI_TRACE(proc_ctx->GetLogObject(), LRELEASE, "Reconfiguration: Deploying new funcs.....\n");
        configuration_update(re->body);
    }
    else
    {
        DIGGI_ASSERT(false); //No other control messages implemented yet
    }

    http_respond_ok(re->GetConnection());
    delete re;
}
/**
 * @brief crafts and sends a http 200 response to configuration request initiator.
 * Called after configuration update completes.
 * @param connection connection object used to transmitt over TCP.
 */
void http_respond_ok(IConnection *connection)
{

    ALIGNED_CONST_CHAR(8)
    http[] = "HTTP/1.1";
    zcstring http_str(http);
    ALIGNED_CONST_CHAR(8)
    contenttype[] = "text/html";
    zcstring cont_string(contenttype);

    ALIGNED_CONST_CHAR(8)
    body[] = "<t1>Message Recieved</t1>";
    zcstring body_str(body);

    ALIGNED_CONST_CHAR(8)
    type[] = "OK";
    zcstring type_str(type);
    ALIGNED_CONST_CHAR(8)
    twohundred[] = "200";
    zcstring twohundred_str(twohundred);

    auto re = new http_response(connection, proc_ctx->GetLogObject());

    re->set_response_type(type_str);
    re->set_status_code(twohundred_str);
    re->set_protocol_version(http_str);
    re->set_header("Content-Type", cont_string);
    re->body = body_str;
    re->Send(http_respond_ok_done_cb, re);
}

/**
 * @brief after response is successfully transmitted across TCP this callback is invoked for cleanup
 * Closes connection and deletes leftover datastructures.
 * @param ptr http response object
 * @param status status flag, unused parameter, future work
 */
void http_respond_ok_done_cb(void *ptr, int status)
{

    DIGGI_ASSERT(ptr);
    auto resp = (http_response *)ptr;
    DIGGI_TRACE(proc_ctx->GetLogObject(), LDEBUG, "respond_ok_done_cb: done sending HTTP OK\n");

    auto con = resp->GetConnection();
    con->close();
    auto icon = (Connection *)con;
    delete icon;
    delete resp;
}
#endif
