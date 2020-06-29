#ifndef ITHREADSAFEMM_H
#define ITHREADSAFEMM_H
#include "messaging/IAsyncMessageManager.h"
class AsyncMessageManager;

class IThreadSafeMM {
public:

	IThreadSafeMM() {}
	virtual ~IThreadSafeMM() {};
	virtual AsyncMessageManager *getAsyncMessageManager() = 0;
	virtual IAsyncMessageManager *getIAsyncMessageManager() = 0;
};

#endif