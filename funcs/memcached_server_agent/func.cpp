
#include "runtime/func.h"
#include "memcached/memcached.h"
#include "posix/libeventStubs.h"
#include "posix/pipeStubs.h"
#include "posix/libeventStubsPrivate.h"
#include "network/NetworkHandle.h"
#include "handle/pipehandle.h"
#include "posix/net_stubs.h"
#include "runtime/DiggiAPI.h"
#include "clogger.h"

static NetworkHandle *nhandle = nullptr;
static PipeHandle *phandle = nullptr;

void func_init(void * ctx, int status)
{
	auto acont = (DiggiAPI*)ctx;

	nhandle = new NetworkHandle (acont);
	phandle = new PipeHandle (acont);

	libevent_set_context (ctx);
	netstubs_set_context (ctx);
	netstubs_set_handle (nhandle);
	pipe_set_handle (phandle);
	libevent_add_handle (HANDLE_TYPE_SOCKET, (IHandle*)nhandle);
	libevent_add_handle (HANDLE_TYPE_PIPE, (IHandle*)phandle);

	acont->GetLogObject()->Log("Init\n");
}

void func_start(void *ctx, int status)
{
	auto acont = (DiggiAPI*)ctx;

	acont->GetLogObject()->Log("Memcached Init start\n");

    char *error = NULL;

    // Set the log instance for the c interface logger
    set_log_instance (acont->GetLogObject());

	// Non-blocking
	auto ret = memcached_run(1, NULL, &error);
    if (ret < 0)
    {
	   acont->GetLogObject()->Log("Memcached return an error\n");
    }

    if (error != NULL)
    {
	    acont->GetLogObject()->Log(error);
        DIGGI_ASSERT(false);
    }

	acont->GetLogObject()->Log("Memcached Init done\n");

}

void func_stop(void *ctx, int status)
{
	if (nhandle)
	{
		delete nhandle;
	}
	if (phandle)
	{
		delete phandle;
	}
}
