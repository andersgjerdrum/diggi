#ifndef funcCONTEXT_H
#define funcCONTEXT_H

#include "threading/IThreadPool.h"
#include "messaging/IMessageManager.h"
#include "Logging.h"
#include "messaging/IAsyncMessageManager.h"
#include "storage/IStorageManager.h"
#include "network/INetworkManager.h"
#include "JSONParser.h"
#include "SignalHandler.h"
#include "datatypes.h"

class IDiggiAPI {
public:
	IDiggiAPI() {}
	virtual ~IDiggiAPI() {}
	virtual IThreadPool* GetThreadPool() = 0;
	virtual IMessageManager * GetMessageManager() = 0;
	virtual ILog * GetLogObject() = 0;
	virtual aid_t GetId() = 0;
	virtual IStorageManager * GetStorageManager() = 0;
	virtual json_node& GetFuncConfig() = 0;
	virtual void SetFuncConfig(json_node conf) = 0;  
	virtual void SetThreadPool(IThreadPool * pool) = 0;
	virtual void SetMessageManager(IMessageManager *mngr) = 0;
	virtual void SetLogObject(ILog *lg) = 0;
	virtual void SetId(aid_t id) = 0;
    virtual ISignalHandler*  GetSignalHandler() = 0;
/// only usable inside enclave.
#ifdef DIGGI_ENCLAVE
    virtual sgx_enclave_id_t GetEnclaveId() = 0;
    virtual void SetEnclaveId(sgx_enclave_id_t id) = 0;

#endif
	virtual INetworkManager * GetNetworkManager() = 0;
};

class DiggiAPI : public IDiggiAPI {
	IThreadPool *tpool;
	IStorageManager *stomanager;
	IMessageManager *msgmanager;
	INetworkManager *netmanager;
    ISignalHandler * shandler;
	ILog *logr;
	aid_t aid;
#ifdef DIGGI_ENCLAVE
    sgx_enclave_id_t enclaveid;
#endif
	void *dl_handle;
	json_node configuration;
    
public:
	DiggiAPI(
        IThreadPool *pool, 
        IStorageManager *smngr,
        IMessageManager *mmngr,
        INetworkManager *nmngr, 
        ISignalHandler  *sighandler,
        ILog *log, 
        aid_t id,
        void* handle);
    DiggiAPI();
    DiggiAPI(IDiggiAPI * dapi);
	// getters	
	IThreadPool* GetThreadPool();
	IStorageManager* GetStorageManager();
	IMessageManager* GetMessageManager();
	INetworkManager* GetNetworkManager();

#ifdef DIGGI_ENCLAVE
    sgx_enclave_id_t GetEnclaveId();
    void SetEnclaveId(sgx_enclave_id_t id);
#endif
    ISignalHandler*  GetSignalHandler();
	void * GetDlHandle();
	ILog * GetLogObject();
	aid_t GetId();
	json_node& GetFuncConfig();
	void SetFuncConfig(json_node conf);  
	void SetThreadPool(IThreadPool * pool);
	void SetStorageManager(IStorageManager * store);
    void SetSignalHandler(ISignalHandler* handler);
	void SetMessageManager(IMessageManager *mngr);
	void SetNetworkManager(INetworkManager *netmanager);
	void SetLogObject(ILog *lg);
	void SetId(aid_t id);


};
#endif