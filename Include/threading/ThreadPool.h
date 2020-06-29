#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <vector>
#include <thread>
#include <atomic>
#include "DiggiAssert.h"
#include "misc.h"
#include <pthread.h>

#include "threading/IThreadPool.h"
#include "datatypes.h"
#include "Logging.h"
#include "threading/affinity.h"
#include <unistd.h>
#include "lockfree_rb_q.h"
#include "threading/setjmp.h"

using namespace std;




/**
 * @brief representation of virtual thread execution
 * Includes stack pointer/instruction pointer and other registry state.
 * 
 */
typedef struct jmp_buf_internal_t{
	jmp_buf_d inner ____cacheline_aligned;
}jmp_buf_internal_t;
///mode of threading, parameter used in threadpool creation @see ThreadPool::ThreadPool
typedef enum threading_mode_t
{
	ENCLAVE_MODE = 0,
	REGULAR_MODE
} threading_mode_t;

///faking STL threads for Constructor of threadpool for enclave instances, not invoked
#ifdef DIGGI_ENCLAVE
/*
	fake thread def
*/
namespace std
{
class thread
{
  public:
	thread()
	{
	}
	void join()
	{
	}
};
}; // namespace std
#endif

/**
 * @brief Class definition for threadpool object, implements interface IThreadPool.
 * IThreadPool is the api exposed to diggi instances for threading/ scheduling callbacks.
 * Used to implement pthread support inside diggi enclave instances.
 * @see posix/PthreadStubs.cpp
 */
class ThreadPool : public IThreadPool
{

  public:
    ///array of arrays of jmp_buf execution storage. indexed on array[physical thread][virtual thread]
 	std::vector<std::vector<jmp_buf_internal_t>> coroutine_bufs;
    ///quit flag used to checki if all virtual threads are done executing
	volatile size_t quit;
    //stop flag used to signal to physical threads to exit scheduler loop
	volatile size_t stop;
    ///target request FIFO queues used for scheduling execution callbacks onto threadpool
	std::vector<lf_buffer_t *> target_queue;
    /// variable holding physical thread count 
	volatile size_t threads;
    /// array holding STL thread info for each physical thread(allocated through pthreads)
	std::vector<std::thread> workers;
    /// collective name for all threads, used for debugging
	string name;
    /// mode of threading  which the threadpool is initialized for(ENCLAVE,REGULAR)
	threading_mode_t mode;
    /// variable holding next assignable monotonic physical thread id.
	unsigned volatile monotonic_thrd_id = 0;
	unsigned get_thrd_id();
	ThreadPool(size_t threads, threading_mode_t mode = REGULAR_MODE, string name = "diggi_thread");
	~ThreadPool();
	void Yield();
	/*Copy from caller*/
	void Schedule(async_cb_t cb, void *args, const char *label);
	void ScheduleOn(size_t id, async_cb_t cb, void *args, const char *label);
	int currentThreadId();
	size_t physicalThreadCount();
	size_t currentVThreadId();
	static void  alignstack(void * ptr);
	static void  SchedulerLoop(void *ptr, int status);
	static void  SchedulerLoopInternal(void *ptr);
	void InitializeThread();
	void Stop();
    bool Alive();
};

#endif
