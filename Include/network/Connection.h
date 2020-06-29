#ifndef CONNECTION_H
#define CONNECTION_H


#include "DiggiAssert.h"
#include "DiggiGlobal.h"
#include "threading/IThreadPool.h"
#include "network/IConnection.h"
#include <vector>
#include <algorithm>
#include "posix/stdio_stubs.h"
#include "Logging.h"
#include "misc.h"
#include "network/DiggiNetwork.h"
#include "telemetry.h"
#include "AsyncContext.h"
#include <errno.h>

using namespace std;

#define MAX_BUFFERED_LENGTH 128 * 1024




class Connection : public IConnection
{
	zcstring buff_;

	string dns_hostname;
	IThreadPool *tpool;
	async_cb_t server_loop_cb;
	string port;
	IConnectionPrimitives *con;
	ILog *log;
public:
	tcp_connection_context_t * ctx;
	Connection();
	Connection(Connection *ptr);
	Connection(IThreadPool *pool,
		async_cb_t cb, IConnectionPrimitives *prime, ILog * lg
	);
	~Connection();
	void initialize(tcp_connection_context_t *fd);
	int initialize(string hostname, string servername, string serverport);
	void InitializeServer(const char* serverip, const char *serverport);
	static void ServerLoop(void *ptr, int status);
	/*more performant with nonblocking sockets*/
	void read_line(async_cb_t cb, void* ptr);
	static void read_line_nb(void * ptr, int status);
	void read(size_t length, async_cb_t cb, void * context);
	static void read_nb(void * context, int status);
	void write(zcstring buf, async_cb_t, void *ptr);
	static void write_cb(void *ptr, int status);
	string get_hostname();
	string get_port();
	void close();
};
typedef AsyncContext<Connection*, zcstring, async_cb_t, void *> connection_write_ctx_t;
typedef AsyncContext<Connection*, zcstring, tcp_connection_context_t *, async_cb_t, void*, size_t> connection_read_ctx_t;
typedef AsyncContext<Connection*, zcstring, tcp_connection_context_t *, async_cb_t, void* > connection_read_line_ctx_t;

#endif