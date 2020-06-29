#include <gtest/gtest.h>
#include "ZeroCopyString.h"

TEST(zero_copy_string, simple_concat_zc_w_constc)
{

	ALIGNED_CONST_CHAR(8) str1[] = "trolololololo\n";
	ALIGNED_CONST_CHAR(8)  str2[] = "man\n";

	std::string expstr(str1);
	expstr.append(str2);

	zcstring str(str1);

	str = str + str2;

	EXPECT_TRUE(strcmp(str.tostring().c_str(), expstr.c_str()) == 0);
	EXPECT_TRUE(str.size() == 18);
	auto mb = str.getmbuf();

	auto node = mb->head;
	while (node != nullptr) {
		EXPECT_TRUE(node->ref == 1);
		node = node->next;
	}
}

TEST(zero_copy_string, simple_concat_zc_w_zc)
{
	ALIGNED_CONST_CHAR(8) str1[] = "trolololololo\n";
	ALIGNED_CONST_CHAR(8)  str2[] = "man\n";

	std::string expstr(str1);
	expstr.append(str2);

	zcstring stra(str1);
	zcstring strb(str2);

	auto strc = stra + strb;

	EXPECT_TRUE(strcmp(strc.tostring().c_str(), expstr.c_str()) == 0);
	EXPECT_TRUE(strc.size() == 18);
	EXPECT_TRUE(stra.getmbuf()->head->ref == 2);
	EXPECT_TRUE(strb.getmbuf()->head->ref == 3);

	auto node = strc.getmbuf()->head;
	EXPECT_TRUE(node->ref == 2);
	EXPECT_TRUE(node->next->ref == 3);
	EXPECT_TRUE(node->next->next == nullptr);
}

TEST(zero_copy_string, simple_concat_zc_w_constc_2)
{
	ALIGNED_CONST_CHAR(8) str1[] = "trolololololo\n";
	ALIGNED_CONST_CHAR(8)  str2[] = "man\n";

	std::string expstr(str1);
	expstr.append(str2);

	zcstring stra(str1);

	auto strc = stra + str2;

	EXPECT_TRUE(strcmp(strc.tostring().c_str(), expstr.c_str()) == 0);
	EXPECT_TRUE(strc.size() == 18);
	EXPECT_TRUE(stra.getmbuf()->head->ref == 2);
	EXPECT_TRUE(strc.getmbuf()->head->ref == 2);

	auto node = strc.getmbuf()->head;
	EXPECT_TRUE(node->ref == 2);
	EXPECT_TRUE(node->next->ref == 2);
	EXPECT_TRUE(node->next->next == nullptr);
}

TEST(zero_copy_string, simple_substring) 
{	
	ALIGNED_CONST_CHAR(8) str1[] = "trololoxyololo\n";
	zcstring stra(str1);
	
	auto strb = stra.substr(7, 2);

	EXPECT_TRUE(strb.size() == 2);
	
	EXPECT_TRUE(strcmp(strb.tostring().c_str(), "xy") == 0);

}

TEST(zero_copy_string, simple_concat_substring)
{
	ALIGNED_CONST_CHAR(8) str1[] = "trolololololo\n";
	ALIGNED_CONST_CHAR(8)  str2[] = "man\n";

	std::string expstr(str1);
	expstr.append(str2);

	zcstring stra(str1);
	zcstring strb(str2);

	auto strc = stra + strb;
	EXPECT_TRUE(strc.getmbuf()->head->ref == 2);
	auto stre = strc.substr(14, 3);

	EXPECT_TRUE(stre.size() == 3);
	EXPECT_TRUE(strcmp(stre.tostring().c_str(), "man") == 0);

	EXPECT_TRUE(stre.getmbuf()->head->ref == 4);
}

/*
	Fails when compiler is ran with O2 optimizations, as if statement is optimized away.
*/

#pragma GCC push_options
#pragma GCC optimize ("O0")

TEST(zero_copy_string, simple_destructor_subscope)
{
	ALIGNED_CONST_CHAR(8) str1[] = "trolololololo\n";
	
	zcstring *stra = nullptr;

	if (true) {
		zcstring lol(str1);
		stra = &lol;
	}	

	//destructor called when out of subscope
	//akward but works
	EXPECT_TRUE(stra->size() == 0);
}

#pragma GCC pop_options


TEST(zero_copy_string, nested_substing)
{
	ALIGNED_CONST_CHAR(8) str1[] = "lololololololololololololololololololololololololololololololoo\n";
	unsigned size = 64;
	zcstring stra(str1);
	zcstring strb = stra;

	for (unsigned i = 1; i < size; i+=1)
	{
		stra = stra.substr(1, size - i);
		EXPECT_TRUE(stra.size() == size - i);
		EXPECT_TRUE(strcmp(stra.tostring().c_str(), &str1[i]) == 0);
		//destructor has been called
		EXPECT_TRUE(stra.getmbuf()->head->ref == 2);
	}

}

TEST(zero_copy_string, index_of) 
{
	ALIGNED_CONST_CHAR(8) str1[] = "lololololololololololololololoboylololololololololololololololoo\n";
	ALIGNED_CONST_CHAR(8) str2[] = "boy";
	zcstring stra(str1);
	zcstring strb(str2);
	EXPECT_TRUE(stra.indexof(&strb) == 30);
}

TEST(zero_copy_string, index_of_concat_string)
{
	ALIGNED_CONST_CHAR(8) str1[] = "lol";
	zcstring stra(str1);

	ALIGNED_CONST_CHAR(8) strx[] = "boy";
	zcstring strb(strx);

	for (int i = 0; i < 100; i++) {
		stra.append(str1);
	}

	//illegal as it causes a looop
	//zcstring strapp(str1);

	for (int i = 0; i < 100; i++) {
		stra = stra + zcstring(str1);
	}

	EXPECT_TRUE(stra.size() == 603);
	
	stra.append(strx);

	EXPECT_TRUE(stra.indexof(&strb) == 603);

}

TEST(zero_copy_string, index_of_concat_string_non_contiguous)
{
	ALIGNED_CONST_CHAR(8) str1[] = "lol";
	zcstring stra(str1);

	ALIGNED_CONST_CHAR(8) strx[] = "boy";
	ALIGNED_CONST_CHAR(8) strexp[] = "boyboy";
	auto strsol = zcstring(strexp);

	for (int i = 0; i < 50; i++) {
		stra.append(str1);
	}

	//illegal as it causes a looop
	//zcstring strapp(str1);

	stra = stra + zcstring(strx);
	stra = stra + zcstring(strx);

	for (int i = 0; i < 50; i++) {
		stra = stra + zcstring(str1);
	}

	EXPECT_TRUE(stra.size() == 309);
	EXPECT_TRUE(stra.indexof(&strsol) == 153);
}

TEST(zero_copy_string, index_of_concat_string_find) 
{
	ALIGNED_CONST_CHAR(8) str1[] = "lol";
	zcstring stra(str1);

	ALIGNED_CONST_CHAR(8) strx[] = "boy";
	ALIGNED_CONST_CHAR(8) strexp[] = "boyboy";
	auto strsol = zcstring(strexp);

	for (int i = 0; i < 50; i++) {
		stra.append(str1);
	}

	//illegal as it causes a looop
	//zcstring strapp(str1);

	stra = stra + zcstring(strx);
	stra = stra + zcstring(strx);

	for (int i = 0; i < 50; i++) {
		stra = stra + zcstring(str1);
	}

	EXPECT_TRUE(stra.size() == 309);
	EXPECT_TRUE(stra.find(strexp) == 153);
	EXPECT_TRUE(stra.find(strx, 154) == 156);
	EXPECT_TRUE(stra.find_last_of(strx) == 156);
	EXPECT_TRUE(stra.find_last_of((char*)strexp, 6, 0) == 307);

}

TEST(zero_copy_string, pop_front_test) 
{
	ALIGNED_CONST_CHAR(8) str1[] = "lol";
	zcstring stra(str1);

	for (int i = 0; i < 50; i++) {
		stra.append(str1);
	}
	stra.pop_front(10);
	EXPECT_TRUE(stra.offset == 1);

}

TEST(zero_copy_string, reserve_popfront_test)
{
	zcstring stra;
	for (int i = 0; i < 10; i++) {

		auto ptr = (char*)stra.reserve(8);
		EXPECT_TRUE(stra.reserved == 8);
		stra.append(ptr, 8);
		EXPECT_TRUE(stra.reserved == 0);
	}
	stra.pop_front(8);
	EXPECT_TRUE(stra.offset == 0);
}

TEST(zero_copy_string, reserve_dobble_add_regression)
{
	zcstring stra;

	auto ptr = (char*)stra.reserve(8);
	stra.append(ptr, 8);
	EXPECT_TRUE(stra.getmbuf()->head->next == nullptr);
	stra.pop_front(8);
	EXPECT_TRUE(stra.getmbuf()->head == nullptr);

}

TEST(zero_copy_string, indexof_end_test) 
{
	ALIGNED_CONST_CHAR(8) str1[] = "n";
	ALIGNED_CONST_CHAR(8) str2[] = "lolol";

	zcstring stra(str1);
	zcstring strb;
	zcstring strc(str2);

	EXPECT_TRUE(stra.indexof('m', 0) == UINT_MAX);
	EXPECT_TRUE(stra.indexof('n', 0) == 0);
	EXPECT_TRUE(strb.indexof('n', 0) == UINT_MAX);
	EXPECT_TRUE(strc.indexof('l', 2) == 2);

	EXPECT_TRUE(stra.find(str2, 0) == UINT_MAX);

}

TEST(zero_copy_string, pop_front_index_offset)
{
	ALIGNED_CONST_CHAR(8) str1[] = "lololol";

	zcstring stra(str1);
	stra.pop_front(1);
	EXPECT_TRUE(stra.indexof('l', 0) == 1);
}

TEST(zero_copy_string, reserve_test)
{
	ALIGNED_CONST_CHAR(8) str1[] = "lololol";

	zcstring stra(str1);
	auto ptr = stra.reserve(8);
	memcpy(ptr, str1, 8);
	stra.abort_reserve(ptr);
	ptr = stra.reserve(8);
	stra.append((char*)ptr, 8);
	EXPECT_TRUE(stra.size() == 15);
	EXPECT_TRUE(stra.getmbuf()->head->next->next == nullptr);
}

TEST(zero_copy_string, pop_back)
{
	ALIGNED_CONST_CHAR(8) str1[] = "lololol";

	zcstring stra(str1);
	auto ptr = stra.reserve(8);
	memcpy(ptr, str1, 8);
	stra.abort_reserve(ptr);
	ptr = stra.reserve(8);
	stra.append((char*)ptr, 8);
	stra.pop_back(1);
	stra.pop_back(5);
	EXPECT_TRUE(stra.size() == 9);
	stra.pop_back(1);
	EXPECT_TRUE(stra.size() == 8);
	EXPECT_TRUE(stra.getmbuf()->head->next != nullptr);
	EXPECT_TRUE(stra.getmbuf()->head->next->size == 1);
	stra.pop_back(1);
	EXPECT_TRUE(stra.getmbuf()->head->next == nullptr);
	stra.pop_back(1);
	stra.pop_back(1);
	stra.pop_back(1);
	EXPECT_TRUE(stra.size() == 4);
}
TEST(zero_copy_string, compare_test)
{
	ALIGNED_CONST_CHAR(8) str1[] = "Cache - Control: no - cache, no - store, max - age = 0, must - revalidate, no - store\r\n";
	ALIGNED_CONST_CHAR(8) moniker[] = "\r\n";

	zcstring stra(str1);
	zcstring strb(moniker);

	EXPECT_TRUE(!stra.compare(moniker));
	EXPECT_TRUE(strb.compare(moniker));
}
TEST(zero_copy_string, concat_empty)
{
	ALIGNED_CONST_CHAR(8) str1[] = "lololol";

	zcstring stra;
	zcstring strb(str1);

	stra.append(strb);

	EXPECT_TRUE(stra.size() == strb.size());
}

TEST(zero_copy_string, pop_back_print)
{
	ALIGNED_CONST_CHAR(8) str1[] = "lololol";
	ALIGNED_CONST_CHAR(8) str2[] = "lololol";

	zcstring stra(str1);
	zcstring strb(str2);
	stra.append(strb);
	EXPECT_TRUE(strb.getmbuf()->head->size == 7);

	stra.pop_back(1);
	stra.pop_back(1);
	EXPECT_TRUE(strb.getmbuf()->head->size == 5);
	strb.tostring();
}


static unsigned int x = 2;

void return_callback(zcstring ptr, zcstring *strb) {
	if (x < 3) {

		EXPECT_TRUE(ptr.getmbuf()->head->ref == x);
		x++;
		return_callback(ptr, strb);
		x--;
		EXPECT_TRUE(ptr.getmbuf()->head->ref == x);

	}
	else if(x == 3) {
		ptr.append(*strb);
	}
}


TEST(zero_copy_string, refcounting_on_stack)
{
	ALIGNED_CONST_CHAR(8) str1[] = "initial";
	ALIGNED_CONST_CHAR(8) extratest[] = "extratest";

	zcstring stra(str1);
	zcstring* strb = new zcstring(extratest);
	return_callback(stra, strb);

	EXPECT_TRUE(stra.getmbuf()->head->ref == 1);
	EXPECT_TRUE(strb->getmbuf()->head->ref == 1);
	delete strb;
}


TEST(zero_copy_string, refcount_append) {

	ALIGNED_CONST_CHAR(8) str1[] = "initial";
	ALIGNED_CONST_CHAR(8) extratest[] = "extratest";

	zcstring stra(str1);
	zcstring strb(extratest);

	stra.append(strb);
	EXPECT_TRUE(stra.getmbuf()->head->ref == 1);
	EXPECT_TRUE(strb.getmbuf()->head->ref == 2);
}



/*
	For some reason these two leak memory
*/
TEST(zero_copy_string, appending_offset_zcstring) {

	ALIGNED_CONST_CHAR(8) str1[] = "lololol";
	ALIGNED_CONST_CHAR(8) str2[] = "2o2o2o2";

	zcstring stra(str1);
	zcstring strb(str2);
	strb.pop_front(2);
	stra.append(strb);

	EXPECT_TRUE(stra.size() == 12);
	stra.tostring();

}

TEST(zero_copy_string, appending_offset_zcstring_multimbuf) {

	ALIGNED_CONST_CHAR(8) str1[] = "one";
	ALIGNED_CONST_CHAR(8) str2[] = "two";
	ALIGNED_CONST_CHAR(8) str3[] = "three";

	zcstring stra(str1);
	zcstring strb(str2);
	zcstring strc(str3);
	stra.append(strb);
	stra.pop_front(3);
	strc.append(stra);

	EXPECT_TRUE(strc.size() == 8);
	strc.tostring();
}




/*
TODO:

ASSINGNMENT FROM STRING AND CHAR TEST
****
ALIGNED_CONST_CHAR(8) chunked_label[] = "chunked";
zcstring chunked_label_str(chunked_label); ---Does not work without this line currently!!!
//only support content length
headers[transfer_encoding_field_name] = chunked_label_str;

****
delete at test

TODO: test for find first of and find last of, with offset*/
