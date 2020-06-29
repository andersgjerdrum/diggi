#include "runtime/func.h"
#include "messaging/IMessageManager.h"
#include "DiggiAssert.h"
#include <inttypes.h>
#include "runtime/DiggiAPI.h"
#include "misc.h"
#include "storage/StorageServer.h"
/*File IO func*/


/*do not yield here, another thread entirely*/
void func_init(void *ctx, int status)
{
	DIGGI_ASSERT(ctx);
	auto a_cont = (DiggiAPI*)ctx;
	DIGGI_TRACE(a_cont->GetLogObject(), LRELEASE, "Initializing File IO func\n");
}
void execute_storage_thread(void *ctx, int status) {
	auto strg_server = (StorageServer*)ctx;
	DIGGI_ASSERT(strg_server);
	strg_server->initializeServer();
}
void func_start(void *ctx, int status)
{
	DIGGI_ASSERT(ctx);
	auto a_cont = (DiggiAPI*)ctx;
	DIGGI_TRACE(a_cont->GetLogObject(), LRELEASE, "Starting File IO func with configuration = %s\n", static_attested_diggi_configuration);
	auto thread_p = a_cont->GetThreadPool();
	for (unsigned i = 0; i < thread_p->physicalThreadCount(); i++) {
		auto strg_server = new StorageServer(a_cont);
		thread_p->ScheduleOn(i, execute_storage_thread, strg_server, __PRETTY_FUNCTION__);
	}
}

void func_stop(void *ctx, int status) {
	DIGGI_ASSERT(ctx);
	auto con = (DiggiAPI*)ctx;
	DIGGI_TRACE(con->GetLogObject(),LRELEASE,"Stopping File IO func func\n");

}
