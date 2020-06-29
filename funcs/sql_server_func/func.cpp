#include "runtime/func.h"
#include "messaging/IMessageManager.h"
#include "posix/pthread_stubs.h"
#include "posix/stdio_stubs.h"
#include <stdio.h>
#include <inttypes.h>
#include "runtime/DiggiAPI.h"
#include "sqlite3.h"
#include "posix/io_stubs.h"
#include "messaging/Util.h"
#include "telemetry.h"
#include "misc.h"
#include "posix/intercept.h"
#include "dbserver.h"

/*
	Database Server func
*/
/* 
	Accessed by single thread
*/
static std::string database_name = "boron.db";
void func_init(void *ctx, int status)
{
    DIGGI_ASSERT(ctx);
    auto a_cont = (DiggiAPI *)ctx;

    /* 
		Set encrypted to true 
	*/
    pthread_stubs_set_thread_manager(a_cont->GetThreadPool());
    auto encrypt = (a_cont->GetFuncConfig()["fileencryption"].value == "1");
    int syscall_interpose = (a_cont->GetFuncConfig()["syscall-interposition"].value == "1");

    iostub_setcontext(a_cont, encrypt);
    set_syscall_interposition(syscall_interpose);

    a_cont->GetLogObject()->Log(LRELEASE,
                                "Initializing Boron database func with encryption=%d and syscallnterpose=%d\n",
                                encrypt,
                                syscall_interpose);
}

void execute_database_thread(void *ctx, int status)
{
    auto a_cont = (DiggiAPI *)ctx;
    DIGGI_ASSERT(a_cont);
    a_cont->GetLogObject()->Log(LRELEASE, "Starting Boron database on thread %d with config %s\n", a_cont->GetThreadPool()->currentThreadId(), static_attested_diggi_configuration);

    auto inmem = (size_t)atoi(a_cont->GetFuncConfig()["in-memory"].value.tostring().c_str());
    if (inmem == 0)
    {
        new DBServer(a_cont, std::to_string(a_cont->GetId().raw) + "." + database_name);
    }
    else
    {
         sqlite3_config(SQLITE_CONFIG_URI,1);
        new DBServer(a_cont, "file::memory:?cache=shared");
    }
}

void func_start(void *ctx, int status)
{
    auto a_cont = (DiggiAPI *)ctx;

    auto thread_p = a_cont->GetThreadPool();
    for (unsigned i = 0; i < thread_p->physicalThreadCount(); i++)
    {
        thread_p->ScheduleOn(i, execute_database_thread, a_cont, __PRETTY_FUNCTION__);
    }
    DIGGI_ASSERT(ctx);
}

void func_stop(void *ctx, int status)
{
    DIGGI_ASSERT(ctx);
    auto a_cont = (DiggiAPI *)ctx;
    a_cont->GetLogObject()->Log(LRELEASE, "Stopping Boron database func\n");
    pthread_stubs_unset_thread_manager();
    set_syscall_interposition(0);
}