#include "runtime/func.h"

#include "messaging/IMessageManager.h"
#include "DiggiAssert.h"
#include <inttypes.h>
#include "runtime/DiggiAPI.h"



/*
    Trusted Root func
*/


void func_init(void *ctx, int status)
{
    DIGGI_ASSERT(ctx);

}
void func_start(void *ctx, int status)
{
    DIGGI_ASSERT(ctx);

    //do nothing
}

/*Simple loopback test*/

void func_stop(void *ctx, int status)
{
    DIGGI_ASSERT(ctx);
    auto a_cont = (DiggiAPI *)ctx;
    a_cont->GetLogObject()->Log(LRELEASE, "Stopping Com func\n");
}