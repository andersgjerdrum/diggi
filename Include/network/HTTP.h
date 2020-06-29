#ifndef HTTP_H
#define HTTP_H

#include <string>
#include <map>
#include <cstdlib>
#include "messaging/Util.h"
#include "network/Connection.h"
#include "JSONParser.h"
#include "misc.h"
#include "Logging.h"

using namespace std;
#define READBUFFERSIZE 4096U


static const string transfer_encoding_field_name = "Transfer-Encoding";
static const string content_length_field_name = "Content-Length";

enum close_semantics
{
	CHUNKED,
	SOCKET,
	LENGTH,
	NONE
};

class http_object 
{
public:
	map<string, zcstring> headers;
	zcstring protocol_version;
	zcstring body;
	http_object(IConnection *conn, ILog *lg);
	virtual ~http_object(){
		headers.clear();
	}

	void set_header(string name, zcstring value);
	void set_protocol_version(zcstring vers);
	void Send(async_cb_t cb, void *ctx);
	void Get(async_cb_t cb, void * ptr);
	void set_body(zcstring value);
	IConnection* GetConnection();
	virtual zcstring compile_request() = 0;
	virtual void process_headers(async_cb_t cb, void * ptr) = 0;

protected:
	async_cb_t recieve_done;
	void *done_ctx;
	char buffer[READBUFFERSIZE];
	IConnection *connection;
	ILog *log;
	close_semantics get_close_semantics();
	static void process_headers_cb(void *ptr, int status);
	static void process_chunk_cb(void *ptr, int status);
	static void process_chunk_body_cb(void *ptr, int status);
	void put_header(zcstring line);

};


class http_response : public http_object
{
	friend http_object;
public:
	zcstring response_type;
	zcstring status_code;
	void set_response_type(zcstring response);
	void set_status_code(zcstring status);
	using http_object::http_object;
private:
	void process_headers(async_cb_t cb, void * ptr);

	zcstring compile_request();
	static void extract_first_header_cb(void *ptr, int status);

};

class http_request : public http_object
{
	friend http_object;
public:
	using http_object::http_object;
	zcstring request_type;
	zcstring url;
	void set_request_type(zcstring r_type);
	void set_url(zcstring url_i);
private:
	void process_headers(async_cb_t cb, void * ptr);
	static void extract_first_header_cb(void *ptr, int status);
	zcstring compile_request();
};
#endif