#include <gtest/gtest.h>
#include "storage/StorageManager.h"
#include "runtime/DiggiAPI.h"
#include "threading/ThreadPool.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "DiggiAssert.h"
#include "messaging/Util.h"


static const char* randstring = "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";

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

class MockMessageManager : public IMessageManager 
{
	int test_iter = 0;
	// Inherited via IMessageManager
	void Send(msg_t * msg, async_cb_t cb, void * cb_context)
	{
		if (test_iter == 0) {
			EXPECT_TRUE(msg->type == FILEIO_OPEN);
			auto ptr = msg->data;
			EXPECT_TRUE(0 == *(mode_t*)ptr);
			ptr += sizeof(mode_t);
			EXPECT_TRUE(O_RDONLY == *(int*)ptr);
			ptr += sizeof(int);
			ptr += sizeof(int);
			EXPECT_TRUE(memcmp(ptr, "unit_tests/testdata/json_array_elements.json", 45) == 0);
			auto resp = ALLOC(msg_async_response_t);
			resp->context = cb_context;
			test_iter++;
			resp->msg = ALLOC_P(msg_t, sizeof(int));
			auto accountingformessage = resp->msg;

			cb(resp, 1);
			free(accountingformessage);
			free(resp);
		}
		else if(test_iter == 1) {
			auto ptr = msg->data;
			EXPECT_TRUE(msg->type == FILEIO_READ);
			EXPECT_TRUE(33 == *(int*)ptr);
			ptr += sizeof(int);
			EXPECT_TRUE(ENCRYPTED_BLK_SIZE * 2 + 1 == *(size_t*)ptr);

			EXPECT_TRUE(NOSEEK == *(read_type_t*)(ptr + sizeof(size_t)));

			auto resp = ALLOC(msg_async_response_t);
			resp->context = cb_context;
			resp->msg = ALLOC_P(msg_t, ENCRYPTED_BLK_SIZE * 3 + sizeof(size_t) + sizeof(off_t));
			auto accountingformessage = resp->msg;
			size_t retval = ENCRYPTED_BLK_SIZE * 3;
			auto ptrsize = resp->msg->data + sizeof(size_t) + sizeof(off_t);
			((sgx_sealed_data_t*)ptrsize)->aes_data.payload_size = SPACE_PER_BLOCK;
			ptrsize += ENCRYPTED_BLK_SIZE;
			((sgx_sealed_data_t*)ptrsize)->aes_data.payload_size = SPACE_PER_BLOCK;
			ptrsize += ENCRYPTED_BLK_SIZE;
			((sgx_sealed_data_t*)ptrsize)->aes_data.payload_size = 1;
			memcpy(resp->msg->data, &retval, sizeof(size_t));
			off_t offset = 0;
			memcpy(resp->msg->data + sizeof(size_t), &offset, sizeof(off_t));
			test_iter++;
			cb(resp, 1);
			free(accountingformessage);
			free(resp);
		}
		else if (test_iter == 2) {
			auto ptr = msg->data;
			EXPECT_TRUE(msg->type == FILEIO_READ);
			EXPECT_TRUE(33 == *(int*)ptr);
			ptr += sizeof(int);
			EXPECT_TRUE((SPACE_PER_BLOCK * 2 - 15) == *(size_t*)ptr);
			EXPECT_TRUE(SEEKBACK == *(read_type_t*)(ptr + sizeof(size_t)));
			auto resp = ALLOC(msg_async_response_t);
			resp->context = cb_context;
			resp->msg = ALLOC_P(msg_t, ENCRYPTED_BLK_SIZE * 2 + sizeof(size_t) + sizeof(off_t));
			auto accountingformessage = resp->msg;
			size_t retval = ENCRYPTED_BLK_SIZE * 2;
			memcpy(resp->msg->data, &retval, sizeof(size_t));
			off_t offset = 15;
			memcpy(resp->msg->data + sizeof(size_t), &offset, sizeof(off_t));
			auto ptrsize = resp->msg->data + sizeof(size_t) + sizeof(off_t);
			((sgx_sealed_data_t*)ptrsize)->aes_data.payload_size = SPACE_PER_BLOCK;
			ptrsize += ENCRYPTED_BLK_SIZE;
			((sgx_sealed_data_t*)ptrsize)->aes_data.payload_size = SPACE_PER_BLOCK;
			test_iter++;
			cb(resp, 1);
			free(accountingformessage);
			free(resp);
		}
		else if (test_iter == 3) {
			auto ptr = msg->data;
			EXPECT_TRUE(msg->type == FILEIO_WRITE);
			EXPECT_TRUE(33 == *(int*)ptr);
			ptr += sizeof(int);
			ptr += sizeof(size_t);
			ptr += sizeof(int);
			EXPECT_TRUE(msg->size == (sizeof(msg_t) + sizeof(int) + sizeof(size_t) + sizeof(int) + ENCRYPTED_BLK_SIZE * 2));
			ptr = ptr + sizeof(sgx_sealed_data_t) + 15; /*offset*/
			EXPECT_TRUE(memcmp(ptr, randstring, SPACE_PER_BLOCK - 15) == 0);
			auto offsetptr = randstring + (SPACE_PER_BLOCK - 15);
			auto actual_offset_ptr = ptr + (SPACE_PER_BLOCK - 15) + sizeof(sgx_sealed_data_t);
			EXPECT_TRUE(memcmp(actual_offset_ptr, offsetptr, SPACE_PER_BLOCK) == 0);
			auto resp = ALLOC(msg_async_response_t);
			resp->context = cb_context;
			test_iter++;
			cb(resp, 1);
			free(resp);
		}
		else {
			auto ptr = msg->data;
			EXPECT_TRUE(msg->type == FILEIO_CLOSE);
			EXPECT_TRUE(33 == *(int*)ptr);
			test_iter++;
		}
		free(msg);
	}
	
	msg_t * allocateMessage(msg_t * msg, size_t payload_size)
	{
		auto msgret = ALLOC_P(msg_t, payload_size);
		msgret->size = sizeof(msg_t) + payload_size;
		return msgret;
	}
	void endAsync(msg_t *msg) {
		return;
	}
    msg_t * allocateMessage(aid_t destination, size_t payload_size, msg_convention_t async){
        auto msgret = ALLOC_P(msg_t, payload_size);
		msgret->size = sizeof(msg_t) + payload_size;
		return msgret;
    }

	msg_t * allocateMessage(std::string destination, size_t payload_size, msg_convention_t async)
	{
        EXPECT_TRUE(memcmp(destination.c_str(), "file_io_func", 14) == 0);
        return allocateMessage(aid_t(), payload_size, async);
		
	}
	void registerTypeCallback(async_cb_t cb, msg_type_t type, void * ctx)
	{
		DIGGI_ASSERT(false);
	}
	std::map<std::string, aid_t>  getfuncNames() {
		return std::map<std::string, aid_t>();
	}
};
static int test_callback_done = 0;
void write_cb(void *ptr, int status)
{
	DIGGI_ASSERT(ptr);
	auto resp = (msg_async_response_t *)ptr;
	auto a_context = (DiggiAPI*)resp->context;
	test_callback_done = 1;
	a_context->GetStorageManager()->async_close(33,false);
}
void read_source_cb(void *ptr, int status)
{
	DIGGI_ASSERT(ptr);
	auto resp = (msg_async_response_t *)ptr;
	auto a_context = (DiggiAPI*)resp->context;
	//free(resp->msg);
	char buf[8092];
	memcpy(buf, randstring, SPACE_PER_BLOCK * 2 - 15 );
														/*random write size*/
	a_context->GetStorageManager()->async_write(33, buf, SPACE_PER_BLOCK * 2 - 15, write_cb, a_context, true,false);
}
void test_cb(void * ctx, int status) 
{
	DIGGI_ASSERT(ctx);
	auto resp = (msg_async_response_t *)ctx;

	auto a_context = (DiggiAPI*)resp->context;
															/*random read size*/
	a_context->GetStorageManager()->async_read(33, nullptr, ENCRYPTED_BLK_SIZE * 2 + 1, read_source_cb, a_context, true,false);
}