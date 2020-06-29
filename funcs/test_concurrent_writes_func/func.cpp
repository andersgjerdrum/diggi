#include "runtime/func.h"
#include "messaging/IMessageManager.h"
#include "posix/pthread_stubs.h"
#include "posix/stdio_stubs.h"
#include <stdio.h>
#include <inttypes.h>
#include "runtime/DiggiAPI.h"
#include "posix/io_stubs.h"
#include "messaging/Util.h"
#include "telemetry.h"
#include "misc.h"
#include "posix/intercept.h"
#include "telemetry.h"

/*
    Concurrent Read Test func
*/

void func_init(void *ctx, int status)
{
    DIGGI_ASSERT(ctx);
    auto a_cont = (DiggiAPI *)ctx;
    pthread_stubs_set_thread_manager(a_cont->GetThreadPool());
    auto encrypt = (a_cont->GetFuncConfig()["fileencryption"].value == "1");
    int syscall_interpose = (a_cont->GetFuncConfig()["syscall-interposition"].value == "1");

    if(syscall_interpose)
    {
        iostub_setcontext(a_cont, encrypt);
    }
    else{
        iostub_setcontext(nullptr, encrypt);
    }
    
    set_syscall_interposition(syscall_interpose);

    a_cont->GetLogObject()->Log(LRELEASE,
                                "Initializing rw test func with encryption=%d and syscallnterpose=%d\n",
                                encrypt,
                                syscall_interpose);
#ifndef DIGGI_ENCLAVE
    telemetry_init();
#endif
}

void func_start(void *ctx, int status)
{
    DIGGI_ASSERT(ctx);
    auto a_cont = (DiggiAPI *)ctx;
    int oflags = O_RDWR | O_CREAT | O_TRUNC;
    mode_t mode = S_IRWXU;
    const char *path_n = "test.file.test";
    int fd = open(path_n, oflags, mode);
    size_t blocksize = (size_t)atoi(a_cont->GetFuncConfig()["Block-size"].value.tostring().c_str());
    size_t iterations = (size_t)atoi(a_cont->GetFuncConfig()["Iterations"].value.tostring().c_str());
    auto buf = (char *)calloc(1, blocksize);
#ifndef DIGGI_ENCLAVE
    telemetry_start();
#endif
    for (size_t i = 0; i < iterations; i++)
    {
        auto retval = write(fd, buf, blocksize);
        DIGGI_ASSERT(retval == blocksize);
        telemetry_capture("write");
    }

    DIGGI_ASSERT(0 == lseek(fd, SEEK_SET, 0));
    for (size_t i = 0; i < iterations; i++)
    {
        auto retval = read(fd, buf, blocksize);
        DIGGI_ASSERT(retval == blocksize);
        telemetry_capture("read");
    }
#ifndef DIGGI_ENCLAVE
    telemetry_write();
#endif
    a_cont->GetSignalHandler()->Stopfunc();
}

void func_stop(void *ctx, int status)
{

    DIGGI_ASSERT(ctx);
    auto a_cont = (DiggiAPI *)ctx;
    a_cont->GetLogObject()->Log(LRELEASE, "Stopping rw test func\n");
    pthread_stubs_unset_thread_manager();
    set_syscall_interposition(0);
}