#include <gtest/gtest.h>
#include "storage/DBClient.h"
#include "dbserver.h"
#include "messaging/IMessageManager.h"
#include "threading/ThreadPool.h"
#include "Logging.h"

static std::string db_client_resultstring = R"(
[[0,"Anders",9000,"123 party avenue",123.0],
[1,"Anders",9000,"123 party avenue",123.0],
[2,"Anders",9000,"123 party avenue",123.0],
[3,"Anders",9000,"123 party avenue",123.0],
[4,"Anders",9000,"123 party avenue",123.0],
[5,"Anders",9000,"123 party avenue",123.0],
[6,"Anders",9000,"123 party avenue",123.0],
[7,"Anders",9000,"123 party avenue",123.0],
[8,"Anders",9000,"123 party avenue",123.0],
[9,"Anders",9000,"123 party avenue",123.0]]
)";


class DBCMockMessageManager: public IMessageManager {
	// Inherited via IMessageManager
	msg_t *lastsent;
	ThreadPool *thrdpool;

public:
	msg_async_response_t * rsp;
	DBCMockMessageManager(ThreadPool *thrdpool): thrdpool(thrdpool){
		lastsent = nullptr;
	}
	msg_t *getlastsentmessage() {
		return lastsent;
	}
	void Send(msg_t *msg, async_cb_t cb, void *cb_context)
	{
        this->rsp = new msg_async_response_t();
		rsp->context = cb_context;
		rsp->msg = COPY(msg_t, msg, msg->size);
		thrdpool->Schedule(cb, rsp, __PRETTY_FUNCTION__);
		
		lastsent = msg;
	}
	void endAsync(msg_t *msg) {
		return;
	}
    msg_t *allocateMessage(std::string destination, size_t payload_size, msg_convention_t async, msg_delivery_t delivery){
        return allocateMessage(aid_t(), payload_size, async, delivery);
    }

    
	msg_t * allocateMessage(msg_t *msg, size_t payload_size)
	{
		return nullptr;
	}
	msg_t * allocateMessage(aid_t destination, size_t payload_size, msg_convention_t async, msg_delivery_t delivery)
	{
		return (msg_t*)ALLOC_P(msg_t, payload_size);
	}
	void registerTypeCallback(async_cb_t cb, msg_type_t type, void *ctx)
	{
	}
	std::map<std::string, aid_t> getfuncNames()
	{
		return std::map<std::string, aid_t>();
	}
};

class MockLogger : public ILog {
	// Inherited via ILog
	void SetFuncId(aid_t aid, std::string name = "func") {

	}
	std::string GetfuncName() {
		return "testfunc";
	}
	virtual void SetLogLevel(LogLevel lvl) override
	{
	}
	virtual void Log(LogLevel lvl, const char * fmt, ...) override
	{
	}
	virtual void Log(const char * fmt, ...) override
	{
	}
};
volatile int testdone = 0;
void transaction_core(void * ptr, int status) {

	DIGGI_ASSERT(ptr);
	const char* dat = "DeployTable";
	auto dbcli = (DBClient*)ptr;
	dbcli->connect("sql_server_func");
	dbcli->beginTransaction();
	dbcli->execute("SELECT * FROM %s", dat);

	testdone = 1;
	__sync_synchronize();

}
volatile int commitdone = 0;
void transaction_commit(void *ptr, int status) {

	DIGGI_ASSERT(ptr);
	auto dbcli = (DBClient*)ptr;
	dbcli->commit();
	commitdone = 1;
	__sync_synchronize();

}
TEST(dbclienttests, simpletransaction) 
{
	std::string expected_q = "BEGIN TRANSACTION;SELECT * FROM DeployTable";
	std::string expected_commit = "COMMIT;";
	auto log = new MockLogger();
	auto thrdpool = new ThreadPool(1);
	auto mm = new DBCMockMessageManager(thrdpool);
	auto dbcli = new DBClient(mm, thrdpool, CLEARTEXT);
	thrdpool->Schedule(transaction_core, dbcli, __PRETTY_FUNCTION__);

	while (!testdone) {
		__sync_synchronize();
	}

	auto msg = mm->getlastsentmessage();
	free(mm->rsp->msg);
	delete mm->rsp;
	std::string actual_q = std::string((char*)msg->data, msg->size - sizeof(msg_t) - 1);
	//printf("actual_q:\n\"%s\"\nexpected_q:\n\"%s\"\n", actual_q.c_str(), expected_q.c_str());
	//printf("sizeactual:%lu, sizeexpected:%lu\n", actual_q.size(), expected_q.size());
	EXPECT_TRUE(actual_q.compare(expected_q) == 0);
	free(msg);
	thrdpool->Schedule(transaction_commit, dbcli, __PRETTY_FUNCTION__);

	while (!commitdone){
		__sync_synchronize();
	}
	msg = mm->getlastsentmessage();
	free(mm->rsp->msg);
	delete mm->rsp;
	std::string actual_commit = std::string((char*)msg->data, msg->size - sizeof(msg_t));
	EXPECT_TRUE(actual_commit.compare(expected_commit) == 0);
	free(msg);
	thrdpool->Stop();
	delete log;
	delete thrdpool;
	delete dbcli;
	delete mm;
}
TEST(dbclienttests, fetchone) {
	auto dbcli = new DBClient(nullptr, nullptr, CLEARTEXT);
	auto rsp = new msg_async_response_t();
	rsp->context = dbcli;
	rsp->msg = (msg_t*)ALLOC_P(msg_t, db_client_resultstring.size());
	memcpy(rsp->msg->data, db_client_resultstring.c_str(), db_client_resultstring.size());
	rsp->msg->size = db_client_resultstring.size() + sizeof(msg_t);
	DBClient::set_callback_msg(rsp, 1);
	auto result = dbcli->fetchone();
	EXPECT_TRUE(result.getInt(0) == 0);
	EXPECT_TRUE(result.getString(1).compare("Anders") == 0);
	EXPECT_TRUE(result.getUnsigned(2) == 9000);
	EXPECT_TRUE(result.getString(3).compare("123 party avenue") == 0);
	EXPECT_TRUE(result.getDouble(4) == 123.0);
	delete rsp;
}

TEST(dbclienttests, fetchall) {
	auto dbcli = new DBClient(nullptr, nullptr, CLEARTEXT);
	auto rsp = new msg_async_response_t();
	rsp->context = dbcli;
	rsp->msg = (msg_t*)ALLOC_P(msg_t, db_client_resultstring.size());
	memcpy(rsp->msg->data, db_client_resultstring.c_str(), db_client_resultstring.size());
	rsp->msg->size = db_client_resultstring.size() + sizeof(msg_t);
	DBClient::set_callback_msg(rsp, 1);
	auto results = dbcli->fetchall();
	EXPECT_TRUE(results.size() == 10);
	int i = 0;
	for (auto result : results) {
		EXPECT_TRUE(result.getInt(0) == i);
		EXPECT_TRUE(result.getString(1).compare("Anders") == 0);
		EXPECT_TRUE(result.getUnsigned(2) == 9000);
		EXPECT_TRUE(result.getString(3).compare("123 party avenue") == 0);
		EXPECT_TRUE(result.getDouble(4) == 123.0);
		i++;
	}
	delete rsp;
}