/**
 * @file connection.cpp
 * @author Anders Gjerdrum (anders.t.gjerdrum@uit.no)
 * @brief implementation of asynchronous connection handler, used by the untrusted runtime to handle cross-host message.
 * May be used by http.cpp to implement http server.
 * @see http.cpp
 * Please refer to main.cpp and diggiruntime.cpp for usage examples.
 * @see main.cpp
 * @see http.cpp
 * @see diggiruntime.cpp
 * @version 0.1
 * @date 2020-02-02
 * @copyright Copyright (c) 2020
 * 
 */
#include "network/Connection.h"
using namespace std;

/**
 * @brief Construct a new Connection:: Connection object
 * Does not open or start processing loop. 
 */
Connection::Connection() {}

/**
 * @brief Construct a new Connection:: Connection object
 * Initalizes members for connection.
 * Does not open or start processing loop.
 * Creates request connection connection from listen connection, forking listening tcp socket and request socket.
 * @param ptr listen connection
 */
Connection::Connection(Connection *ptr) : tpool(ptr->tpool), server_loop_cb(ptr->server_loop_cb), con(ptr->con), log(ptr->log) {}

/**
 * @brief Construct a new Connection:: Connection object
 * Initalizes members for connection.
 * Does not open or start processing loop.
 * Creation of new listen connection, and upon a request invokes the cb to handle setup of request connection.
 * @param pool threadpool api object for asynchronous thread execution
 * @param cb request event callback
 * @param prime connection api used to implement network com, supports TLS/TCP atm.
 * @param lg logging api object.
 */
Connection::Connection(IThreadPool *pool,
                       async_cb_t cb,
                       IConnectionPrimitives *prime, ILog *lg) : tpool(pool), server_loop_cb(cb), con(prime), log(lg) {}

Connection::~Connection()
{
}

#ifdef DIGGI_ENCLAVE
/**
 * @brief sockets may return error info, parsed by in-enclave function instead of asking untrusted runtime.
 * only for use in trusted runtime
 * @param msg 
 */
void perror(const char *msg)
{
    GET_DIGGI_GLOBAL_CONTEXT()->GetLogObject()->Log(LRELEASE, "ERROR: %s\n");
}
#endif
/**
 * @brief Initialzes listening connection server, binding to argument ip:port pair.
 * creates asynchronous loop which attempts a nonblocking poll of socket.
 * accepts NULL as input if not binding to particular interface, then connection API searches all availible.
 * @param serverip 
 * @param serverport 
 */
void Connection::InitializeServer(const char *serverip, const char *serverport)
{
    ctx = new tcp_connection_context_t();
    con->init__(ctx);

    int ret = con->bind__(ctx, serverip, serverport);

    if (ret != 0)
    {
        DIGGI_ASSERT(false);
    }

    initialize(ctx);
    tpool->Schedule(Connection::ServerLoop, this, __PRETTY_FUNCTION__);
}
/**
 * @brief Internal loop used for periodic asynchronous listen socket poll.
 * Agnostic to underlying socket implementation, however, requires non blocking operation.
 * Attempts to accept incomming connection, and invokes callback server_loop_cb to create new request connection object.
 * @param ptr this connection object
 * @param status unused parameter, error handling, future work
 */
void Connection::ServerLoop(void *ptr, int status)
{

    auto _this = (Connection *)ptr;
    if (_this == nullptr)
    {
        return;
    }
    if (_this->ctx == nullptr)
    {
        return;
    }
    if (_this->con == nullptr)
    {
        return;
    }

    auto client_fd = new tcp_connection_context_t();
    //_this->con->init(&client_fd);
    auto ret = _this->con->accept__(
        _this->ctx,
        client_fd,
        NULL,
        0,
        NULL);
    if (ret == DIGGI_NETWORK_WOULD_BLOCK || (ret == -74))
    { /*Fix for unit tests*/
        delete  client_fd;
        _this->tpool->Schedule(Connection::ServerLoop, _this, __PRETTY_FUNCTION__);
        return;
    }
    else if (ret != 0)
    {
        DIGGI_ASSERT(false);
    }

    DIGGI_TRACE(_this->log, LDEBUG, "Accepting tcp connection\n");
    auto cli = new Connection(_this);
    cli->initialize(client_fd);
    _this->tpool->Schedule(cli->server_loop_cb, cli, __PRETTY_FUNCTION__);

    DIGGI_TRACE(_this->log, LDEBUG, "Scheduling server loop\n");
    _this->tpool->Schedule(Connection::ServerLoop, _this, __PRETTY_FUNCTION__);
}
/**
 * @brief internal method to intitialize a connection context object.
 * Context object may be simple file descriptor(for non-encrypted session) or TLS context for
 * transport layer security.
 * Inititalizes socket to non-blocking, if not availible this will cause an assertion failure.
 * 
 * @param fd context object
 */
void Connection::initialize(tcp_connection_context_t *fd)
{
    ctx = fd;
    auto ret = con->set_nonblock__(ctx);
    DIGGI_ASSERT(ret == 0);
    DIGGI_TRACE(log, LDEBUG, "Set connection to non blocking\n");
}
/**
 * @brief Initialize client connection to access server connection at given servername:port
 * Sets connection to non-blocking
 * @param hostname unused, revenant of earlier implementation
 * @param servername server dns or ip address 
 * @param serverport port number of server
 * @return int success flag, 0 if successfull connection
 */
int Connection::initialize(string hostname, string servername, string serverport)
{
    auto client_fd = new tcp_connection_context_t();
    //con->init(client_fd);

    dns_hostname = servername;
    port = serverport;
    if (con->connect__(client_fd, servername.c_str(), serverport.c_str()) != 0)
    {
        DIGGI_TRACE(log, LDEBUG, "Failed to connect to tcp server\n");
        return -1;
    }
    DIGGI_TRACE(log, LDEBUG, "Connected to tcp server\n");

    /*initialize happens after connect*/
    initialize(client_fd);
    return 0;
}
/**
 * @brief asynchronous call for requesting a line from the connection.
 * Lines are terminated with newline (UNIX) character.
 * Callback is invoked upon completion of a full line-read.
 * Pro-Tip: callback may continue reading in a loop to read multiple lines, invoking itself until a complete-condition is met.
 * @param cb completion callback
 * @param ptr calle managed context object delivered to completion callback.
 */
void Connection::read_line(async_cb_t cb, void *ptr)
{
    auto zcstringobj = zcstring();
    auto ctxt = new connection_read_line_ctx_t( this, zcstringobj, ctx, cb, ptr);
    return read_line_nb(ctxt, 1);
}

/**
 * @brief readline internal call which is repeatedly invoked until a newline terminal character is recieved.
 * Stores intermediary results in a connection managed buffer. 
 * The connection managed buffer is directly delivered to the caller of read line through the zcstring mutable buffer structure.
 * This avoids multiple copy operations on objects from sockets, while providing stream read functionality,
 * exemplified by this implementation of readline.
 * Will cause assertion failure if connection is closed unexpectedly prior to completed read (perhaps a little harsh..)
 * Schedules itself onto the threadpool for repeated reads.
 * @param ptr 
 * @param status 
 */
void Connection::read_line_nb(void *ptr, int status)
{
    DIGGI_ASSERT(ptr);
    auto ctx = (connection_read_line_ctx_t *)ptr;
    auto _this = ctx->item1;
    auto fd = ctx->item3;
    DIGGI_ASSERT(_this);
    DIGGI_ASSERT(fd);
    DIGGI_ASSERT(fd->fd > 0);

    auto pos = _this->buff_.indexof('\n', 0);
    if (pos == UINT_MAX)
    {
        auto buf = _this->buff_.reserve(MAX_BUFFERED_LENGTH);
        int bytes_read = _this->con->recv__(fd, (unsigned char *)buf, MAX_BUFFERED_LENGTH);

        if (bytes_read == DIGGI_NETWORK_WOULD_BLOCK)
        {
            _this->tpool->Schedule(
                read_line_nb,
                ctx, __PRETTY_FUNCTION__);
            _this->buff_.abort_reserve(buf);
            return;
        }
        if (bytes_read < 0)
        {
            DIGGI_ASSERT(false);
        }

        if (bytes_read == 0)
        {
            _this->buff_.abort_reserve(buf);
        }
        else
        {
            _this->buff_.append((char *)buf, bytes_read);
        }

        pos = _this->buff_.indexof('\n', 0);
    }

    if (pos != UINT_MAX)
    {
        ctx->item2 = _this->buff_.substr(0, pos + 1);
        _this->buff_.pop_front(pos + 1);
        ctx->item4(ctx, 1);

        /*
			callback fulfilled so delete the context and buffer
		*/
        delete ctx;
        return;
    }

    _this->tpool->Schedule(
        read_line_nb,
        ctx, __PRETTY_FUNCTION__);
}

/**
 * @brief api to invoke asynchronous read of particular length.
 * Callback invoked once read is complete
 * @param length length of read in bytes
 * @param cb completion callback
 * @param context calle managed context object, for convenience.
 */
void Connection::read(size_t length, async_cb_t cb, void *context)
{
    //telemetry_capture("read start");

    auto rctx = new connection_read_ctx_t( this, zcstring(), ctx, cb, context, length);
    read_nb(rctx, 1);
}

/**
 * @brief internal  asynchonous callback for reads of particular lenght
 * Reschedules self onto pool until read is completed. 
 * Will cause assertion failure if connection is closed unexpectedly prior to completed read (perhaps a little harsh..)
 * Uses internal mutable zcstring buffer to preserve reads before delivery, directly returned to callback once done, without any copy operations.
 * zcstring implements a virtual contiguous buffer over partial chunks read from socket.
 * For use in trusted runtime, buffer must be managed to prevent high-memory footprint (currently statically defined)
 * @param ptr connection_read_ctx_t
 * @param status unused parameter, error handling, future work
 */
void Connection::read_nb(void *ptr, int status)
{
    DIGGI_ASSERT(ptr);
    auto context = (connection_read_ctx_t *)ptr;
    DIGGI_ASSERT(context->item1);
    DIGGI_ASSERT(context->item3);
    DIGGI_ASSERT(context->item4);
    auto _this = context->item1;
    auto fd = context->item3;

    DIGGI_ASSERT(fd->fd > 0);

    size_t getfrombuf = min(_this->buff_.size(), context->item6);
    //_this->DIGGI_TRACE(log, "Getfrombuf: %lu \n", getfrombuf);

    if (getfrombuf > 0)
    {
        /*appending to buf causes problems, because offset of buff_ is not 0*/
        auto data_from_buf = _this->buff_.substr(0, getfrombuf);
        context->item2.append(data_from_buf);
        _this->buff_.pop_front(getfrombuf);
    }

    size_t from_socket = context->item6 - getfrombuf;
    from_socket = min(from_socket, (size_t)MAX_BUFFERED_LENGTH);

    // _this->log->Log(LRELEASE, "we need from socket: %lu \n", from_socket);

    DIGGI_ASSERT(from_socket >= 0);
    DIGGI_ASSERT(from_socket <= MAX_BUFFERED_LENGTH);

    if (from_socket > 0)
    {
        auto bufr = (char *)context->item2.reserve(MAX_BUFFERED_LENGTH);
        auto read = _this->con->recv__(fd, (unsigned char *)bufr, from_socket);
        //_this->DIGGI_TRACE(log, "recieved from socket: %d \n", read);

        if (read == DIGGI_NETWORK_WOULD_BLOCK)
        {
            _this->tpool->Schedule(
                read_nb,
                context, __PRETTY_FUNCTION__);
            context->item2.abort_reserve(bufr);
            return;
        }
        if (read < 0)
        {
            perror("socket error");
            DIGGI_ASSERT(false);
        }

        if (read == 0)
        {
            context->item2.abort_reserve(bufr);
        }
        else
        {
            context->item6 -= read;
            context->item2.append(bufr, read);
        }
    }
    else
    {
        context->item6 -= getfrombuf;
        DIGGI_ASSERT(context->item6 == 0);
        //telemetry_capture("read stop");

        context->item4(context, 1);

        delete context;
        /*delete context*/
        return;
    }

    /*
	reschedule task
	*/
    _this->tpool->Schedule(
        read_nb,
        context, __PRETTY_FUNCTION__);
}
/**
 * @brief Write api call for performing an asynchronous write. 
 * Accepts a zcstring mutable buffer for writing.
 * On completion, the input callback is invoked.
 * @param buf 
 * @param cb 
 * @param ptr 
 */
void Connection::write(zcstring buf, async_cb_t cb, void *ptr)
{
    auto wctx = new connection_write_ctx_t( this, buf, cb, ptr);
    Connection::write_cb(wctx, 1);
}

/**
 * @brief internal write operation.
 * Reschedules itself until completed, at which point the input completion callback is called.
 * Will cause assertion failure if connection is closed unexpectedly prior to completed write (perhaps a little harsh..)
 * @param ptr 
 * @param status 
 */
void Connection::write_cb(void *ptr, int status)
{
    auto ctx = (connection_write_ctx_t *)ptr;
    auto _this = ctx->item1;
    DIGGI_ASSERT(_this != nullptr);
    DIGGI_ASSERT(_this->tpool != nullptr);

    DIGGI_ASSERT(ctx->item2.getmbuf() != nullptr);
    DIGGI_ASSERT(ctx->item2.getmbuf()->head != nullptr);
    DIGGI_ASSERT(ctx->item2.getmbuf()->head->data != nullptr);
    DIGGI_ASSERT(ctx->item2.getmbuf()->head->size != 0);

    auto data = ctx->item2.getmbuf()->head->data + ctx->item2.getoffset();

    int ret;

    ret = _this->con->send__(_this->ctx, (unsigned char *)data, ctx->item2.getmbuf()->head->size - ctx->item2.getoffset());
    if (ret < 0)
    {
        /*
            EAGAIN implies that the output buffer is full and a send would block
            We should retry the operation instead
        */
        if (errno != EAGAIN)
        {
            perror("socket error");
            DIGGI_ASSERT(false);
        }
        else
        {
            _this->tpool->Schedule(
                write_cb,
                ctx, __PRETTY_FUNCTION__);
            DIGGI_TRACE(GET_DIGGI_GLOBAL_CONTEXT()->GetLogObject(), LDEBUG, "Connection::write_cb: -> Retry send\n");
            return;
        }
    }

    DIGGI_ASSERT((size_t)ret <= ctx->item2.getmbuf()->head->size - ctx->item2.getoffset());

    ctx->item2.pop_front((size_t)ret);
    if (ctx->item2.size() == 0)
    {
        if (ctx->item3 != nullptr)
        {
            ctx->item3(ctx->item4, 1);
        }
        delete ctx;
        return;
    }
    _this->tpool->Schedule(
        write_cb,
        ctx, __PRETTY_FUNCTION__);
}

/**
 * @brief get hostname of connection (may be dns or ip address)
 * 
 * @return string 
 */
string Connection::get_hostname()
{
    return dns_hostname;
}

/**
 * @brief get port address of connection
 * 
 * @return string 
 */
string Connection::get_port()
{
    return port;
}

/**
 * @brief close connection, free socket, and stop serverloop if serverside connection.
 * 
 */
void Connection::close()
{
    DIGGI_TRACE(log, LDEBUG, "Closing connection with hostname: %s and port: %s \n", dns_hostname.c_str(), port.c_str());
    con->free__(ctx);
    delete ctx;
    ctx = nullptr;
}
