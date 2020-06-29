#ifndef ZCSTRING_H
#define ZCSTRING_H

#include <limits.h>
#include <cassert>
#include "mbuf.h"
#include <string>
#include <stdlib.h>
#include "DiggiAssert.h"

typedef enum zcstring_reference_type_t{
	COPY_TYPE_BEHAVIOUR,
}zcstring_reference_type_t;
class zcstring
{

#ifdef TEST_DEBUG
#include <gtest/gtest_prod.h>
FRIEND_TEST(zero_copy_string, pop_front_test);
FRIEND_TEST(zero_copy_string, reserve_popfront_test);
#endif
	mbuf_t *mbuf;
	size_t length;
	size_t offset;
	size_t reserved;
public:
	zcstring(char *head, size_t size);
	zcstring(const char *head);
	zcstring(const char *head, size_t size);
	zcstring(std::string str, zcstring_reference_type_t type);
	zcstring(const std::string &str);
	zcstring(std::string &str);
	zcstring(mbuf_t *head, size_t size);
	zcstring(mbuf_t *head, size_t size, size_t offset);
	zcstring(const zcstring &obj);
	zcstring();
	~zcstring();
	bool empty();

	size_t size() const;

	void * reserve(size_t size);
	void abort_reserve(void *ptr);

	//expensive if memory is fragmented
	char &operator[](unsigned index) const;

	unsigned find(const char *right);
	unsigned find(const char *right, unsigned index);
	unsigned find(const std::string &right, unsigned index);
	unsigned find_last_of(const std::string& str);
	unsigned find_last_of(const char *str);
	unsigned indexof(zcstring *right);

	//assume single contiguous memory region for right zcstring
	//supports non contiguous left zcstring
	unsigned indexof(zcstring *right, unsigned index, bool last = false /*=true*/);

	unsigned indexof(char c, unsigned index, bool last = false);

	unsigned find_last_of(const char *ar, size_t s, unsigned index);

	unsigned find_first_of(const char *ar, size_t s, unsigned index);

	bool contains(zcstring *zcs);

	bool compare(zcstring *zc);
	bool compare(const std::string &str);
	bool compare(std::string &str);
	bool compare(const char *zc, size_t size);
	bool compare(zcstring &c);
	bool compare(const char *zc);

	bool operator==(const char *zc);
	bool operator==(zcstring &zc);
	bool operator!=(const char *zc);

	zcstring& operator+(const zcstring& rhs) {
		DIGGI_ASSERT(rhs.offset == 0);
		mbuf_concat(this->mbuf, rhs.mbuf);
		this->length += rhs.length;
		return *this;
	}

	zcstring& operator+(const char *zc) {

		mbuf_append_tail(this->mbuf, zc, this->length + this->offset);
		this->length += strlen(zc);
		return *this;
	}

	void operator=(const zcstring& rhs) {
		if (this != &rhs) {
			mbuf_destroy(this->mbuf, this->offset, this->length);
			this->mbuf = nullptr;
			this->length = 0;
			this->offset = 0;
			this->mbuf = mbuf_new();
		}

		this->mbuf->head = rhs.mbuf->head;
		this->length = rhs.length;
		this->offset = rhs.offset;

		if (this != &rhs) {
			if (this->mbuf->head != nullptr) {
				mbuf_incref(this->mbuf);
			}
		}
	}

	void operator=(const char *zc) {
		assert(zc != nullptr);
		zcstring convert(zc);

		mbuf_destroy(this->mbuf, this->offset, this->length);
		this->mbuf = nullptr;
		this->length = 0;
		this->offset = 0;
		this->mbuf = mbuf_new();

		this->mbuf->head = convert.mbuf->head;
		this->length = convert.length;
		this->offset = convert.offset;
		if (this->mbuf->head != nullptr) {
			mbuf_incref(this->mbuf);
		}
	}
	
	void append(const char *zc);

	void append(void *ptr, size_t len, zcstring_reference_type_t type);

	void append(std::string &str, zcstring_reference_type_t type);

	void append(char *zc, size_t len);

	void append(const std::string &str);

	void append(zcstring& str);
    char* getptr(size_t expected_chunk_size);
	mbuf_t* getmbuf();
	size_t getoffset();

	/*
	* does not clean up tail pointers, returning string might be longer than expected.
	*/
	zcstring substr(unsigned index, size_t length);
	zcstring substr(unsigned index);

	void pop_back(size_t count);
	void pop_front(size_t count);

    void replace(char* str, size_t len);

	/*
	* must force copy
	*/
	char* copy();
    char* copy(char * str, size_t len);

	std::string tostring();
	void tostring(std::string *str);
	static const unsigned npos = UINT_MAX;
	//create copy to method
};


//#pragma GCC diagnostic push
//#pragma GCC diagnostic ignored "-Wunused-variable"
//static const zcstring * null_zcstring = new zcstring();
//#pragma GCC diagnostic pop


zcstring operator "" _M(const char* arr, size_t size);

#endif