#ifndef ITHREADPOOL_H
#define ITHREADPOOL_H

#include "DiggiAssert.h"
#include "datatypes.h"
#include "posix/stdio_stubs.h"

#define THREAD_POOL_SIZE 4096 * 2 * 2


class IThreadPool {

public:
	IThreadPool() {}
	virtual void Yield() = 0;
	virtual int currentThreadId() = 0;
	virtual size_t currentVThreadId() = 0;
	virtual void Schedule(async_cb_t cb, void *args, const char *label) = 0;
	virtual void ScheduleOn(size_t id, async_cb_t cb, void *args, const char *label) = 0;
	virtual size_t physicalThreadCount() = 0;
	virtual void Stop() = 0;
    virtual bool Alive() = 0;
	virtual ~IThreadPool() {};
};

#endif