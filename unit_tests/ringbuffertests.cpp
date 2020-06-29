#include <gtest/gtest.h>
#include "lockfree_rb_q.h"
#include <thread>

void producerlf_c(lf_buffer_t *rb, size_t id, unsigned rounds)
{
	for (unsigned i = 0; i < rounds; i++)
	{
		auto itm = malloc(sizeof(int));
		memcpy(itm, &i, sizeof(int));
		lf_send(rb, itm, id);
	}
}

void consumerlf_c(lf_buffer_t *rb, size_t id, unsigned rounds)
{
	for (unsigned i = 0; i < rounds; i++)
	{
		auto recv_itm = lf_recieve(rb, id);

		EXPECT_TRUE(recv_itm != NULL); 

		free(recv_itm);
	}
}

/*
	Life lesson here: If you increase the amount of threads above physical cores, 
	you should insert usleep to avoid starvation.
*/

TEST(ringbuffertests, multithreaded_8_way_test)
{

	auto rb = lf_new(16, 4, 4);

	std::thread p1(producerlf_c, rb, 0, 100000);
	std::thread p2(producerlf_c, rb, 1, 100000);
	std::thread p3(producerlf_c, rb, 2, 100000);
	std::thread p4(producerlf_c, rb, 3, 100000);
	//std::thread p5(produclfer_c, rb, 100000);
	std::thread c1(consumerlf_c, rb, 0, 100000);
	std::thread c2(consumerlf_c, rb, 1, 100000);
	std::thread c3(consumerlf_c, rb, 2, 100000);
	std::thread c4(consumerlf_c, rb, 3, 100000);
	//std::thread c5(consumer_c, rb, 100000);

	p1.join();
	p2.join();
	p3.join();
	p4.join();
	//p5.join();
	c1.join();
	c2.join();
	c3.join();
	c4.join();
	//c5.join();
	EXPECT_TRUE(true);
	lf_destroy(rb);
}