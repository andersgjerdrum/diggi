/**
 * @file threadpool.cpp
 * @author your name (you@domain.com)
 * @brief implementation of diggi threadpool.
 * include all threading facilites used for virtual non-preemptive threading and asynchronous scheduling.
 * For each physical thread allocated to a particular threadpool, a statically defined number of virtual threads are allocated, defined by MAX_COROUTINES.
 * each virtual thread is allocated a separate stack of STACKSIZE. each virtual thread is subject to cooperative sheduling, invoked in round robin through Yield()
 * One physical thread cannot be hosted by multiple threadpools.
 * @version 0.1
 * @date 2020-02-04
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#include "threading/ThreadPool.h"

/// thread local argument when switching to virtual thread.
static thread_local void *coarg;
/// thread local current virtual thread
static thread_local int coroutine_id = -1;
/// thread local next virtual thread in round robin scheduler.
static thread_local int next_coroutine_id = 0;

///GCC compiler options to supress problems caused by stack manipulation.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"

/**
 * @brief transition to next virtual thread.
 * 
 * @param here the execution context of current virtual thread
 * @param there the execution context of next virtual thread
 * @param arg argument passed to next virtual thread
 * @return void* return value will not be used, or ever reached.(hence the compiler suppressions above)
 */
void *coto(jmp_buf_d here, jmp_buf_d there, void *arg)
{
	coarg = arg;
	if (setjmp_d(here))
		return (coarg);
	longjmp_d(there, 1);
}
///pop gcc warning supression
#pragma GCC diagnostic pop
///Simpler to adhere to regular linux x86, we use downwards stack
#define STACKDIR - // set to + for upwards and - for downwards


/*
    Debug Requires more stack space for symbols
    howver, release binary enclave must be as small as possible, 
    so we set it accordingly
*/

#ifdef RELEASE
#define STACKSIZE (1 << 16)
#else
#define STACKSIZE (1 << 17)

#endif
/// top of stack
static thread_local char *tos;
/**
 * @brief initialize and start a new virtual thread
 * 
 * @param here this execution context
 * @param fun callback executing new virtual thread
 * @param arg arguments provided to new virtual thread.
 * @return void* not used (or: dont worry about it)
 */
void *cogo(jmp_buf_d here, void (*fun)(void *), void *arg)
{
	if (tos == NULL)
		tos = (char *)&arg;
	tos += STACKDIR STACKSIZE;
	char n[STACKDIR(tos - (char *)&arg)];
	coarg = n; // ensure optimizer keeps n
	if (setjmp_d(here))
		return (coarg);
	fun(arg);
	return nullptr;
}
///virtual threads per physicaln threa
#define MAX_COROUTINES 2
/**
 * @brief assing a thread id to a physical thread.
 * monotonically increasing
 * @return unsigned new thread id
 */
unsigned ThreadPool::get_thrd_id()
{
	unsigned local_id = 0;
	do
	{
		local_id = monotonic_thrd_id;
	} while (__sync_val_compare_and_swap(&monotonic_thrd_id, local_id, local_id + 1) != local_id);

	return local_id;
}
/**
 * @brief Construct a new Thread Pool:: Thread Pool object
 * creates a new thread pool for a predefined number of physical threads.
 * If created in threading_mode_t is ENCLAVE_MODE, we expect an elligible thread to call InitializeThread() which captures it for the threadpool.
 * Used sinse all threads begin execution in regular process memory.
 * if threading_mode_t is ENCLAVE_MODE InitializeThread() must at most be invoked by the expected threadcount.
 * if REGULAR_MODE the threadpool will internally create the neccesary physical threads.
 * Physical thread ids and virtual thread ids are only relative to this threadpool. 
 * Other threadpools may have conflicting internal identity representation.
 * 
 * Creator allocates a target work queue fo each thread and execution state(jmp_bufs) for each virtual thread.
 * work queues are used for cross-physical-thread messaging allowing threads to invoke ScheduleOn(thread id, parameter) to call other threads.
 * work queues use lockfree_rb_q.cpp implementation.
 * Threadpool created on separate thread, but all apis except for stop must be invoked on threads in pool.
 * @see lockfree_rb_q.cpp
 * @param threads thread count of expected threads executing in this threadpool
 * @param mode ENCLAVE_MODE or REGULAR_MODE
 * @param name set pthread name for all physical threads in pool, used for simplified debugging
 */
ThreadPool::ThreadPool(size_t threads,
					   threading_mode_t mode,
					   string name) : quit(0),
									  stop(1),
									  threads(threads),
									  name(name),
									  mode(mode)
{
	for (unsigned i = 0; i < threads; i++)
	{
		coroutine_bufs.push_back(std::vector<jmp_buf_internal_t>(MAX_COROUTINES));
		target_queue.push_back(lf_new(THREAD_POOL_SIZE, threads + 1, threads + 1));
#ifndef DIGGI_ENCLAVE
		if (mode != ENCLAVE_MODE)
		{
			workers.push_back(new_thread_with_affinity(ThreadPool::SchedulerLoop, this));
		}
#endif
	}
	stop = 0;
	__sync_synchronize();
}
/**
 * @brief retrieve physical thread count
 * 
 * @return size_t 
 */
size_t ThreadPool::physicalThreadCount()
{
	return threads;
}
/**
 * @brief stop threadpool
 * blocks while waiting for join operation on threads.
 * Should be invoked by thread not on threadpool, or else it will deadlock
 * 
 */
void ThreadPool::Stop()
{

	// DIGGI_ASSERT(quit == 0);
	// DIGGI_ASSERT(stop == 0);

	__sync_fetch_and_add(&stop, 1);
	/*
		causes no more tasks to be scheduled and pops all remaining tasks
	*/
	auto id = currentThreadId();
	if (id < 0)
	{
		/*
			for destruction purposes, we allow main thread to schedule onto pool 
		*/
		id = 0;
	}

	for (size_t i = 0; i < threads; i++)
	{
		lf_send(target_queue[i], nullptr, id);
	}
	/*
		ensure all threads exit loop  before join, as stack may be affected by early join. 
	*/
	// while (quit < threads)
	// {
	// 	__sync_synchronize();
	// }

	// printf("attempting join\n");
	if (mode != ENCLAVE_MODE)
	{
		for (size_t i = 0; i < workers.size(); ++i)
		{
			workers[i].join();
		}
	}
}
bool ThreadPool::Alive(){
    return (stop == 0);
}
/**
 * @brief Destroy the Thread Pool:: Thread Pool object
 * Destroy Threadpool datastructures. 
 * Must be invoked AFTER threadpool stop() has completed
 */
ThreadPool::~ThreadPool()
{

	DIGGI_ASSERT(stop);
	for (auto lf : target_queue)
	{
		lf_destroy(lf);
	}

	target_queue.clear();

	if (mode != ENCLAVE_MODE)
	{
		workers.clear();
	}
	coroutine_bufs.clear();
}

/**
 * @brief schedule callback on top of working queue on current thread(Self)
 * will execute after all preceding pending callbacks are done (FIFO)
 * May be invoked by thread not on pool. In this case the "self"-thread is interpreted as physical thread id 0.
 * Will not schedule callback if threadpool stop is invoked in the interim.
 * @param cb callback to invoke
 * @param args arguments passed to callback
 * @param label optional string function label for debuggability(can be null)
 */
void ThreadPool::Schedule(async_cb_t cb, void *args, const char *label)
{
	if (stop)
	{
		return;
	}
	if (quit)
	{
		return;
	}
	//DIGGI_ASSERT(args);
	DIGGI_ASSERT(cb);
	auto workitem = (async_work_t *)malloc(sizeof(async_work_t));
	workitem->label = label;
	workitem->cb = cb;
	workitem->arg = args;
	workitem->status = 1;

	auto id = currentThreadId();
	if (id < 0)
	{
		/*
			for initialization purposes, we allow main thread to schedule onto pool
		*/
		id = 0;
	}
	lf_send(target_queue[id], workitem, id);
}
/**
 * @brief Schedule callback on thread with id
 * Schedule a callback on a thread with a given callback.
 * Will execute after all preceding pending callbacks are done (FIFO)
 * May be invoked by thread not on pool. 
 * Can also invoke on self.
 * will not schedule callback if threadpool stop is invoked in the interim.
 * @param id identity of target thread
 * @param cb callback to execute on target thread,
 * @param args arguments passed to callback
 * @param label ontional string function label for debuggabillity(can be null)
 */
void ThreadPool::ScheduleOn(size_t id, async_cb_t cb, void *args, const char *label)
{

	if (stop)
	{
		return;
	}
	if (quit)
	{
		return;
	}
	DIGGI_ASSERT(id < threads);
	//DIGGI_ASSERT(args);
	DIGGI_ASSERT(cb);
	auto workitem = (async_work_t *)malloc(sizeof(async_work_t));
	workitem->label = label;
	workitem->cb = cb;
	workitem->arg = args;
	workitem->status = 1;
	auto self_id = currentThreadId();
	if (self_id < 0)
	{
		/*
			for initialization purposes, we allow main thread to schedule onto pool
		*/
		self_id = 0;
	}
	lf_send(target_queue[id], workitem, self_id);
}
/**
 * @brief get current physical thread id.
 * 
 * @return int 
 */
int ThreadPool::currentThreadId()
{

	/*
		Use intermediary because global thread_id value must be resolved to a static
		symbol in base process

	*/
	return thr_id();
}
/**
 * @brief get current virtual thread id
 * 
 * @return size_t 
 */
size_t ThreadPool::currentVThreadId()
{
	return __pthr_id;
}

/**
 * @brief Method for cooperative yielding of physical thread, allowing other virtual threads to execute.
 * Will attempt to execute all other virtual threads allocated to this particular physical thread before returning here (round robin)
 * 
 * TODO: when threadpool exits, we may have dangling async_work_t causing memleaks
 * TODO: Supress invalid write in file, as we are manipulating the stack which confuses valgrind
 */
void ThreadPool::Yield()
{
    /*
        We want maximum utility from threads and so it does not make sense to limit them, 
        might block applicaitons
    */

    // DIGGI_ASSERT (currentVThreadId() <= physicalThreadCount() * MAX_THREADS_NESTED);

    #ifdef TEST_DEBUG
        if (currentVThreadId() >= physicalThreadCount() * MAX_THREADS_NESTED)
        {
            #ifdef DIGGI_ENCLAVE
                __asm volatile("pause" ::
                            : "memory");
            #else
                pthread_yield();
            #endif
            return;
        }
    #endif
    volatile int self_id = currentThreadId();

	async_work_t *volatile task = (async_work_t *)lf_try_recieve(target_queue[self_id], self_id);
    if (task != nullptr)
    {
        DIGGI_ASSERT(task->label != nullptr);
        DIGGI_ASSERT(task->cb != nullptr);
        // printf("schedule task=%p, pointer to loc=%p\n",task, &task);
        __pthr_id += physicalThreadCount();

        task->cb(task->arg, task->status);
        __pthr_id -= physicalThreadCount();

        // printf("end schedule task=%p, pointer to loc=%p\n",task, &task);

        free(task);
        /*workitem is responsible for deallocating argument resources*/
    }
	volatile int self_vthread = currentVThreadId();
	__pthr_id += physicalThreadCount();
	DIGGI_ASSERT(coroutine_id > 0);
	volatile int cur_id = coroutine_id;
	if(next_coroutine_id < MAX_COROUTINES - 1){
			coroutine_id = 0;
	}
	else if (coroutine_id + 1 == MAX_COROUTINES)
	{
		coroutine_id = 1;
	}
	else
	{
		coroutine_id = (cur_id + 1) % (MAX_COROUTINES);
	}
	#ifdef DIGGI_ENCLAVE
	// printf("yield to coroutine=%d from current=%d ,physicalthread = %d\n", coroutine_id, cur_id, self_id);
	#endif
	coto(coroutine_bufs[self_id][cur_id].inner, coroutine_bufs[self_id][coroutine_id].inner, nullptr);
	DIGGI_ASSERT(coroutine_id > 0);
	#ifdef DIGGI_ENCLAVE
	// printf("back from yield to coroutine=%d from current=%d ,physicalthread = %d\n", coroutine_id, cur_id, self_id);
	#endif	
	__pthr_id = self_vthread;
}
/**
 * @brief Initialize thread, used to capture threads into threadpool. 
 * Used by main thread allocated to untrusted runtime.
 * Used by enclave instances, where each thread enters enclave via special Ecall operation and invoke this method to participate in threadpool.
 */
void ThreadPool::InitializeThread()
{
	SchedulerLoop(this, 1);
	DIGGI_ASSERT(quit);
}
/**
 * @brief starting point for all physical threads.
 * Callback invoked from pthread create or InitializeThread().
 * Initializes and begins virtual thread execution loop for each physical thread.
 * Each physical thread, will in round robin execute virtual threads. 
 * Virtual threads relinquish controll volentarily through yield().
 * 
 * @param ptr 
 * @param status 
 */
void ThreadPool::SchedulerLoop(void *ptr, int status)
{
	volatile int count = 0;
	ThreadPool *volatile pool = (ThreadPool *)ptr;
	while (pool->stop)
	{
		__asm volatile("pause" ::
						   : "memory");
	}
	DIGGI_ASSERT(pool);
	if (thr_id() < 0)
	{

#ifndef DIGGI_ENCLAVE
/// set name for debuggability. if executing inside an enclave, this api is not availible.
		volatile int setname = pthread_setname_np(pthread_self(), pool->name.substr(0, 15).c_str());
		if (setname)
		{
			errno = setname;
			/*
				failed to set threadname
			*/
			DIGGI_ASSERT(false);
		}
#endif
		/*
			make sure id is less than consumer size
		*/
		set_thr_id(pool->get_thrd_id());
	}
	__pthr_id = pool->currentThreadId();
	volatile int self_id = pool->currentThreadId();
	DIGGI_ASSERT(coroutine_id == -1);
    ///begin virtual thread loops
	while (++count < MAX_COROUTINES)
	{
		coroutine_id = 0;
		cogo(pool->coroutine_bufs[self_id][0].inner, ThreadPool::alignstack, ptr);
		if(!pool->stop){
			DIGGI_ASSERT(coroutine_id == 0);
		}
	}
	coroutine_id = -1;
	next_coroutine_id = 0;
	__sync_fetch_and_add(&pool->quit, 1);
}
/**
 * @brief forces push onto stack if not aligned on 16 bytes. 
 * Avoids triggering SGX SDK stack validation.
 * @param ptr 
 */
void ThreadPool::alignstack(void * ptr){
	__asm__ volatile("pushq    %rsp");
	__asm__ volatile("subq    $16,%rsp");
	__asm__ volatile("andq    $-0x10,%rsp");
	ThreadPool::SchedulerLoopInternal(ptr);
	__asm__ volatile("popq %rsp");
}
/**
 * @brief internal loop executed by each physical thread.
 * Virtual threads returning to this loop are done executing.
 * @param ptr this (ThreadPool pointer)
 */
void ThreadPool::SchedulerLoopInternal(void *ptr)
{
	ThreadPool *volatile pool = (ThreadPool *)ptr;
	if (pool->stop)
	{
		return;
	}
	DIGGI_ASSERT(coroutine_id == 0);
	next_coroutine_id++;
	coroutine_id = next_coroutine_id;
	volatile int self_id = pool->currentThreadId();
	while (!pool->stop)
	{
		DIGGI_ASSERT(coroutine_id > 0);
		async_work_t *volatile task = (async_work_t *)lf_try_recieve(pool->target_queue[self_id], self_id);
		if (task != nullptr)
		{
			DIGGI_ASSERT(task->label != nullptr);
			DIGGI_ASSERT(task->cb != nullptr);
			// printf("schedule task=%p, pointer to loc=%p\n",task, &task);
			task->cb(task->arg, task->status);
			// printf("end schedule task=%p, pointer to loc=%p\n",task, &task);

			free(task);
			/*workitem is responsiblqe for deallocating argument resources*/
		}
        /*
            allow other untrusted threads to run
            important, in case we are overprovisioning threads.
        */
        #ifndef DIGGI_ENCLAVE
            pthread_yield();
        #endif

		volatile int cur_id = coroutine_id;
		if(next_coroutine_id < MAX_COROUTINES - 1){
			coroutine_id = 0;
		}
		else if (coroutine_id + 1 == MAX_COROUTINES)
		{
			coroutine_id = 1;
		}
		else
		{
			coroutine_id = (cur_id + 1) % (MAX_COROUTINES);
		}
		DIGGI_ASSERT(coroutine_id >= 0);
#ifdef		DIGGI_ENCLAVE
		// printf("going to target=%d cur=%d,physicalthread = %d\n", coroutine_id, cur_id, self_id);
		#endif
		coto(pool->coroutine_bufs[self_id][cur_id].inner, pool->coroutine_bufs[self_id][coroutine_id].inner, nullptr);
	#ifdef DIGGI_ENCLAVE
		// printf("back from target=%d cur=%d,physicalthread = %d\n", coroutine_id, cur_id, self_id);
#endif
	}
	coto(pool->coroutine_bufs[self_id][coroutine_id].inner, pool->coroutine_bufs[self_id][0].inner, nullptr);
}