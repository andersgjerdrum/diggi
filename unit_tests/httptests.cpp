
#include <string.h>
#include "network/HTTP.h"
#include <gtest/gtest.h>
#include <fstream>
#include <streambuf>
#include "network/HTTP.h"
using namespace std;

 
#define SERVER_NAME "ec2-52-49-210-202.eu-west-1.compute.amazonaws.com"
#define SERVER_AUTHORIZATION_PORT "8082"
#define USERNAME "localguy"
#define PASSWORD "password"
#define CLIENT_IDENTIFICATION "example-client-id"
#define CLIENT_SECRET "example-client-secret"

ALIGNED_CONST_CHAR(8)  testResponse[] =
		"HTTP 200 OK\r\n"
		"Cache-Control: no-cache, no-store, max-age=0, must-revalidate, no-store\r\n"
		"Content-Type: application/json;charset=UTF-8\r\n"
		"Expires: 0\r\n"
		"Pragma: no-cache, no-cache\r\n"
		"Server: Jetty(8.1.15.v20140411)\r\n"
		"Transfer-Encoding: chunked\r\n"
		"X-Content-Type-Options: nosniff\r\n"
		"X-Frame-Options: DENY\r\n"
		"X-XSS-Protection:1; mode=block\r\n"
		"\r\n"
		"4\r\n"
		"Wiki\r\n"
		"5\r\n"
		"pedia\r\n"
		"E\r\n"
 		" in\r\n"
		"\r\n"
		"chunks.\r\n"
		"0\r\n"
		"\r\n";
ALIGNED_CONST_CHAR(8) expectedbody[] =
	"Wikipedia in\r\n"
	"\r\n"
	"chunks.";

ALIGNED_CONST_CHAR(8)  expectedrequest_1[] =
	"POST /dsu/oauth/token HTTP/1.1\r\n"
	"Accept: application/json\r\n"
	"Accept-Encoding: gzip, deflate\r\n"
	"Accept-Language: nb-NO,nb;q=0.8,no;q=0.6,nn;q=0.4,en-US;q=0.2,en;q=0.2\r\n"
	"Authorization: Basic bG9sOmxvbAA=\r\n"
	"Cache-Control: no-cache\r\n"
	"Connection: keep-alive\r\n"
	"Content-Type: multipart/form-data; boundary=----WebKitFormBoundarybbJWyCoSsWWmaKFF\r\n"
	"Host: lol:lol\r\n"
	"Origin: chrome-extension://fhbjgbiflinjbdggehcddcbncdddomop\r\n"
	"Postman-Token: 13f7d4c8-e091-38a7-9a82-77109f6108a0\r\n"
	"Transfer-Encoding: chunked\\r\n"
	"User-func: Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/51.0.2704.84 Safari/537.36\r\n"
	"\r\n"	
	"163\r\n"
	"------WebKitFormBoundarybbJWyCoSsWWmaKFF\r\n"
	"Content-Disposition: form-data; name=\"grant_type\"\r\n"
	"\r\n"
	"password\r\n"
	"------WebKitFormBoundarybbJWyCoSsWWmaKFF\r\n"
	"Content-Disposition: form-data; name=\"username\"\r\n"
	"\r\n"
	"lol\r\n"
	"------WebKitFormBoundarybbJWyCoSsWWmaKFF\r\n"
	"Content-Disposition: form-data; name=\"password\"\r\n"
	"\r\n"
	"lol\r\n"
	"------WebKitFormBoundarybbJWyCoSsWWmaKFF--\r\n"
	"\r\n"
	"0\r\n\r\n";

ALIGNED_CONST_CHAR(8)  expectedrequest_2[] = ""
	"GET / HTTP/1.1\r\n"
	"Host: 192.168.137.98 : 4443\r\n"
	"Connection : keep - alive\r\n"
	"Cache - Control : max - age = 0\r\n"
	"Upgrade - Insecure - Requests : 1\r\n"
	"User - func : Mozilla / 5.0 (Windows NT 10.0; Win64; x64) AppleWebKit / 537.36 (KHTML, like Gecko) Chrome / 58.0.3029.96 Safari / 537.36\r\n"
	"Accept : text / html, application / xhtml + xml, application / xml; q = 0.9, image / webp, */*;q=0.8\r\n"
	"Accept-Encoding: gzip, deflate, sdch\r\n"
	"Accept-Language: nb-NO,nb;q=0.8,no;q=0.6,nn;q=0.4,en-US;q=0.2,en;q=0.2\r\n"
	"\r\n";

ALIGNED_CONST_CHAR(8)  expectedrequest_3[] = ""
	"GET /lol/man HTTP/1.1\r\n"
	"Accept-Encoding: gzip, deflate, sdch\r\n"
	"Transfer-Encoding: chunked\r\n"
	"\r\n"
	"2b\r\n"
	"<header><title>Hello World</title></header>\r\n"
	"0\r\n\r\n";

ALIGNED_CONST_CHAR(8)  expectedresponse_3[] = ""
	"HTTP 200 OK\r\n"
	"Content-Type: application/json;charset=UTF-8\r\n"
	"Transfer-Encoding: chunked\r\n"
	"\r\n"
	"2b\r\n"
	"<header><title>Hello World</title></header>\r\n"
	"0\r\n\r\n";

class dummyConnection: public IConnection
{
	bool firsttime;
	bool success;
	int readlineindex = 0;
	string expectedrequest;
public:
	dummyConnection(string req):firsttime(true),success(false), expectedrequest(req)
	{

	}
	~dummyConnection()
	{

	}
	int initialize(string hostname, string servername, string serverport)
	{
		return 0;
	}
	void write(zcstring buf, async_cb_t cb, void *ptr)
	{
		success = (strcmp(expectedrequest.c_str(),(char*)buf.tostring().c_str()) == 0);
		EXPECT_TRUE(success);
		cb(ptr, 1);
	}
	void read_line(async_cb_t cb, void *ptr)
	{
		ALIGNED_CONST_CHAR(8) moniker[] = "\r\n";
		auto tresp = zcstring(expectedrequest);
		zcstring subject = tresp.substr(readlineindex);
		//printf("current head:%s\n", subject.tostring().c_str());
		int upcomming = subject.find(moniker);
		zcstring nextchunk = subject.substr(0, upcomming + 2);
		DIGGI_ASSERT(nextchunk.size() >= 2);
		readlineindex = readlineindex + nextchunk.size();
		zcstring ret;
		auto ptr_i = ret.reserve(nextchunk.size());
		//printf("lines read:%s \n", nextchunk.tostring().c_str());

		memcpy(ptr_i, nextchunk.tostring().c_str(), nextchunk.size());
		ret.append((char*)ptr_i, nextchunk.size());
		auto lol = new AsyncContext<IConnection*, zcstring, void *, async_cb_t, void* >(this, ret, nullptr, cb, ptr);
		cb(lol, 1);
		delete lol;
	}

	virtual	void read(size_t length, async_cb_t cb, void * context)
	{
		auto tresp = zcstring(expectedrequest);

		zcstring subject = tresp.substr(readlineindex);
		zcstring nextchunk = subject.substr(0,length);
		readlineindex = readlineindex + nextchunk.size();
		auto ret = new zcstring();
		auto ptr = ret->reserve(nextchunk.size());
		memcpy(ptr, nextchunk.tostring().c_str(), nextchunk.size());
		ret->append((char*)ptr, nextchunk.size());
		auto ctx = new AsyncContext<IConnection*, zcstring, void *, async_cb_t, void*, size_t >(this, *ret, 0, cb, context, length);
		cb(ctx, 1);
		delete ctx;
	}

	string get_hostname()
	{
		return "lol";
	}
	string get_port()
	{
		return "lol";
	}
	bool comparetestdata()
	{
		return success;
	}
	void close()
	{
	} 
};

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
static MockLog* mklog = nullptr;
void http_test_cb(void *ptr, int status)
{
	ALIGNED_CONST_CHAR(8) chnkd[] = " chunked";
	ALIGNED_CONST_CHAR(8) ok[] = "OK";
	ALIGNED_CONST_CHAR(8) http[] = "HTTP";
	ALIGNED_CONST_CHAR(8) twohundred[] = "200";

	auto re = (http_response*)ptr;

	EXPECT_TRUE(re->headers["Transfer-Encoding"].compare(chnkd));
	EXPECT_TRUE(re->status_code.compare(twohundred));
	EXPECT_TRUE(re->response_type.compare(ok));
	EXPECT_TRUE(re->body.compare(expectedbody));
	//printf("re->body refcount, %lu\n", re->body.getmbuf()->head->ref);
	EXPECT_TRUE(re->protocol_version.compare(http));
	delete re->GetConnection();
	delete re;
	delete mklog;

}

TEST(httptests, httpresponseIsCorrectlyparsed)
{
	auto con = new dummyConnection(testResponse);
	mklog = new MockLog();
	auto re = new http_response(con, mklog);
	re->Get(http_test_cb, re);
}

static bool httpresponsecompile_cb_called = false;

void http_response_send_test_cb(void *ptr, int status) {

	httpresponsecompile_cb_called = true;
}

TEST(httptests, httpresponsesend)
{
	ALIGNED_CONST_CHAR(8) http[] = "HTTP";
	zcstring http_str(http);

	ALIGNED_CONST_CHAR(8) type[] = "OK";
	zcstring type_str(type);

	ALIGNED_CONST_CHAR(8) hdr[] = "application/json;charset=UTF-8";
	zcstring hdr_str(hdr);

	ALIGNED_CONST_CHAR(8) body[] = "<header><title>Hello World</title></header>";
	zcstring body_str(body);

	ALIGNED_CONST_CHAR(8) twohundred[] = "200";
	zcstring twohundred_str(twohundred);

	auto con = new dummyConnection(expectedresponse_3);
	auto mklogg = new MockLog();
	auto re = new http_response(con, mklogg);

	re->set_response_type(type_str);
	re->set_status_code(twohundred_str);
	re->set_protocol_version(http_str);
	re->set_header("Content-Type", hdr_str);
	re->body = body_str;
	re->Send(http_response_send_test_cb, nullptr);
	EXPECT_TRUE(httpresponsecompile_cb_called);
	delete con;
	delete re;
	delete mklogg;
}


void http_req_test_cb(void *ptr, int status)
{
	ALIGNED_CONST_CHAR(8) url[] = "/";
	ALIGNED_CONST_CHAR(8) method[] = "GET";
	ALIGNED_CONST_CHAR(8) http[] = "HTTP/1.1";
	ALIGNED_CONST_CHAR(8) acpt[] = " gzip, deflate, sdch";

	auto re = (http_request*)ptr;

	EXPECT_TRUE(re->headers["Accept-Encoding"].compare(acpt));
	EXPECT_TRUE(re->url.compare(url));
	EXPECT_TRUE(re->request_type.compare(method));
	EXPECT_TRUE(re->body.size() == 0);
	EXPECT_TRUE(re->protocol_version.compare(http));
	delete re->GetConnection();
	delete re;
	delete mklog;

}

TEST(httptests, httprequestIsCorrectlyparsed)
{
	auto con = new dummyConnection(expectedrequest_2);
	mklog = new MockLog();
	auto re = new http_request(con, mklog);
	re->Get(http_req_test_cb, re);
}
static bool httprequestcompile_cb_called = false;
void httprequestcompile_cb(void *ptr, int status) 
{
	httprequestcompile_cb_called = true;
}

TEST(httptests, httprequestcompile)
{
	ALIGNED_CONST_CHAR(8) method[] = "GET";
	zcstring method_str(method);

	ALIGNED_CONST_CHAR(8) http[] = "HTTP/1.1";
	zcstring http_str(http);

	ALIGNED_CONST_CHAR(8) url[] = "/lol/man";
	zcstring url_str(url);
	
	ALIGNED_CONST_CHAR(8) acpt[] = "gzip, deflate, sdch";	
	zcstring acpt_str(acpt);

	ALIGNED_CONST_CHAR(8) body[] = "<header><title>Hello World</title></header>";
	zcstring body_str(body);

	auto con = new dummyConnection(expectedrequest_3);
	auto mklogg = new MockLog();
	auto re = new http_request(con, mklogg);
	re->set_request_type(method_str);
	re->set_protocol_version(http_str);
	re->set_url(url_str);
	re->set_body(body_str);
	re->set_header("Accept-Encoding", acpt_str);
	re->Send(httprequestcompile_cb, nullptr);
	EXPECT_TRUE(httprequestcompile_cb_called);
	delete con;
	delete re;
	delete mklogg;
}

