/**
 * @file http.cpp
 * @author your name (you@domain.com)
 * @brief Implement a http client and http server, used for dataplane access in untrusted runtime to issue configuration updates to runtime.
 * This implementation technically only provide methods for compiling http messages and
 * uses connection.cpp as server implementation and message api.
 * http_request and http_response are derived classes of the base http_object class implementing common functionallity
 * @see connection.cpp
 * @version 0.1
 * @date 2020-02-02
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#include "network/HTTP.h"



/**
 * @brief compile a http request based on member info specified in http_response object.
 * Marshalled into a zcstring virtual contguous buffer, directly sendable via Connection::write()
 * Compiles body as chunked encoding, based on content of member "body"

 * @return zcstring virtual mutable contiguous buffer.
 */
zcstring http_response::compile_request()
{
	ALIGNED_CONST_CHAR(8) newline[] = "\r\n";
	ALIGNED_CONST_CHAR(8) space[] = " ";
	ALIGNED_CONST_CHAR(8) end_character[] = "0\r\n\r\n";
	ALIGNED_CONST_CHAR(8) header_separator[] = ": ";
	ALIGNED_CONST_CHAR(8) chunked_label[] = "chunked";

	zcstring chunked_label_str(chunked_label);

	//only support content length
	headers[transfer_encoding_field_name] = chunked_label_str;
	zcstring request;
	request.append(protocol_version);
	request.append(space);
	request.append(status_code);
	request.append(space);
	request.append(response_type);
	request.append(newline);
	map<std::string, zcstring>::iterator iterator;
	for (iterator = headers.begin(); iterator != headers.end(); iterator++)
	{
		request.append(iterator->first);
		request.append(header_separator);
		request.append(iterator->second);
		request.append(newline);
	}
	if (body.size() > 0)
	{
		request.append(newline);
		auto hex = Utils::utohex(body.size());
		auto ptr_hex = request.reserve(hex.length());
		memcpy(ptr_hex, hex.c_str(), hex.length());
		request.append((char *) ptr_hex, hex.length());
		request.append(newline);
		request.append(body);
	}

	request.append(newline);
	request.append(end_character);
	return request;
}


/**
 * @brief set response type field on http response
 * 
 * @param response virtual contiguous buffer containing response
 */
void http_response::set_response_type(zcstring response) {
	response_type = response;
}
/**
 * @brief set status code field in response
 * 
 * @param status virtual contiguous buffer containing encoded status
 */
void http_response::set_status_code(zcstring status) {
	status_code = status;
}


/**
 * @brief asynchronous callback invoked as response to a recieve request on a http_response.
 * called from process_headers() which is invoked via http_object->Get()
 * un-marshals a zcstring virtual contiguous buffer recieved from network placing results into http_response object members.
 * All operations are by-reference so http_response is populated with original buffer recieved from network.
 * extracts the very first line of http request: GET /www/http/test.index HTTP/1.1
 * forwards next execution step asynchronously to  which handles the rest of the header fields.
 * @param ptr connection_read_line_ctx_t
 * @param status unused parameter, error handling, future work.
 */
void http_response::extract_first_header_cb(void *ptr, int status)
{
	ALIGNED_CONST_CHAR(8) moniker[] = " ";
	auto ctx = (connection_read_line_ctx_t*)ptr;
	DIGGI_ASSERT(ctx->item1);
	DIGGI_ASSERT(ctx->item5);
	auto _this = (http_response*)ctx->item5;
	auto firstline = ctx->item2;
	auto string = firstline.tostring();
	int firstempty = firstline.indexof(' ', 0);
	DIGGI_ASSERT(firstempty);
	_this->log->Log(LDEBUG, "extract_first_header_cb\n");
	_this->protocol_version = firstline.substr(0, firstempty);
	auto secondempty = firstline.find(moniker, firstempty + 1);
	_this->status_code = firstline.substr(firstempty + 1, secondempty - firstempty);
	auto lastofindex = firstline.find_last_of(moniker) + 1;
	_this->response_type = firstline.substr(lastofindex, firstline.size() - lastofindex);
	ctx->item1->read_line(process_headers_cb, _this);
}
/**
 * @brief initialize reponse processing 
 * Invoke readline on first header, and submit a callback which processes the first header: extract_first_header_cb
 * Function accepts a callback completion event which is invoked once http_response is done processing.
 * 
 * @param cb completion callback
 * @param ptr Convenience context callee managed
 */
void http_response::process_headers(async_cb_t cb, void * ptr)
{
	recieve_done = cb;
	done_ctx = ptr;
	connection->read_line(extract_first_header_cb, this);
}

/**
 * @brief compile a http_request based on member info in object.
 * Marshaled into zcstring virtual congiguous buffer, diretly consumable by Connection::write, without any copy operations.
 * zcstring references directly from http_request object members, so ensure not to delete http_request object until it is sent.
 * Compiles body as chunked encoding, based on content of member "body"
 * @return zcstring 
 */
zcstring http_request::compile_request()
{
	ALIGNED_CONST_CHAR(8) newline[] = "\r\n";
	ALIGNED_CONST_CHAR(8) space[] = " ";
	ALIGNED_CONST_CHAR(8) end_character[] = "0\r\n\r\n";
	ALIGNED_CONST_CHAR(8) header_separator[] = ": ";
	ALIGNED_CONST_CHAR(8) chunked_label[] = "chunked";

	zcstring chunked_label_str(chunked_label);

	//only support content length
	headers[transfer_encoding_field_name] = chunked_label_str;
	zcstring request;
	request.append(request_type);
	request.append(space);
	request.append(url);
	request.append(space);
	request.append(protocol_version);
	request.append(newline);
	map<std::string, zcstring>::iterator iterator;
	for (iterator = headers.begin(); iterator != headers.end(); iterator++)
	{
		request.append(iterator->first);
		request.append(header_separator);
		request.append(iterator->second);
		request.append(newline);
	}
	if (body.size() > 0)
	{
		request.append(newline);
		auto hex = Utils::utohex(body.size());
		auto ptr_hex = request.reserve(hex.length());
		memcpy(ptr_hex, hex.c_str(), hex.length());
		request.append((char *)ptr_hex, hex.length());
		request.append(newline);
		request.append(body);
	}

	request.append(newline);
	request.append(end_character);
	return request;
}
/**
 * @brief set request type onto http_request member
 * 
 * @param r_type virtual contiguous buffer encoded request type
 */
void http_request::set_request_type(zcstring r_type)
{
	request_type = r_type;
}
/**
 * @brief set url onto http_request member
 * 
 * @param url_i virtual contiguous buffer encoded url.
 */
void http_request::set_url(zcstring url_i)
{
	url = url_i;
}

/**
 * @brief initialize request processing 
 * Invoke readline on first header, and submit a callback which processes the first header: extract_first_header_cb
 * Function accepts a callback completion event which is invoked once http_request is done processing.
 * 
 * @param cb completion callback
 * @param ptr Convenience context callee managed
 */
void http_request::process_headers(async_cb_t cb, void * ptr)
{
	recieve_done = cb;
	done_ctx = ptr;
	connection->read_line(extract_first_header_cb, this);
}

/**
 * @brief asynchronous callback to recieve http_request
 * called from process_headers() which is invoked via http_object->Get()
 * un-marshals a zcstring virtual contiguous buffer recieved from network placing results into http_request object members.
 * All operations are by-reference so http_request is populated with original buffer recieved from network.
 * extracts the very first line of http request: GET /www/http/test.index HTTP/1.1
 * forwards next execution step asynchronously to  which handles the rest of the header fields.
 * @param ptr connection_read_line_ctx_t
 * @param status unused parameter, error handling, future work.
 */
void http_request::extract_first_header_cb(void *ptr, int status)
{
	ALIGNED_CONST_CHAR(8) moniker[] = " ";
	auto ctx = (connection_read_line_ctx_t*)ptr;
	DIGGI_ASSERT(ctx->item1);
	DIGGI_ASSERT(ctx->item5);
	auto _this = (http_request*)ctx->item5;
	auto firstline = ctx->item2;
	int firstempty = firstline.indexof(' ', 0);
	DIGGI_ASSERT(firstempty);
	_this->log->Log(LDEBUG, "extract_first_header_cb\n");
	_this->request_type = firstline.substr(0, firstempty);
	_this->url = firstline.substr(firstempty + 1, firstline.find(moniker, firstempty + 1) - (firstempty + 1));
	auto lastofindex = firstline.find_last_of(moniker) + 1;
	_this->protocol_version = firstline.substr(lastofindex, firstline.size() - lastofindex);
	ctx->item1->read_line(process_headers_cb, _this);
}
/**
 * @brief process repeated header lines for http_request or http_response.
 * Reinvokes self through new read_line.
 * Un-marshal lines into http_object member map.
 * After all header lines are done, If the http_object contains a body, processing is delegated further to chunk processing body
 * @param ptr connection_read_line_ctx_t
 * @param status unused parameter, error handling, future work.
 */
void http_object::process_headers_cb(void *ptr, int status) {

	ALIGNED_CONST_CHAR(8) moniker[] = "\r\n";

	DIGGI_ASSERT(ptr);
	auto ctx = (connection_read_line_ctx_t*)ptr;
	DIGGI_ASSERT(ctx->item5);
	auto _this = (http_object*)ctx->item5;
	_this->log->Log(LDEBUG, "process_headers_cb\n");

	auto line = ctx->item2;

	if (line.compare(moniker))
	{
		if (_this->get_close_semantics() == NONE) {
			_this->recieve_done(_this->done_ctx, 1);
		}
		else {
			DIGGI_ASSERT(ctx->item5);

			ctx->item1->read_line(process_chunk_cb, _this);
		}
	}
	else {
		_this->put_header(line);
		ctx->item1->read_line(process_headers_cb, _this);
	}
}

/**
 * @brief callback invoked after last header line is read.
 * Processes chunked encoded body of http_object.
 * chunk encoded bodys consist of a hexadecimal header specifying length followed by a chunk of the body.
 * Repeatedly reads and invokes process_chunk_body_cb in tandem via completion callback until all chunks are done.
 * Done criteria set by an empty chunk followed by carrige return + newline: "0\r\n"
 * http_object all-up callback completion invoked after done processing.
 * The http server/client may then consume the content of the
 * @param ptr connection_read_line_ctx_t
 * @param status 
 */
void http_object::process_chunk_cb(void *ptr, int status)
{
	ALIGNED_CONST_CHAR(8) moniker[] = "0\r\n";
	DIGGI_ASSERT(ptr);
	auto ctx = (connection_read_line_ctx_t*)ptr;
	DIGGI_ASSERT(ctx->item5);
	auto _this = (http_object*)ctx->item5;
	_this->log->Log(LDEBUG, "process_chunk_cb\n");

	close_semantics opt = _this->get_close_semantics();
	auto line = ctx->item2;
	DIGGI_ASSERT(opt == CHUNKED);
	auto hex_val = line.substr(0, line.size() - 2);

	auto hex_chunk_header = strtoul(hex_val.tostring().c_str(), NULL, 16);
	if (hex_chunk_header)
	{
		_this->log->Log(LDEBUG, "reading chunk size: %lu \n", hex_chunk_header);
		ctx->item1->read(hex_chunk_header + 2, process_chunk_body_cb, _this);
	}
	else 
	{
		DIGGI_ASSERT(line.compare(moniker));
		/*done*/
		DIGGI_ASSERT(_this->recieve_done);
		_this->log->Log(LDEBUG, "recieve_done\n");
		_this->recieve_done(_this->done_ctx, 1);

	}
}

/**
 * @brief callback for processing chunk body, based on decoded hexadecimal size discovered by process_chunk_cb
 * reads and invokes process_chunk_cb in tandem via completion callback until all chunks are done.
 * places result in http_object body. managed as virtual contiguous buffer.
 * @param ptr connection_read_ctx_t
 * @param status 
 */
void http_object::process_chunk_body_cb(void *ptr, int status)
{
	DIGGI_ASSERT(ptr);
	auto ctx = (connection_read_ctx_t*)ptr;
	DIGGI_ASSERT(ctx->item5);
	DIGGI_ASSERT(ctx->item1);
	auto _this = (http_object*)ctx->item5;
	/*might require improvement later*/
	_this->body.append(ctx->item2);

	/*escaping newlines*/
	_this->body.pop_back(1);
	_this->body.pop_back(1);
	ctx->item1->read_line(process_chunk_cb, _this);
}

/**
 * @brief method to ad new header value encoded as (key:value)
 * header values are placed in map internal ton http_object, 
 * and marshalling will order headers in alphanumeric order.
 * @param line virtual contiguous buffer reference 
 */
void  http_object::put_header(zcstring line)
{
	int delimiter = line.indexof(':', 0);

	DIGGI_ASSERT(delimiter);
	zcstring intermediary = line.substr(delimiter + 1);
	auto key = line.substr(0, delimiter).tostring();
	auto value = intermediary.substr(0, intermediary.size() - 2);
	headers.insert(std::make_pair(key, value));
}

/**
 * @brief set unformated header value
 * @see http_object::put_header
 * @param name 
 * @param value 
 */
void http_object::set_header(string name, zcstring value)
{
	headers[name] = value;
}
/**
 * @brief set protocol version header info for http_object
 * This implementation support both http 1.1 and 1.0
 * @param vers virtual contiguous buffer containing encoded version.
 */
void http_object::set_protocol_version(zcstring vers) {
	protocol_version = vers;
}

/**
 * @brief retrieve close semantics for processing http_object body.
 * Depending on if this is chunked encoded, http 1.1 or others, different socket termination protocols exist.
 * @return close_semantics 
 */
close_semantics http_object::get_close_semantics()
{
	ALIGNED_CONST_CHAR(8) moniker[] = "1.1";

	if (headers.find(content_length_field_name) != headers.end())
	{
		return LENGTH;
	}
	else if (headers.find(transfer_encoding_field_name) != headers.end())
	{
		return CHUNKED;
	}
	else
	{
		DIGGI_ASSERT((protocol_version.find(moniker) != string::npos));
		return NONE;
	}
}
http_object::http_object(IConnection *conn, ILog *lg) :connection(conn), log(lg) { }

/**
 * @brief all up http_object send api call.
 * Marshals a http_object and sends it using Connection:send()
 * Completion callback called upon success.
 * 
 * @param cb completion callback
 * @param ctx callee managed convenience context object
 */
void http_object::Send(async_cb_t cb, void *ctx)
{
	auto res = compile_request();
	connection->write(res, cb, ctx);
}

/**
 * @brief request processing of an inbound http_object.
 * This call follows an initial incomming request callback by Connection
 * Callback invoked upon a completed read
 * @param cb completion callback
 * @param ptr calle managed convenience context object.
 */
void http_object::Get(async_cb_t cb, void * ptr)
{
	process_headers(cb, ptr);
}

/**
 * @brief set body value as virtual contiguous buffer. requires no copy operation
 * Source buffer must exist atleast until send is completed.
 * No requirement for pre encoding as chunked, this is handled internally.
 * @param value 
 */
void http_object::set_body(zcstring value)
{
	body = value;
}

/**
 * @brief get underlying conneciton object associated with the http_object
 * 
 * @return IConnection* 
 */
IConnection* http_object::GetConnection() {
	return connection;
}