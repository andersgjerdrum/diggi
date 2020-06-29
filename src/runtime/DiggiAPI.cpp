
/**
 * @file DiggiAPI.cpp
 * @author Anders Gjerdrum (anders.t.gjerdrum@uit.no)
 * @brief implements an api interface for all system funcitonality availible to diggi instances.
 * @version 0.1
 * @date 2020-01-29
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#include "runtime/DiggiAPI.h"

/**
 * @brief Construct a new Func Context:: Func Context object
 * 
 * Construct a new api object capturing the different interfaces availible to diggi developers.
 * includes Logging, thread management, storage, signalhandling, identity, configuration, networking, and message passing.
 * exclusively for use within diggi instances.
 * @param pool theadpool interface reference
 * @param smngr storagemanager interface reference
 * @param mmngr securemessagemanager interface reference
 * @param nmngr networkmanager interface reference
 * @param sighandler signalhandler interface reference
 * @param log logging interface reference
 * @param id indenty handle
 * @param handle dynamic loadable library handle.
 */
DiggiAPI::DiggiAPI(
    IThreadPool *pool,
    IStorageManager *smngr,
    IMessageManager *mmngr,
    INetworkManager *nmngr,
    ISignalHandler *sighandler,
    ILog *log,
    aid_t id,
    void *handle) : tpool(pool),
                    stomanager(smngr),
                    msgmanager(mmngr),
                    netmanager(nmngr),
                    shandler(sighandler),
                    logr(log),
                    aid(id),
                    dl_handle(handle)
{
}
DiggiAPI::DiggiAPI()
{
}
DiggiAPI::DiggiAPI(IDiggiAPI *dapi):tpool(dapi->GetThreadPool()),
                    stomanager(dapi->GetStorageManager()),
                    msgmanager(dapi->GetMessageManager()),
                    netmanager(dapi->GetNetworkManager()),
                    shandler(dapi->GetSignalHandler()),
                    logr(dapi->GetLogObject()),
                    aid(dapi->GetId()),
                    dl_handle(nullptr)
{

}
#ifdef DIGGI_ENCLAVE
/**
 * @brief Get the Enclave Id
 * 
 * @return sgx_enclave_id_t 
 */
sgx_enclave_id_t DiggiAPI::GetEnclaveId()
{
    return enclaveid;
}
/**
 * @brief Set the Enclave Id
 * 
 * @param identity 
 */
void DiggiAPI::SetEnclaveId(sgx_enclave_id_t identity)
{
    enclaveid = identity;
}
#endif
/**
 * @brief get threadpool reference
 * 
 * @return IThreadPool* 
 */
IThreadPool *DiggiAPI::GetThreadPool()
{
    return tpool;
}
/**
 * @brief get storagemanager reference
 * 
 * @return IStorageManager* 
 */
IStorageManager *DiggiAPI::GetStorageManager()
{
    return stomanager;
}
/**
 * @brief get messagemanager reference
 * 
 * @return IMessageManager* 
 */
IMessageManager *DiggiAPI::GetMessageManager()
{
    return msgmanager;
}
/**
 * @brief get networkmanager reference
 * 
 * @return INetworkManager* 
 */
INetworkManager *DiggiAPI::GetNetworkManager()
{
    return netmanager;
}
/**
 * @brief get signal handler
 * 
 * @return ISignalHandler* 
 */
ISignalHandler *DiggiAPI::GetSignalHandler()
{
    return shandler;
}
/**
 * @brief get logging object
 * 
 * @return ILog* 
 */
ILog *DiggiAPI::GetLogObject()
{
    return logr;
}
/**
 * @brief get instance identifier
 * 
 * @return aid_t 
 */
aid_t DiggiAPI::GetId()
{
    return aid;
}
/**
 * @brief get dll handle reference
 * 
 * @return void* 
 */
void *DiggiAPI::GetDlHandle()
{
    return dl_handle;
}

/**
 * @brief get func configuration as JSON object reference. Specified by developer at compile time.
 * Embedded in trusted binary, to contribute to measurement. 
 * 
 * @return json_node& 
 */
json_node &DiggiAPI::GetFuncConfig()
{
    return configuration;
}
/**
 * @brief set func configuration, not for use by developers. Setting config after init may lead to undefined behaviour
 * 
 * @param conf 
 */
void DiggiAPI::SetFuncConfig(json_node conf)
{
    configuration = conf;
}
/**
 * @brief set threadpool, not for use by developers. setting config after init may lead to undefined behaviour
 * 
 * @param pool 
 */
void DiggiAPI::SetThreadPool(IThreadPool *pool)
{
    tpool = pool;
}
/**
 * @brief set storagemanager, not for use by developers. setting config after iit may lead to undefined behaviour
 * 
 * @param store 
 */
void DiggiAPI::SetStorageManager(IStorageManager *store)
{
    stomanager = store;
}
/**
 * @brief set message manager, not for use by developers. setting config after iit may lead to undefined behaviour
 * 
 * @param mngr 
 */
void DiggiAPI::SetMessageManager(IMessageManager *mngr)
{
    msgmanager = mngr;
}
/**
 * @brief set signal handler, not for use by developers. setting config after iit may lead to undefined behaviour
 * 
 * @param handler 
 */
void DiggiAPI::SetSignalHandler(ISignalHandler *handler)
{
    shandler = handler;
}
/**
 * @brief set network manager, not for use by developers. setting config after iit may lead to undefined behaviour
 * 
 * @param nmngr 
 */
void DiggiAPI::SetNetworkManager(INetworkManager *nmngr)
{
    netmanager = nmngr;
}
/**
 * @brief set logobject, not for use by developers. setting config after iit may lead to undefined behaviour
 * 
 * @param lg 
 */
void DiggiAPI::SetLogObject(ILog *lg)
{
    logr = lg;
}
/**
 * @brief set instance identity, not for use by developers. setting config after iit may lead to undefined behaviour
 * 
 * @param id 
 */
void DiggiAPI::SetId(aid_t id)
{
    aid = id;
}
