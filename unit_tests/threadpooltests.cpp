#include <gtest/gtest.h>
#include "threading/ThreadPool.h"
#include "threading/IThreadPool.h"
#include <vector>
#include <thread>
static volatile uint64_t nextid = 0;

#define TOTAL_ITTERATIONS 10000
unsigned incmonocounter_tp(volatile uint64_t *monoptr)
{
	unsigned local_id = 0;
	do
	{
		local_id = *monoptr;
	} while (__sync_val_compare_and_swap(monoptr, local_id, local_id + 1) != local_id);
	return local_id;
}

static IThreadPool *threadpool = nullptr;

void schedule_b(void *ptr, int status);
void schedule_a(void *ptr, int status)
{
	auto expected = (unsigned *)ptr;
	EXPECT_TRUE(expected != nullptr);
	EXPECT_TRUE(*expected == (unsigned)threadpool->currentThreadId());
	auto retval = incmonocounter_tp(&nextid);
	*expected = retval % threadpool->physicalThreadCount();
	if (retval < TOTAL_ITTERATIONS)
	{
		threadpool->ScheduleOn(*expected, schedule_b, expected, __PRETTY_FUNCTION__);
	}
}
void schedule_b(void *ptr, int status)
{
	auto expected = (unsigned *)ptr;
	EXPECT_TRUE(expected != nullptr);
	EXPECT_TRUE(*expected == (unsigned)threadpool->currentThreadId());
	auto retval = incmonocounter_tp(&nextid);
	*expected = retval % threadpool->physicalThreadCount();
	if (retval < TOTAL_ITTERATIONS)
	{
		threadpool->ScheduleOn(*expected, schedule_a, expected, __PRETTY_FUNCTION__);
	}
}

TEST(threadpool_tests, schedule_a_b)
{
	threadpool = new ThreadPool(8);
	for (unsigned i = 0; i < threadpool->physicalThreadCount(); i++)
	{
		auto val = malloc(sizeof(unsigned));
		memcpy(val, &i, sizeof(unsigned));
		threadpool->ScheduleOn(i, schedule_a, val, __PRETTY_FUNCTION__);
	}
	while (nextid < TOTAL_ITTERATIONS)
		;
	threadpool->Stop();
	delete threadpool;
}

void init_threads(void *tp)
{
	((ThreadPool *)tp)->InitializeThread();
}

TEST(threadpool_tests, schedule_a_b_trusted_threadpool)
{
	nextid = 0;
	threadpool = new ThreadPool(8, ENCLAVE_MODE);
	auto threadlist = std::vector<std::thread>();
	for (int x = 0; x < 8; x++)
	{
		threadlist.push_back(std::thread(init_threads, threadpool));
	}

	for (unsigned i = 0; i < threadpool->physicalThreadCount(); i++)
	{
		auto val = malloc(sizeof(unsigned));
		memcpy(val, &i, sizeof(unsigned));
		threadpool->ScheduleOn(i, schedule_a, val, __PRETTY_FUNCTION__);
	}

	while (nextid < TOTAL_ITTERATIONS)
		;
	threadpool->Stop();
	for (int x = 0; x < 8; x++)
	{
		threadlist[x].join();
	}
	delete threadpool;
}

static volatile int yield_count_total = 0;
void even_odd_yield_schedule(void *ptr, int status)
{
	auto tp = (ThreadPool *)ptr;
	auto testval = 1234567;
	if (yield_count_total < 100)
	{
		tp->Schedule(even_odd_yield_schedule, tp, __PRETTY_FUNCTION__);
		testval+=1;
		tp->Yield();
		testval-=1;
		EXPECT_TRUE(testval == 1234567);
		yield_count_total++;
	}
}

TEST(threadpool_tests, yield_bonansa_setjmp_test)
{
	auto tp = new ThreadPool(1, REGULAR_MODE);
	tp->Schedule(even_odd_yield_schedule, tp, __PRETTY_FUNCTION__);
	while (yield_count_total < 104);
	tp->Stop();

	EXPECT_TRUE(yield_count_total == 108);
	delete tp;
}
void just_yield(void *ptr, int status)
{
	// printf("starting just_yield\n");
	auto tp = (ThreadPool *)ptr;
	if (yield_count_total < 50)
	{
		/*
			testing that stack is not smashed.
		*/
		auto testval = 1234567;
		testval++;
		tp->Yield();
		testval--;
		EXPECT_TRUE(testval == 1234567);

		yield_count_total++;
	}
}
void just_schedule(void *ptr, int status){
	// printf("starting just_schedule\n");

	auto tp = (ThreadPool *)ptr;
	if (yield_count_total < 50)
	{
		// printf("scheduling yield\n");

		tp->Schedule(just_yield, tp, __PRETTY_FUNCTION__);
		// printf("scheduling self\n");

		tp->Schedule(just_schedule, tp, __PRETTY_FUNCTION__);

	}
}
TEST(threadpool_tests, yield_schedule_repeat)
{
	yield_count_total = 0;
	auto tp = new ThreadPool(1, REGULAR_MODE);
	tp->Schedule(just_schedule, tp, __PRETTY_FUNCTION__);
	while (yield_count_total < 50);

	tp->Stop();
	
	EXPECT_TRUE(yield_count_total >= 50);
	delete tp;
}


void yield_while_loop(void *ptr, int status){
	auto tp = (ThreadPool *)ptr;
	while(yield_count_total < 100){
		tp->Yield();
		yield_count_total++;
	}
}
TEST(threadpool_tests, while_yield_pattern){

	yield_count_total = 0;
	auto tp = new ThreadPool(1, REGULAR_MODE);
	tp->Schedule(yield_while_loop, tp, __PRETTY_FUNCTION__);
	while (yield_count_total < 100);

	tp->Stop();
	
	EXPECT_TRUE(yield_count_total == 100);
	delete tp;
}
void yield_while_loop_dual(void *ptr, int status){
	auto tp = (ThreadPool *)ptr;
	while(yield_count_total < 10000){
		tp->Yield();
		yield_count_total++;
	}
}

TEST(threadpool_tests, while_yield_pattern_2){

	yield_count_total = 0;
	auto tp1 = new ThreadPool(1, REGULAR_MODE);
	auto tp2 = new ThreadPool(1, REGULAR_MODE);

	tp1->Schedule(yield_while_loop_dual, tp1, __PRETTY_FUNCTION__);
	tp2->Schedule(yield_while_loop_dual, tp2, __PRETTY_FUNCTION__);

	while (yield_count_total < 10000);

	tp1->Stop();	
	tp2->Stop();

	
	EXPECT_TRUE(yield_count_total >= 10000);
	delete tp1;
	delete tp2;

}
