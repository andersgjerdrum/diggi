/**
 * @file pthreadStubs.cpp
 * @author Anders Gjerdrum (anders.gjerdrum@uit.no)
 * @brief A partial implementation of pthreads on top of the diggi runtime threadpool implementation,
 * Not preemptable, however, simulated by cooperative scheduling, requires consumers to yield explicitly.
 * allow unbound ammounts of threads by virtual threading 
 * @see  ThreadPool::ThreadPool
 * @version 0.1
 * @date 2020-02-03
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#include "DiggiAssert.h"
#include "misc.h"
#include "posix/pthread_stubs.h"
#include "threading/IThreadPool.h"
#ifdef __cplusplus
extern "C" {
#endif



/**
 * @brief threadpool api reference.
 * 
 */
static IThreadPool *t_pool_obj = nullptr;
/**
 * @brief state structure for join info for pthread
 * 
 */
typedef struct pthread_state_arr_t {
    ///return value pointer
	void * retval;
    ///flag set when thread is joinable, after execution returns
	volatile int joinable_flag;
}pthread_state_arr_t;
/**
 * @brief static array holding all join pthreads state
 * 
 */
static pthread_state_arr_t join_array[MAX_VIRTUAL_THREADS];

/**
 * @brief state structure for pthreads
 * 
 */
typedef struct pthread_pack_data_t {
    ///argument input to thread
	void *arg;
    /// id of pthread
	pthread_t thrdid;
    /// pthreads execution callback
	void *(*strt)(void*);
}pthread_pack_data_t;
/// next asignable thread, index into static array
static volatile size_t curr_head_thread = 1;

/**
 * @brief set thread pool api 
 * initializes join array.
 * 
 * @param tpool 
 */
void pthread_stubs_set_thread_manager(void * tpool) {
	DIGGI_ASSERT(!t_pool_obj);
	DIGGI_ASSERT(tpool);
	t_pool_obj = (IThreadPool *)tpool;
	for (int i = 0; i < MAX_VIRTUAL_THREADS; i++) {
		/*
			initialize all to not joinable
		*/
		join_array[i].joinable_flag = 0;
		join_array[i].retval = NULL;

	}
}
/**
 * @brief unset thread api reference
 * 
 */
void pthread_stubs_unset_thread_manager() {
	t_pool_obj = nullptr;
	curr_head_thread = 1;
}

/**
 * @brief declaration of phtread 
 * holds identifier
 */
struct pthread
{
    int pt_id;
};
/**
 * @brief declaration of mutex
 * holds volatile concurrently checkable lock variable
 * 
 */
struct pthread_mutex
{
   volatile int mu_lock;
};
/**
 * @brief initialize mutex attribute
 * noop, as we do not support any special conventions
 * @warning for threads expecting special behaviours, this may cause bugs
 * @return int 
 */
int   i_pthread_mutexattr_init(pthread_mutexattr_t *) 
{
	return 0;
}
/**
 * @brief set type on mutex
 * noop, as we do not support any special conventions
 * @warning for threads expecting special behaviours, this may cause bugs
 * @return int 
 */
int i_pthread_mutexattr_settype(pthread_mutexattr_t *, int)
{
	return 0;
}
/**
 * @brief destroy mutex attribute
 * noop, as we do not support any special conventions 
 * @return int 
 */
int  i_pthread_mutexattr_destroy(pthread_mutexattr_t *)
{
	return 0;
}

/**
 * @brief initialize a mutex and clear the ownership and locking info 
 * 
 * @param mutext 
 * @param attr 
 * @return int 
 */
int i_pthread_mutex_init (pthread_mutex_t *mutext, const pthread_mutexattr_t *attr)
{
	DIGGI_ASSERT(t_pool_obj->currentVThreadId() < MAX_VIRTUAL_THREADS);
	mutext->__data.__lock = 0;
	/*
		We use thread id + 1 to specify ownership of lock, used for recursive lock support.
		As default value may be 0 if mutex is staticaly initialized before syscall interposition occurs
	*/
	mutext->__data.__owner = 0;

	return 0;
}
/**
 * @brief lock mutex,
 * blocks virtual thread until success.
 * Internally yield to other virtual threads while waiting.
 * Sets current thread as owner of mutex.
 * Mutex locks may be recursively invoked(nested).
 * sets ownership of lock to current thread upon success.
 * @param mutex 
 * @return int 
 */
int i_pthread_mutex_lock (pthread_mutex_t *mutex)
{	
	DIGGI_ASSERT(t_pool_obj->currentVThreadId() < MAX_VIRTUAL_THREADS);
	//printf("attempting lock mutex %p, mutex on thread %lu\n", mutex, t_pool_obj->currentVThreadId());

	if (mutex->__data.__lock != ((int)t_pool_obj->currentVThreadId() + 1)) {
		while (__sync_val_compare_and_swap(&mutex->__data.__lock, 0, ((int)t_pool_obj->currentVThreadId() + 1)) != 0) {
			t_pool_obj->Yield();
		} 
	}
	else {
		DIGGI_ASSERT(mutex->__data.__count > 0);

	}
	//printf("locked mutex %p, mutex on thread %lu\n", mutex, t_pool_obj->currentVThreadId());
	mutex->__data.__owner = (int)t_pool_obj->currentVThreadId() + 1;
	mutex->__data.__count++;
    return 0;
}
/**
 * @brief unlock mutex
 * Must be invoked by same virtual thread which locked the mutex in the first place.
 * if nested locks, the lock will only release once last unlock is called.
 * last unlock will unset ownership
 * @param mutex 
 * @return int 
 */
int i_pthread_mutex_unlock (pthread_mutex_t *mutex)
{
	DIGGI_ASSERT(t_pool_obj->currentVThreadId() < MAX_VIRTUAL_THREADS);
	DIGGI_ASSERT(mutex->__data.__count > 0);
	if (--mutex->__data.__count > 0) {
		/*
			We expect the locking thread to do its own unlocking 
			therefore we expect the owning thread to be itself.
		*/
		DIGGI_ASSERT(mutex->__data.__owner == ((int)t_pool_obj->currentVThreadId() + 1));
		return 0;
	}
	DIGGI_ASSERT(mutex->__data.__owner != 0);
	DIGGI_ASSERT(mutex->__data.__lock != 0);
	mutex->__data.__owner = 0;
	__sync_synchronize();
	mutex->__data.__lock = 0;
	//printf("unlocked mutex %p, mutex on thread %lu\n", mutex, t_pool_obj->currentVThreadId());
	return 0;
}
/**
 * @brief attempt to lock mutex without blocking
 * May be used in nested call.
 * upon success, sets lock ownership to current thread.
 * @param mutex 
 * @return int 
 */
int i_pthread_mutex_trylock (pthread_mutex_t *mutex)
{
	//printf("attempting try lock mutex %p, mutex on thread %lu\n", mutex, t_pool_obj->currentVThreadId());

	DIGGI_ASSERT(t_pool_obj->currentVThreadId() < MAX_VIRTUAL_THREADS);
	if (mutex->__data.__lock != ((int)t_pool_obj->currentVThreadId() + 1)) {

		if (__sync_val_compare_and_swap(&mutex->__data.__lock, 0, ((int)t_pool_obj->currentVThreadId() + 1)) == 0) {
			mutex->__data.__owner = (int)t_pool_obj->currentVThreadId() + 1;
			mutex->__data.__count++;
			//printf("trylock succeded mutex %p, mutex on thread %lu\n", mutex, t_pool_obj->currentVThreadId());
			return 0;
		}
		else {
			return EBUSY;
		}
	}
	else {
		DIGGI_ASSERT(mutex->__data.__count > 0);
		mutex->__data.__count++;
		DIGGI_ASSERT(mutex->__data.__lock == ((int)t_pool_obj->currentVThreadId() + 1));
		//printf("trylock succeded mutex %p, mutex on thread %lu\n", mutex, t_pool_obj->currentVThreadId());
		return 0;
	}
	return EBUSY;
}
/**
 * @brief destroy mutex.
 * Currently a noop as no internal state is required.
 * TODO: should purge content of mutex, and ensure no locks are held.
 * @param mutex 
 * @return int 
 */
int i_pthread_mutex_destroy (pthread_mutex_t *mutex)
{
    return 0;
}

/**
 * @brief internal executor callback for all pthreads.
 * executes pthread funciton set by pthread create and, upon completion, unsets the joinable flag for the current virtual thread.
 * stores return value from thread into join_array entry for current virtual thread
 * @param ptr 
 * @param status 
 */
void __pthread_internal_run(void * ptr, int status) 
{
	auto dt = (pthread_pack_data_t*)ptr;
    DIGGI_ASSERT(dt->thrdid < MAX_VIRTUAL_THREADS);
	DIGGI_ASSERT(join_array[dt->thrdid].joinable_flag);
	/* 
		Execute pthread callback 
	*/

	join_array[dt->thrdid].retval = dt->strt(dt->arg);

	while (__sync_val_compare_and_swap(&join_array[dt->thrdid].joinable_flag, 1, 0) != 1){
        printf("yieldbonanza\n");
        t_pool_obj->Yield();
	}

	/*Join Point*/
	delete dt;
}

/**
 * @brief create a new pthread using the threadpool virtual thread implementation
 * @warning, only works on physical threads, may be a bug here.
 * @warning should use virtual threads.
 * @param thread pthread datastructure
 * @param attr pthread attributes, not used in current impl
 * @param start_routine callback used to execute new thread
 * @param arg arguments to new thread
 * @return int 
 */
int i_pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                  void *(*start_routine)(void*), void *arg)
{
	DIGGI_ASSERT(thread != NULL);
	DIGGI_ASSERT(start_routine != NULL);
	DIGGI_ASSERT(t_pool_obj->currentVThreadId() < MAX_VIRTUAL_THREADS);
	/*
		No thread calls create before someone calls create on it
	*/
	DIGGI_ASSERT(curr_head_thread > t_pool_obj->currentVThreadId());
	unsigned local_id = 0;
	do {
		local_id = curr_head_thread;
	} while (__sync_val_compare_and_swap(&curr_head_thread, local_id, local_id + 1) != local_id);

	auto pack = new pthread_pack_data_t();
	pack->arg = arg;
	pack->strt = start_routine;
	pack->thrdid = local_id;

	while (__sync_val_compare_and_swap(&join_array[local_id].joinable_flag, 0, 1) != 0) {
        t_pool_obj->Yield();
	}

	*thread = local_id;
    
	t_pool_obj->ScheduleOn(local_id % t_pool_obj->physicalThreadCount(), __pthread_internal_run, pack, __PRETTY_FUNCTION__);

    return 0;
}
/**
 * @brief wait for thread to join
 * will block until thread is joinable
 * threads are joinable after callback completes execution in __pthread_internal_run
 * will yield to allow other virtual threads to execute in the interim. 
 * @param thread 
 * @param value_ptr 
 * @return int 
 */
int i_pthread_join (pthread_t thread, void **value_ptr)
{
	DIGGI_ASSERT(thread < MAX_VIRTUAL_THREADS);
	while(join_array[thread].joinable_flag){
		t_pool_obj->Yield();
	}

	*value_ptr = join_array[thread].retval;
	join_array[thread].retval = NULL;
    return 0;
}
/**
 * @brief not implemented, will throw assertion failure
 * 
 * @param cond 
 * @param mutex 
 * @return int 
 */
int i_pthread_cond_wait (pthread_cond_t *cond, pthread_mutex_t *mutex)
{
	DIGGI_ASSERT(false);
    return 0;
}
/**
 * @brief  not implemented, will throw assertion failure
 * 
 * @param cond 
 * @return int 
 */
int i_pthread_cond_broadcast (pthread_cond_t *cond)
{
	DIGGI_ASSERT(false);
    return 0;
}
/**
 * @brief  not implemented, will throw assertion failure
 * 
 * @param cond 
 * @return int 
 */
int i_pthread_cond_signal (pthread_cond_t *cond)
{
	DIGGI_ASSERT(false);
    return 0;
}
/**
 * @brief  not implemented, will throw assertion failure
 * 
 * @param cond 
 * @param attr 
 * @return int 
 */
int i_pthread_cond_init (pthread_cond_t *cond, const pthread_condattr_t *attr)
{
	DIGGI_ASSERT(false);
    return 0;
}
/**
 * @brief return own pthread id, virtual thread id.
 * 
 * @return int 
 */
int i_pthread_self(void){
	return (int)t_pool_obj->currentVThreadId();
}
#ifdef __cplusplus
}
#endif // __cplusplus