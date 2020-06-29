#ifndef DIGGI_ASSERT_H
#define DIGGI_ASSERT_H

#ifdef __cplusplus
extern "C" {
#endif

extern void _enclave_assert(
	bool condition,
	const char *condition_text,
	const char *_caller_name,
	int line,
	const char * filename);

#include <stdio.h>
#ifdef __cplusplus
#include <cassert>
#else
#include <assert.h> 
#endif

#include "posix/stdio_stubs.h"
#if defined(DIGGI_ENCLAVE)

	#include "enclave_t.h"
#else
	#include <signal.h>
#endif

/*to avoid recursion*/
extern  int thr_id(void);

#ifdef RELEASE
#define DIGGI_ASSERT(condition) /* noop */
#else
#define DIGGI_ASSERT(condition) _enclave_assert(condition, #condition, __PRETTY_FUNCTION__, __LINE__, __FILE__)
#endif

#if defined(DIGGI_ENCLAVE)

#define DIGGI_DEBUG_BREAK()  DIGGI_ASSERT(false); /* Debug break only availible outside enclaves */

#else

#define DIGGI_DEBUG_BREAK()  raise(SIGTRAP);

#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif