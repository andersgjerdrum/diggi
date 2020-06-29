#ifndef MISC_H
#define MISC_H

#include <stdlib.h>
#include <time.h>
#include <stddef.h>
#include <errno.h>
#include <string>
#include <string.h>
#include "posix/stdio_stubs.h"
#include "DiggiAssert.h"
#include "lockfree_rb_q.h"
#include "sgx_error.h"

#define MAX_DIGGI_MEM_SIZE  1024 * 1024 /*1MB*/
#define MAX_DIGGI_MEM_ITEMS (1024 * 1024 * 1024) / (MAX_DIGGI_MEM_SIZE)

//TODO (anders): separate threads into special file.

#define DIGGI_RUNTIME_THREAD_COUNT 1

#define DIGGI_MAX_RECIPIENTS 512

#define DIGGI_THREADMAX 20

/*
    Must be a multiple of block size
*/

#define DIGGI_ENCRYPTED_MEMORY_CHUNK_SIZE 1024 * 1024 * 16 
/*
	Max allowed reuse of threads nested by yield as new threads
	directly impacts stack size.
	Mythical max, discovered through testing
*/
#define MAX_THREADS_NESTED 8

/*
	If all 8 threads nests to max,
	this is expected pthread state size
*/
#define MAX_VIRTUAL_THREADS MAX_THREADS_NESTED * 8

static int thread_local __thr_id[[gnu::unused]] = -1;
static size_t thread_local __pthr_id[[gnu::unused]] = 0;


#ifdef __cplusplus
extern "C" {
#endif

#ifdef memset_s
#undef memset_s
#endif


/*
 * __memset_vp is a volatile pointer to a function.
 * It is initialised to point to memset, and should never be changed.
 */
static void * (* const volatile __memset_vp)(void *, int, size_t)
    = (memset);

int memset_s(void *s, size_t smax, int c, size_t n);

#ifndef DIGGI_ENCLAVE

int
consttime_memequal(uint8_t *b1, uint8_t *b2, size_t len);

#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

int thr_id();

void set_thr_id(int id);

void delete_memory_buffer(lf_buffer_t *buf, size_t pool_size);

lf_buffer_t *provision_memory_buffer(size_t threads, size_t pool_size, size_t object_size);

size_t roundUp_r(size_t numToRound, size_t multiple);




#if !defined(DIGGI_ENCLAVE) && !defined(TEST_DEBUG)
#ifdef __cplusplus
extern "C" {
#endif
sgx_status_t sgx_read_rand(unsigned char *randbf, size_t length_in_bytes);
#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
#endif