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
    IOThrougput test func
*/

void func_init(void *ctx, int status)
{
    DIGGI_ASSERT(ctx);
    auto a_cont = (DiggiAPI *)ctx;
    pthread_stubs_set_thread_manager(a_cont->GetThreadPool());
    auto encrypt = (a_cont->GetFuncConfig()["fileencryption"].value == "1");
    int syscall_interpose = (a_cont->GetFuncConfig()["syscall-interposition"].value == "1");

    if (syscall_interpose)
    {
        iostub_setcontext(a_cont, encrypt);
    }
    else
    {
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
void  *buffcb = nullptr;
size_t blocksize = 0;
int fd = 0;
size_t iterations = 0;
size_t active_iterations = 0;
size_t done_iterations = 0;
void async_write_contd(void * ptr, int status);
void async_read_contd(void *ptr, int status);

void async_write_contd(void * ptr, int status){
    
    auto ctx = (DiggiAPI*)ptr;
    if(active_iterations < iterations){
        active_iterations++;
        ctx->GetThreadPool()->Schedule(async_write_contd, ctx ,__PRETTY_FUNCTION__);
    }
    
    auto retval = write(fd, buffcb, blocksize);
    done_iterations++;
    if(done_iterations ==  iterations){
        telemetry_capture("write");
        active_iterations = 0;
        done_iterations = 0;
        lseek(fd, SEEK_SET, 0);
        ctx->GetLogObject()->Log(LRELEASE, "write done\n");

        ctx->GetThreadPool()->Schedule(async_read_contd, ctx ,__PRETTY_FUNCTION__);
    }
}

void async_read_contd(void *ptr, int status){
       
    auto ctx = (DiggiAPI*)ptr;
    if(active_iterations < iterations){
        active_iterations++;
        ctx->GetThreadPool()->Schedule(async_read_contd, ctx ,__PRETTY_FUNCTION__);
    }
   
    auto retval = read(fd, buffcb, blocksize);
    done_iterations++;
    if(done_iterations == iterations){
        telemetry_capture("read");
        ctx->GetLogObject()->Log(LRELEASE, "read done\n");

        active_iterations = 0;
        ctx->GetSignalHandler()->Stopfunc();
    }
}

void func_start(void *ctx, int status)
{
    DIGGI_ASSERT(ctx);
    auto a_cont = (DiggiAPI *)ctx;
    int oflags = O_RDWR | O_CREAT | O_TRUNC;
    mode_t mode = S_IRWXU;
    const char *path_n = "test.file.test";
    fd = open(path_n, oflags, mode);
    blocksize = (size_t)atoi(a_cont->GetFuncConfig()["Block-size"].value.tostring().c_str());
    iterations = (size_t)atoi(a_cont->GetFuncConfig()["Iterations"].value.tostring().c_str());
    size_t sync = (size_t)atoi(a_cont->GetFuncConfig()["Sync"].value.tostring().c_str());
    buffcb = (char *)calloc(1, blocksize);

    telemetry_capture("start");

    if (sync == 1)
    {
        for (size_t i = 0; i < iterations; i++)
        {
            auto retval = write(fd, buffcb, blocksize);
            DIGGI_ASSERT(retval == blocksize);
        }
        telemetry_capture("write");
        lseek(fd, SEEK_SET, 0);
        
        for (size_t i = 0; i < iterations; i++)
        {
            auto retval = read(fd, buffcb, blocksize);
            DIGGI_ASSERT(retval == blocksize);
        }
        telemetry_capture("read");
        
        a_cont->GetSignalHandler()->Stopfunc();
    }
    else
    {
        a_cont->GetThreadPool()->Schedule(async_write_contd, ctx ,__PRETTY_FUNCTION__);
    }
    #ifndef DIGGI_ENCLAVE
        set_syscall_interposition(0);

        telemetry_write();
    #endif


}

void func_stop(void *ctx, int status)
{

    DIGGI_ASSERT(ctx);
    auto a_cont = (DiggiAPI *)ctx;
    a_cont->GetLogObject()->Log(LRELEASE, "Stopping rw test func\n");
    pthread_stubs_unset_thread_manager();
    set_syscall_interposition(0);
}