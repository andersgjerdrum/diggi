#ifndef func_H
#define func_H
#include <stdlib.h>
#include "DiggiGlobal.h"
#ifndef UNTRUSTED_APP

#include "attested_config.h"
#endif

#define EXPORTED __attribute__((visibility("default")))


#ifdef __cplusplus
extern "C" {

#endif

void EXPORTED func_init(void * ctx, int status);

#ifndef DIGGI_ENCLAVE
void EXPORTED func_init_pre(void * ptr, int status) {
	auto ctx = (DiggiAPI*)ptr;
	SET_DIGGI_GLOBAL_CONTEXT(ctx);
	zcstring convert(static_attested_diggi_configuration);
	json_node conf(convert);
	
	ctx->SetFuncConfig(conf);
	func_init(ctx, status);
}
#endif 

void EXPORTED func_start(void *com, int status);

void EXPORTED func_stop(void *ctx, int status);

#ifdef __cplusplus
}
#endif
#endif