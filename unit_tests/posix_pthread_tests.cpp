#include <gtest/gtest.h>
#include "Logging.h"
#include <unistd.h>
#include "threading/ThreadPool.h"
#include "posix/pthread_stubs.h"
#include <unistd.h>

class MockLog : public ILog
{
	std::string GetfuncName() {
		return "testfunc";
	}
	void SetFuncId(aid_t aid, std::string name = "func") {


	}
	void SetLogLevel(LogLevel lvl) {

	}
	void Log(LogLevel lvl, const char *fmt, ...) {

	}
	void Log(const char *fmt, ...) {

	}
};
static volatile int tester = 15;
static volatile bool creat_join_ran = false;
static volatile int creat_join_call_count = 0;
void *inc_x(void *x_void_ptr)
{

	EXPECT_TRUE(*(int*)x_void_ptr == 14);
	creat_join_ran = true;
	__sync_synchronize();
	creat_join_call_count++;
	__sync_synchronize();
	return (void*)&tester;
}
void create_pthread_cb(void *ptr, int status)
{
	pthread_t inc_x_thread;
	int test = 14;
	EXPECT_TRUE(i_pthread_create(&inc_x_thread, NULL, inc_x, &test) == 0);

	int data;
	void *datap = &data;
	void **datapp = &datap;
	EXPECT_TRUE(i_pthread_join(inc_x_thread, datapp) == 0);
	EXPECT_TRUE(data = 15);
	EXPECT_TRUE(creat_join_ran);
	EXPECT_TRUE(creat_join_call_count == 1);
	__sync_synchronize();
}

TEST(posix_pthread, create_join) 
{
	auto threadpool1 = new ThreadPool(2);
	pthread_stubs_set_thread_manager(threadpool1);
	threadpool1->Schedule(create_pthread_cb, nullptr, __PRETTY_FUNCTION__);
	while(!creat_join_call_count);
	threadpool1->Stop();
	delete threadpool1;
}

void multi_join_callback(void *ptr, int status){
	pthread_t inc_x_thread[10];

	int test = 14;

	for (int i = 0; i < 10; i++) 
	{
		EXPECT_TRUE(i_pthread_create(&inc_x_thread[i], NULL, inc_x, &test) == 0);
	}
	int data;
	void *datap = &data;
	void **datapp = &datap;

	for (int i = 0; i < 10; i++)
	{
		data = 0;
		EXPECT_TRUE(i_pthread_join(inc_x_thread[i], datapp) == 0);
		EXPECT_TRUE(data = 15);
	}
}

TEST(posix_pthread, create_join_multi)
{

	creat_join_call_count = 0;
	auto threadpool1 = new ThreadPool(10);
	pthread_stubs_unset_thread_manager();
	pthread_stubs_set_thread_manager(threadpool1);
	threadpool1->Schedule(multi_join_callback, nullptr,__PRETTY_FUNCTION__);
	while(creat_join_call_count < 10){
        usleep(1000);
    }
    EXPECT_TRUE(creat_join_call_count == 10);
	threadpool1->Stop();
	delete threadpool1;
}

static volatile int stop_threads = 0;
static volatile int run_thread_a = 1;
static pthread_mutex_t run_lock_a = PTHREAD_MUTEX_INITIALIZER;

static int iterations_a = 0, iterations_b = 0;
void *thread_a(void *x_void_ptr) {
	while (!stop_threads) {
		iterations_a++;
		/* Wait for Thread A to be runnable */
		i_pthread_mutex_lock(&run_lock_a); /*Testing recursive*/

	
		run_thread_a = 0;

		usleep(100);
		EXPECT_TRUE(run_thread_a == 0);
		i_pthread_mutex_lock(&run_lock_a);
		i_pthread_mutex_unlock(&run_lock_a);/*testing recursive*/
		i_pthread_mutex_unlock(&run_lock_a);

	}
	return NULL;
}
void *thread_b(void *x_void_ptr) 
{
	while (!stop_threads) {
		iterations_b++;

		/*testing recursive*/
		i_pthread_mutex_lock(&run_lock_a);
		i_pthread_mutex_lock(&run_lock_a);
		run_thread_a = 1;
		i_pthread_mutex_unlock(&run_lock_a);

		usleep(100);

		EXPECT_TRUE(run_thread_a == 1);

		i_pthread_mutex_unlock(&run_lock_a);
	}
	return NULL;

}

void mutex_recursive_cb(void *ptr, int status)
{
	pthread_t tread_a_t, tread_b_t;
	EXPECT_TRUE(i_pthread_create(&tread_a_t, NULL, thread_a, NULL) == 0);
	EXPECT_TRUE(i_pthread_create(&tread_b_t, NULL, thread_b, NULL) == 0);
}

TEST(posix_pthread, test_mutex_and_recursive) 
{
	auto threadpool1 = new ThreadPool(3);
	pthread_stubs_unset_thread_manager();
	pthread_stubs_set_thread_manager(threadpool1);
	threadpool1->Schedule(mutex_recursive_cb, nullptr, __PRETTY_FUNCTION__);
	while (iterations_b < 100) {
		__sync_synchronize();

	}
	while (iterations_a < 100) {
		__sync_synchronize();
	}
	stop_threads = 1;
	__sync_synchronize();

	threadpool1->Stop();
	delete threadpool1;
}



