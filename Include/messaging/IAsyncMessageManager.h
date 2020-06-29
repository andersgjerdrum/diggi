#ifndef IASYNC_MESSAGE_MANAGER
#define IASYNC_MESSAGE_MANAGER

#include "datatypes.h"

class IAsyncMessageManager
{
public:
	IAsyncMessageManager() {}
	virtual	~IAsyncMessageManager() {}

	virtual void registerTypeCallback(async_cb_t cb, msg_type_t ty, void *arg) = 0;
	virtual void  UnregisterTypeCallback(msg_type_t ty) = 0;

	virtual unsigned long getMessageId(unsigned long func_identifier) = 0;

	/*
	different thread
	*/
	virtual msg_t *allocateMessage(msg_t *msg, size_t payload_size) = 0;

	virtual msg_t *allocateMessage(aid_t source, aid_t dest, size_t payload_size, msg_convention_t async) = 0;

	virtual void Stop() = 0;
	virtual void Start() = 0;


	/*await response*/
	virtual void sendMessageAsync(msg_t *msg, async_cb_t cb, void *ptr) = 0;

	virtual void endAsync(msg_t *msg) = 0;

	/*one way*/
	virtual void sendMessage(msg_t *msg) = 0;

};

#endif
