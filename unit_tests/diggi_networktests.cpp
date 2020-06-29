#include <gtest/gtest.h>
#include "network/Connection.h"
#include "network/DiggiNetwork.h"
#include "threading/ThreadPool.h"
#include "threading/IThreadPool.h"
#include "Logging.h"
#include "network/HTTP.h"
class MockLog : public ILog
{
public:
	MockLog() {

	}
	std::string GetfuncName() {
		return "testfunc";
	}
	void SetFuncId(aid_t aid, std::string name = "func") {
	}
	void SetLogLevel(LogLevel lvl) {}
	void Log(LogLevel lvl, const char *fmt, ...) {}
	void Log(const char *fmt, ...) {}
};
static volatile int diggi_networktests_client_cb_called = 0;
static volatile int diggi_networktests_server_cb_called = 0;
void respond_ok_done_cb(void *ptr, int status)
{

	DIGGI_ASSERT(ptr);
	auto resp = (http_response *)ptr;

	auto con = resp->GetConnection();
	con->close();
	auto coner = (Connection*)con;
	delete coner;
	delete resp;
	diggi_networktests_server_cb_called = 1;
	__sync_synchronize();
}

void respond_ok(IConnection *connection) {

	ALIGNED_CONST_CHAR(8) http[] = "HTTP/1.1";
	zcstring http_str(http);
	ALIGNED_CONST_CHAR(8) contenttype[] = "text/html";
	zcstring cont_string(contenttype);

	ALIGNED_CONST_CHAR(8) body[] = "<t1>Message Recieved</t1>";
	zcstring body_str(body);

	ALIGNED_CONST_CHAR(8) type[] = "OK";
	zcstring type_str(type);
	ALIGNED_CONST_CHAR(8) twohundred[] = "200";
	zcstring twohundred_str(twohundred);

	auto re = new http_response(connection, new MockLog());

	re->set_response_type(type_str);
	re->set_status_code(twohundred_str);
	re->set_protocol_version(http_str);
	re->set_header("Content-Type", cont_string);
	re->body = body_str;
	re->Send(respond_ok_done_cb, re);
}

void http_incomming_cb(void *ptr, int status) {

	DIGGI_ASSERT(ptr);
	auto re = (http_response*)ptr;

	respond_ok(re->GetConnection());
	delete re;
}
void incomming_http(void* ptr, int status)
{
	DIGGI_ASSERT(ptr);

	auto con = (Connection*)ptr;
	auto re = new http_request(con, new MockLog());

	re->Get(http_incomming_cb, re);

}
void incomming_http2(void* ptr, int status)
{
	DIGGI_ASSERT(false);
}
void diggi_networktests_client_cb1(void *ptr, int status)
{
	DIGGI_ASSERT(ptr);
	auto re = (http_response*)ptr;
	EXPECT_TRUE(strcmp("<t1>Message Recieved</t1>", re->body.tostring().c_str()) == 0);
	diggi_networktests_client_cb_called = 1;
	__sync_synchronize();
}
void diggi_networktests_client_cb(void *ptr, int status)
{
	auto con = (Connection*)ptr;
	auto re = new http_response(con, new MockLog());
	re->Get(diggi_networktests_client_cb1, re);
}

TEST(diggi_networktests, simpleserver)
{
	set_syscall_interposition(0);

	auto sslapi = new SimpleTCP();
	auto tpool = new ThreadPool(1);
	auto tpool2 = new ThreadPool(1);

	auto con = new Connection(tpool, incomming_http, sslapi, new MockLog());
	con->InitializeServer(NULL, "7000");
	auto con2 = new Connection(tpool2, incomming_http2, sslapi, new MockLog());

	EXPECT_TRUE(con2->initialize("", "127.0.0.1", "7000") == 0);
	auto re = new http_request(con2, new MockLog());

	ALIGNED_CONST_CHAR(8) method[] = "GET";
	zcstring method_str(method);

	ALIGNED_CONST_CHAR(8) http[] = "HTTP/1.1";
	zcstring http_str(http);

	ALIGNED_CONST_CHAR(8) url[] = "/";
	zcstring url_str(url);

	re->set_request_type(method_str);
	re->set_protocol_version(http_str);
	re->set_url(url_str);
	re->Send(diggi_networktests_client_cb, con2);

	/*waiting for exchange to finish*/
	while (!diggi_networktests_client_cb_called);
	while (!diggi_networktests_server_cb_called);

	
	usleep(100);

	tpool2->Stop();
	tpool->Stop();
	con2->close();
	delete con2;
	con->close();
	delete con;
}