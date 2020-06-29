#include "runtime/func.h"
#include "messaging/IMessageManager.h"
#include "posix/stdio_stubs.h"
#include <stdio.h>
#include <inttypes.h>
#include "runtime/DiggiAPI.h"
#include "network/HTTP.h"
#include "network/Connection.h"
#include "posix/net_stubs.h"
/*HTTP server func*/

static Connection *server_connection = nullptr;
static SimpleTCP *api = nullptr;
static DiggiAPI *a_con = nullptr;
#define SERVER_PORT "7000"
static size_t MaxDataSize = 1024 * 10; /*1mb max size*/

static std::map<string, zcstring> page_cache_map;
typedef struct AsyncContext<int, DiggiAPI*, char*, size_t, char*, size_t, async_cb_t, http_request *> storage_context_t;

void read_source_cb(void *ptr, int status);
void open_source_cb(void * ptr, int status);

void func_respond_done_cb(void *ptr, int status)
{

	DIGGI_ASSERT(ptr);
	auto resp = (http_response *)ptr;
	a_con->GetLogObject()->Log("respond done: done sending HTTP response\n");

	auto con = resp->GetConnection();
	con->close();
	delete con;
	delete resp;

}

void func_respond_ok(IConnection *connection, zcstring body) {

	ALIGNED_CONST_CHAR(8) http[] = "HTTP/1.1";
	zcstring http_str(http);
	ALIGNED_CONST_CHAR(8) contenttype[] = "text/html";
	zcstring cont_string(contenttype);



	ALIGNED_CONST_CHAR(8) type[] = "OK";
	zcstring type_str(type);
	ALIGNED_CONST_CHAR(8) twohundred[] = "200";
	zcstring twohundred_str(twohundred);

	auto re = new http_response(connection, a_con->GetLogObject());

	re->set_response_type(type_str);
	re->set_status_code(twohundred_str);
	re->set_protocol_version(http_str);
	re->set_header("Content-Type", cont_string);
	re->body = body;
	re->Send(func_respond_done_cb, re);
}

void func_get_file(zcstring url, async_cb_t cb, http_request * context) {
	string path = url.tostring();
	if (path.compare("/") == 0) {
		path = "index.html"; //standard site start
	}
	
	auto storg = a_con->GetStorageManager();
	char *dat = (char*)malloc(MaxDataSize);
	auto resp_ctx = new storage_context_t(0, a_con, dat, MaxDataSize, dat, 0, cb, context);
	storg->async_open(path.c_str(), O_RDONLY, 0, open_source_cb, resp_ctx, false);

}
void func_respond_notfound(storage_context_t * ctx) {

	ALIGNED_CONST_CHAR(8) http[] = "HTTP/1.1";
	zcstring http_str(http);
	ALIGNED_CONST_CHAR(8) contenttype[] = "text/html";
	zcstring cont_string(contenttype);



	ALIGNED_CONST_CHAR(8) type[] = "Not Found";
	zcstring type_str(type);
	ALIGNED_CONST_CHAR(8) twohundred[] = "404";
	zcstring twohundred_str(twohundred);

	auto re = new http_response(ctx->item8->GetConnection(), a_con->GetLogObject());

	re->set_response_type(type_str);
	re->set_status_code(twohundred_str);
	re->set_protocol_version(http_str);
	re->set_header("Content-Type", cont_string);
	re->Send(func_respond_done_cb, re);

	delete ctx->item8;
	delete ctx;
	
}

void open_source_cb(void * ptr, int status)
{
	DIGGI_ASSERT(ptr);
	auto resp = (msg_async_response_t *)ptr;
	auto ctx = (storage_context_t*)resp->context;
	DIGGI_ASSERT(ctx);
	DIGGI_ASSERT(ctx->item2);

	auto a_context = ctx->item2;
	memcpy(&ctx->item1, resp->msg->data, sizeof(int));
	DIGGI_ASSERT(resp->msg->size == (sizeof(msg_t) + sizeof(int)));
	memcpy(&ctx->item1, resp->msg->data, sizeof(int));
	if (ctx->item1 == -1) {
		func_respond_notfound(ctx);
		return;
	}
	a_context->GetStorageManager()->async_read(ctx->item1, ctx->item3, ctx->item4, read_source_cb, ctx, false);
}

void read_source_cb(void *ptr, int status)
{
	DIGGI_ASSERT(ptr);
	auto resp = (msg_async_response_t *)ptr;
	auto ctx = (storage_context_t*)resp->context;
	DIGGI_ASSERT(ctx);
	DIGGI_ASSERT(ctx->item2);
	DIGGI_ASSERT(ctx->item3);
	auto a_cont = ctx->item2;

	size_t chunk_size;
	memcpy(&chunk_size, resp->msg->data, sizeof(size_t));

	DIGGI_ASSERT(chunk_size <= ctx->item4);

	ctx->item6 += chunk_size;

	/*Carrying an additional size for determining if buffer is empty*/
	auto ptr_data = resp->msg->data + sizeof(size_t) + sizeof(off_t);

	memcpy(ctx->item3, ptr_data, chunk_size);

	if (chunk_size < ctx->item4) {
		//done with transfer
		ctx->item2->GetLogObject()->Log(LDEBUG, "done with transfer, chunk_size=%lu ....\n", chunk_size);
		DIGGI_DEBUG_BREAK();

		a_cont->GetStorageManager()->async_close(ctx->item1, ctx->item7, ctx);
	}
	else {
		ctx->item4 -= chunk_size;
		ctx->item3 += chunk_size;
		/*May be more in file*/
		open_source_cb(ctx, 1);
	}
}

void func_cache_page(void *ptr, int status) {
	DIGGI_ASSERT(ptr);
	auto resp = (msg_async_response_t *)ptr;
	auto ctx = (storage_context_t*)resp->context;
	DIGGI_ASSERT(ctx);
	DIGGI_ASSERT(ctx->item5);
	zcstring dat(ctx->item5, ctx->item6);
	http_request *req = ctx->item8;
	page_cache_map[req->url.tostring()] = dat;
	func_respond_ok(req->GetConnection(), dat);
	delete req;
	delete ctx;
}


void func_handle_get(http_request *req) 
{
	a_con->GetLogObject()->Log("Request path %s\n" ,req->url.tostring().c_str());
	func_get_file(req->url, func_cache_page, req);
}

void func_handle_post(http_request *req) {

}

void func_http_incomming_cb(void *ptr, int status) {
	ALIGNED_CONST_CHAR(8) get_method[] = "GET";
	ALIGNED_CONST_CHAR(8) post_method[] = "POST";
	DIGGI_ASSERT(ptr);
	auto re = (http_request*)ptr;
	if (re->request_type.compare(get_method)) {
		func_handle_get(re);
	}
	else if (re->request_type.compare(post_method)) {
		func_handle_post(re);
	}
	else {
		DIGGI_ASSERT(false); /* Not supported http request type */
	}
	
	//func_respond_ok(re->GetConnection());
}


void func_incomming_http(void* ptr, int status)
{
	DIGGI_ASSERT(ptr);

	auto con = (Connection*)ptr;
	auto re = new http_request(con, a_con->GetLogObject());

	a_con->GetLogObject()->Log("Incomming http connection\n");

	re->Get(func_http_incomming_cb, re);
}

void func_init(void * ctx, int status)
{
	DIGGI_ASSERT(ctx);
	a_con = (DiggiAPI*)ctx;

	a_con->GetLogObject()->Log("func_init:http func\n");
	api = new SimpleTCP();
	server_connection = new Connection(a_con->GetThreadPool(),
		func_incomming_http,
		api, 
		a_con->GetLogObject());
	server_connection->InitializeServer(NULL, SERVER_PORT);
	//do nothing
}

void func_start(void *ctx, int status)
{
	DIGGI_ASSERT(ctx);
	a_con->GetLogObject()->Log("func_start:http func\n");
}

void func_stop(void *ctx, int status)
{
	DIGGI_ASSERT(ctx);
	a_con->GetLogObject()->Log("func_stop:http func\n");
	server_connection->close();
	delete server_connection;
	delete api;
}