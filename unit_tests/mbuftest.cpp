#include <gtest/gtest.h>

#include "mbuf.h"
#include <csignal>
#include <math.h>       /* floor */

TEST(mbuf, chain_tail_to_head)
{

	auto mb = mbuf_new();
	
	char lol1[16];
	memset(lol1, 0x3, 16);
	char lol2[16];
	memset(lol2, 0x3, 16);	
	char lol3[16];
	memset(lol3, 0x3, 16);
	char lol4[16];
	memset(lol4, 0x3, 16);

	mbuf_append_tail(mb, lol1, 16, 0);
	mbuf_append_tail(mb, lol2, 16, 16);
	mbuf_append_tail(mb, lol3, 16, 32);
	mbuf_append_tail(mb, lol4, 16, 48);

	auto test = (char*)mbuf_remove_head(mb, 16);
	EXPECT_TRUE(lol1 == test);
	test = (char*)mbuf_remove_head(mb, 16);
	EXPECT_TRUE(lol2 == test);
	test = (char*)mbuf_remove_head(mb, 16);
	EXPECT_TRUE(lol3 == test);
	test = (char*)mbuf_remove_head(mb, 16);
	EXPECT_TRUE(lol4 == test);

	mbuf_destroy(mb, 0, 0);

}

TEST(mbuf, chain_head_to_tail)
{

	auto mb = mbuf_new();

	char lol1[16];
	memset(lol1, 0x3, 16);
	char lol2[16];
	memset(lol2, 0x3, 16);
	char lol3[16];
	memset(lol3, 0x3, 16);
	char lol4[16];
	memset(lol4, 0x3, 16);

	mbuf_append_head(mb, lol1, 16);
	mbuf_append_head(mb, lol2, 16);
	mbuf_append_head(mb, lol3, 16);
	mbuf_append_head(mb, lol4, 16);

	auto test = (char*)mbuf_remove_tail(mb, 16);
	EXPECT_TRUE(lol1 == test);
	test = (char*)mbuf_remove_tail(mb, 16);
	EXPECT_TRUE(lol2 == test);
	test = (char*)mbuf_remove_tail(mb, 16);
	EXPECT_TRUE(lol3 == test);
	test = (char*)mbuf_remove_tail(mb, 16);
	EXPECT_TRUE(lol4 == test);
	
	mbuf_destroy(mb, 0, 0);

}

TEST(mbuf, chain_stack_head)
{

	auto mb = mbuf_new();

	char lol1[16];
	memset(lol1, 0x1, 16);
	char lol2[16];
	memset(lol2, 0x2, 16);
	char lol3[16];
	memset(lol3, 0x3, 16);
	char lol4[16];
	memset(lol4, 0x4, 16);

	mbuf_append_head(mb, lol1, 16);
	mbuf_append_head(mb, lol2, 16);
	mbuf_append_head(mb, lol3, 16);
	mbuf_append_head(mb, lol4, 16);

	auto test = (char*)mbuf_remove_head(mb, 16);
	EXPECT_TRUE(lol4 == test);
	EXPECT_TRUE(lol4[0] == 0x4);

	test = (char*)mbuf_remove_head(mb, 16);
	EXPECT_TRUE(lol3 == test);
	EXPECT_TRUE(lol3[0] == 0x3);

	test = (char*)mbuf_remove_head(mb, 16);
	EXPECT_TRUE(lol2 == test);
	EXPECT_TRUE(lol2[0] == 0x2);

	test = (char*)mbuf_remove_head(mb, 16);
	EXPECT_TRUE(lol1 == test);
	EXPECT_TRUE(lol1[0] == 0x1);

	mbuf_destroy(mb, 0, 0);

}

TEST(mbuf, chain_stack_tail)
{
	auto mb = mbuf_new();

	char lol1[16];
	memset(lol1, 0x1, 16);
	char lol2[16];
	memset(lol2, 0x2, 16);
	char lol3[16];
	memset(lol3, 0x3, 16);
	char lol4[16];
	memset(lol4, 0x4, 16);

	mbuf_append_tail(mb, lol1, 16, 0);
	mbuf_append_tail(mb, lol2, 16, 16);
	mbuf_append_tail(mb, lol3, 16, 32);
	mbuf_append_tail(mb, lol4, 16, 48);

	auto test = (char*)mbuf_remove_tail(mb, 16);
	EXPECT_TRUE(lol4 == test);
	EXPECT_TRUE(lol4[0] == 0x4);

	test = (char*)mbuf_remove_tail(mb, 16);
	EXPECT_TRUE(lol3 == test);
	EXPECT_TRUE(lol3[0] == 0x3);

	test = (char*)mbuf_remove_tail(mb, 16);
	EXPECT_TRUE(lol2 == test);
	EXPECT_TRUE(lol2[0] == 0x2);

	test = (char*)mbuf_remove_tail(mb, 16);
	EXPECT_TRUE(lol1 == test);
	EXPECT_TRUE(lol1[0] == 0x1);

	mbuf_destroy(mb, 0, 0);

}


TEST(mbuf, chain_concat)
{

	auto mb = mbuf_new(); /*alloc */

	char lol1[16];
	memset(lol1, 0x1, 16);
	char lol2[16];
	memset(lol2, 0x2, 16);
	char lol3[16];
	memset(lol3, 0x3, 16);
	char lol4[16];
	memset(lol4, 0x4, 16);

	mbuf_append_tail(mb, lol1, 16, 0);	 /*alloc */
	mbuf_append_tail(mb, lol2, 16, 16);	/*alloc */
	mbuf_append_tail(mb, lol3, 16, 32);	/*alloc */
	mbuf_append_tail(mb, lol4, 16, 48);	/*alloc */

	auto mb2 = mbuf_new();	/*alloc */

	char lol5[16];
	memset(lol5, 0x5, 16);
	char lol6[16];
	memset(lol6, 0x6, 16);
	char lol7[16];
	memset(lol7, 0x7, 16);
	char lol8[16];
	memset(lol8, 0x8, 16);

	mbuf_append_tail(mb2, lol5, 16, 0);	/*alloc */
	mbuf_append_tail(mb2, lol6, 16, 16);	/*alloc */
	mbuf_append_tail(mb2, lol7, 16, 32);	/*alloc */
	mbuf_append_tail(mb2, lol8, 16, 48);	/*alloc */

	mbuf_concat(mb, mb2);

	auto test = (char*)mbuf_remove_head(mb, 16);	/*free */
	EXPECT_TRUE(lol1 == test);
	EXPECT_TRUE(lol1[0] == 0x1);

	test = (char*)mbuf_remove_head(mb, 16);			/*free */
	EXPECT_TRUE(lol2 == test);
	EXPECT_TRUE(lol2[0] == 0x2);

	test = (char*)mbuf_remove_head(mb, 16);			/*free */
	EXPECT_TRUE(lol3 == test);
	EXPECT_TRUE(lol3[0] == 0x3);

	test = (char*)mbuf_remove_head(mb, 16);			 /*free */
	EXPECT_TRUE(lol4 == test);
	EXPECT_TRUE(lol4[0] == 0x4);

	test = (char*)mbuf_remove_head(mb, 16);
	EXPECT_TRUE(lol5 == test);
	EXPECT_TRUE(lol5[0] == 0x5);

	test = (char*)mbuf_remove_head(mb, 16);
	EXPECT_TRUE(lol6 == test);
	EXPECT_TRUE(lol6[0] == 0x6);

	test = (char*)mbuf_remove_head(mb, 16);
	EXPECT_TRUE(lol7 == test);
	EXPECT_TRUE(lol7[0] == 0x7);

	test = (char*)mbuf_remove_head(mb, 16);
	EXPECT_TRUE(lol8 == test);
	EXPECT_TRUE(lol8[0] == 0x8);
	
	mbuf_destroy(mb, 0, 0); 
	mbuf_destroy(mb2, 0, 64);

}

TEST(mbuf, concat_refcount)
{

	char data[16];

	auto mb1 = mbuf_new();
	for (unsigned i = 0; i < 10; i++) {
		mbuf_append_tail(mb1, data, 16, i * 16);
	}

	auto mb2 = mbuf_new();
	for (unsigned i = 0; i < 10; i++) {
		mbuf_append_tail(mb2, data, 16, i * 16);
	}

	mbuf_concat(mb1, mb2);
	
	EXPECT_TRUE(mb1->head->ref == 1);
	EXPECT_TRUE(mb2->head->ref == 2);

	EXPECT_TRUE(mbuf_destroy(mb2, 0, 160) == 0);
	EXPECT_TRUE(mbuf_destroy(mb1, 0, 320) == 20);

}

TEST(mbuf, mbuf_node_at_position)
{

	char data[1024];
	auto mb1 = mbuf_new();

	for (unsigned i = 0; i < 1024; i += 16) {
		memset(&data[i], i, 16);
		mbuf_append_tail(mb1, &data[i], 16, i * 16);
	}

	for (unsigned i = 0; i < 1024; i += 16) {
		unsigned b = 0;
		auto node = mbuf_node_at_pos(mb1, i, &b);

		EXPECT_TRUE(b == i);
		for (unsigned x = 0; x < 16; x++) 
		{
			EXPECT_TRUE(node->data[x] == (char)i);
		}
	}
	EXPECT_TRUE(mbuf_destroy(mb1, 0, 64 * 16) == 64);

}


TEST(mbuf, mbuf_node_at_position_all)
{

	char data[16];

	auto mb1 = mbuf_new();

	for (unsigned i = 0; i < 1024; i+=16) {
		mbuf_append_tail(mb1, data, 16, 16 * i);
	}

	for (unsigned i = 0; i < 1024; i++) {
		unsigned b = 0;
		mbuf_node_at_pos(mb1, i, &b);
		EXPECT_TRUE(b == floor(((double)i)/16) * 16);
		
	}
	EXPECT_TRUE(mbuf_destroy(mb1, 0, 64 * 16) == 64);

}

TEST(mbuf, mbuf_to_printable) 
{

	ALIGNED_CONST_CHAR(8) lol[] = "man";
	ALIGNED_CONST_CHAR(8) str[] = "manmanmanmanmanmanmanmanmanman";
	auto mb1 = mbuf_new();
	for (unsigned i = 0; i < 10; i += 1) {
		mbuf_append_tail(mb1, lol, 3, 3 * i);
	}
	std::string str1;
	mbuf_to_printable(mb1, 30, 0, &str1);
	EXPECT_TRUE(strcmp(str1.c_str(), str) == 0);
	mbuf_destroy(mb1, 0, 30);
}



TEST(mbuf, malloc_free)
{

	auto ptr = malloc(8);
	auto mb1 = mbuf_new();
	mbuf_append_tail(mb1, (char*)ptr, 8, true, 0);
	EXPECT_TRUE(mb1->head->alloc == 1);
	EXPECT_TRUE(mb1->head->size == 8);
	mbuf_node_destroy(mb1->head, true);
	mbuf_destroy(mb1, 0, 0);
}

