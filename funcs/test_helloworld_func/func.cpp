#include "runtime/func.h"
#include "messaging/IMessageManager.h"
#include "posix/stdio_stubs.h"
#include <stdio.h>
#include <inttypes.h>
#include "runtime/DiggiAPI.h"
/*Simple func*/

static int call_order = 1;

void func_init(void * ctx, int status)
{
	DIGGI_ASSERT(ctx);
	DIGGI_ASSERT(call_order == 1);
	call_order++;
	auto a_cont = (DiggiAPI*)ctx;

	a_cont->GetLogObject()->Log("func_init:hello_world\n");
	//do nothing
}

void func_start(void *ctx, int status)
{
	DIGGI_ASSERT(ctx);
	DIGGI_ASSERT(call_order == 2);
	call_order++;
	auto a_cont = (DiggiAPI*)ctx;

	a_cont->GetLogObject()->Log("func_start:hello_world\n");
}

void func_stop(void *ctx, int status) 
{
	DIGGI_ASSERT(ctx);
	DIGGI_ASSERT(call_order == 3);
	auto a_cont = (DiggiAPI*)ctx;
	a_cont->GetLogObject()->Log("func_stop:hello_world\n");
}